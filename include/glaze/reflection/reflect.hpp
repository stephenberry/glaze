// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(__has_builtin)
#if __has_builtin(__builtin_dump_struct)

#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/reflection/to_tuple.hpp"

namespace glz
{
   namespace detail
   {
      template <class T, std::size_t N>
      struct static_vector
      {
         constexpr auto push_back(const T& elem)
         {
            if (index < N) {
               elems[index++] = elem;
            }
         }
         constexpr auto& operator[](auto i) const { return elems[i]; }

         T elems[N]{};
         size_t index{};
      };

      constexpr auto to_names(auto& out, auto, auto... args)
      {
         if constexpr (sizeof...(args) > 1) {
            auto t = tuplet::tuple{args...};
            if (sv{get<0>(t)} == "  ") {
               out.push_back(get<2>(t));
            }
         }
      }

      template <class T>
      constexpr auto member_names()
      {
         constexpr auto N = count_members<T>();
         static_vector<std::string_view, N> v{};
         T t;
         __builtin_dump_struct(&t, to_names, v);
         return v;
      }

      template <reflectable T>
      struct to_json<T>
      {
         template <auto Options>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            static constexpr auto members = member_names<T>();
            auto t = to_tuple(value);
            using V = decltype(t);
            static constexpr auto N = std::tuple_size_v<V>;
            static_assert(count_members<T>() == N);

            if constexpr (!Options.opening_handled) {
               dump<'{'>(b, ix);
               if constexpr (Options.prettify) {
                  ctx.indentation_level += Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
            }

            bool first = true;
            for_each<N>([&](auto I) {
               static constexpr auto Opts = opening_and_closing_handled_off<ws_handled_off<Options>()>();
               decltype(auto) item = std::get<I>(t);
               using val_t = std::decay_t<decltype(item)>;

               if (skip_member<Opts>(item)) {
                  return;
               }

               // skip file_include
               if constexpr (std::is_same_v<val_t, includer<V>>) {
                  return;
               }
               else if constexpr (std::is_same_v<val_t, hidden> || std::same_as<val_t, skip>) {
                  return;
               }
               else {
                  if (first) {
                     first = false;
                  }
                  else {
                     // Null members may be skipped so we cant just write it out for all but the last member unless
                     // trailing commas are allowed
                     write_entry_separator<Opts>(ctx, b, ix);
                  }

                  const auto key = members[I];

                  write<json>::op<Opts>(key, ctx, b, ix);
                  dump<':'>(b, ix);
                  if constexpr (Opts.prettify) {
                     dump<' '>(b, ix);
                  }

                  write<json>::op<Opts>(item, ctx, b, ix);
               }
            });

            if constexpr (!Options.closing_handled) {
               if constexpr (Options.prettify) {
                  ctx.indentation_level -= Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
               dump<'}'>(b, ix);
            }
         }
      };

      template <class Tuple>
      using reflection_value_tuple_variant_t = typename tuple_ptr_variant<Tuple>::type;

      template <class T, bool use_hash_comparison, size_t... I>
      constexpr auto make_reflection_map_impl(std::index_sequence<I...>)
      {
         using V = decltype(to_tuple(std::declval<T>()));
         constexpr auto n = std::tuple_size_v<V>;
         constexpr auto members = member_names<T>();
         static_assert(count_members<T>() == n);

         using value_t = reflection_value_tuple_variant_t<V>;

         auto naive_or_normal_hash = [&] {
            if constexpr (n <= 20) {
               return glz::detail::naive_map<value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{members[I], std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
            else {
               return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{members[I], std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
         };

         return naive_or_normal_hash();
      }

      template <class T, bool use_hash_comparison = false>
      constexpr auto make_reflection_map()
         requires(!glaze_t<T> && !array_t<T> && std::is_aggregate_v<std::remove_cvref_t<T>>)
      {
         using V = decltype(to_tuple(std::declval<T>()));

         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<V>>{};
         return make_reflection_map_impl<std::decay_t<T>, use_hash_comparison>(indices);
      }

      template <reflectable T>
      struct from_json<T>
      {
         template <auto Options, string_literal tag = "">
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            parse_object_opening<Options>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();

            static constexpr auto num_members = std::tuple_size_v<decltype(to_tuple(std::declval<T>()))>;
            // Only used if error_on_missing_keys = true
            [[maybe_unused]] bit_array<num_members> fields{};

            static constinit auto frozen_map = make_reflection_map<T, Opts.use_hash_comparison>();
            // we have to populate the pointers in the reflection map from the structured binding
            auto t = to_tuple(value);
            for_each<num_members>([&](auto I) {
               std::get<std::add_pointer_t<std::decay_t<decltype(std::get<I>(t))>>>(
                  std::get<I>(frozen_map.items).second) = &std::get<I>(t);
            });

            bool first = true;
            while (true) {
               if (*it == '}') [[unlikely]] {
                  ++it;
                  if constexpr (Opts.error_on_missing_keys) {
                     constexpr auto req_fields = required_fields<T, Opts>();
                     if ((req_fields & fields) != req_fields) {
                        ctx.error = error_code::missing_key;
                     }
                  }
                  return;
               }
               else if (first) [[unlikely]]
                  first = false;
               else [[likely]] {
                  match<','>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  skip_ws<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               if constexpr (num_members == 0) {
                  // parsing to an empty object, but at this point the JSON presents keys
                  const sv key = parse_object_key<T, ws_handled<Opts>(), tag>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  if constexpr (Opts.error_on_unknown_keys) {
                     if constexpr (tag.sv().empty()) {
                        std::advance(it, -int64_t(key.size()));
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                     else if (key != tag.sv()) {
                        std::advance(it, -int64_t(key.size()));
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                  }
                  else {
                     parse_object_entry_sep<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     skip_value<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
               }
               else {
                  const sv key = parse_object_key<T, ws_handled<Opts>(), tag>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  // Because parse_object_key does not necessarily return a valid JSON key, the logic for handling
                  // whitespace and the colon must run after checking if the key exists
                  if (const auto& member_it = frozen_map.find(key); member_it != frozen_map.end()) [[likely]] {
                     parse_object_entry_sep<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     if constexpr (Opts.error_on_missing_keys) {
                        // TODO: Kludge/hack. Should work but could easily cause memory issues with small changes.
                        // At the very least if we are going to do this add a get_index method to the maps and call
                        // that
                        auto index = member_it - frozen_map.begin();
                        fields[index] = true;
                     }
                     std::visit(
                        [&](auto&& member_ptr) {
                           read<json>::op<ws_handled<Opts>()>(get_member(value, member_ptr), ctx, it, end);
                        },
                        member_it->second);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
                  else [[unlikely]] {
                     if constexpr (Opts.error_on_unknown_keys) {
                        if constexpr (tag.sv().empty()) {
                           std::advance(it, -int64_t(key.size()));
                           ctx.error = error_code::unknown_key;
                           return;
                        }
                        else if (key != tag.sv()) {
                           std::advance(it, -int64_t(key.size()));
                           ctx.error = error_code::unknown_key;
                           return;
                        }
                        else {
                           // We duplicate this code to avoid generating unreachable code
                           parse_object_entry_sep<Opts>(ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;

                           skip_value<Opts>(ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                        }
                     }
                     else {
                        // We duplicate this code to avoid generating unreachable code
                        parse_object_entry_sep<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        skip_value<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
               }
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }
      };
   }
}

#endif
#endif
