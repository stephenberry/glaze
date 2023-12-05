// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/reflection/get_name.hpp"
#include "glaze/reflection/to_tuple.hpp"

namespace glz
{
   namespace detail
   {
      template <std::size_t N>
      class fixed_string final
      {
        public:
         constexpr explicit(true) fixed_string(const auto... cs) : data{cs...} {}
         constexpr explicit(false) fixed_string(const char (&str)[N + 1]) { std::copy_n(str, N + 1, std::data(data)); }
         [[nodiscard]] constexpr auto operator<=>(const fixed_string&) const = default;
         [[nodiscard]] constexpr explicit(false) operator std::string_view() const { return {std::data(data), N}; }
         [[nodiscard]] constexpr auto size() const -> std::size_t { return N; }
         std::array<char, N + 1> data{};
      };

      template <std::size_t N>
      fixed_string(const char (&str)[N]) -> fixed_string<N - 1>;

      template <fixed_string Name>
      struct named
      {
         static constexpr std::string_view name = Name;
      };

      template <class TPtr>
      struct ptr
      {
         const TPtr* ptr;
      };

      template <class T>
      extern const T external;

      template <auto Ptr>
      [[nodiscard]] consteval auto member_name_impl() -> std::string_view
      {
         // const auto name = std::string_view{std::source_location::current().function_name()};
         const std::string_view name = GLZ_PRETTY_FUNCTION;
#if defined(__clang__)
         const auto split = name.substr(0, name.find("}]"));
         return split.substr(split.find_last_of(".") + 1);
#elif defined(__GNUC__)
         const auto split = name.substr(0, name.find(")}"));
         return split.substr(split.find_last_of(":") + 1);
#elif defined(_MSC_VER)
         const auto split = name.substr(0, name.find_last_of("}"));
         return split.substr(split.find_last_of(">") + 1);
#endif
      }

      template <auto N>
      [[nodiscard]] consteval auto nth(auto... args)
      {
         return [&]<std::size_t... Ns>(std::index_sequence<Ns...>) {
            return [](decltype((void*)Ns)..., auto* nth, auto*...) { return *nth; }(&args...);
         }(std::make_index_sequence<N>{});
      }

      template <auto N, class T, size_t M = count_members<T>()>
         requires(M <= 14)
      constexpr auto get_ptr(T&& t)
      {
         if constexpr (M == 14) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr<decltype(p14)>{&p14};
         }
         else if constexpr (M == 13) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr<decltype(p13)>{&p13};
         }
         else if constexpr (M == 12) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr<decltype(p12)>{&p12};
         }
         else if constexpr (M == 11) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr<decltype(p11)>{&p11};
         }
         else if constexpr (M == 10) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr<decltype(p10)>{&p10};
         }
         else if constexpr (M == 9) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr<decltype(p9)>{&p9};
         }
         else if constexpr (M == 8) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr<decltype(p8)>{&p8};
         }
         else if constexpr (M == 7) {
            auto&& [p1, p2, p3, p4, p5, p6, p7] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr<decltype(p7)>{&p7};
         }
         else if constexpr (M == 6) {
            auto&& [p1, p2, p3, p4, p5, p6] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr<decltype(p6)>{&p6};
         }
         else if constexpr (M == 5) {
            auto&& [p1, p2, p3, p4, p5] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr<decltype(p5)>{&p5};
         }
         else if constexpr (M == 4) {
            auto&& [p1, p2, p3, p4] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr<decltype(p4)>{&p4};
         }
         else if constexpr (M == 3) {
            auto&& [p1, p2, p3] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr<decltype(p3)>{&p3};
         }
         else if constexpr (M == 2) {
            auto&& [p1, p2] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr<decltype(p2)>{&p2};
         }
         else if constexpr (M == 1) {
            auto&& [p1] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr<decltype(p1)>{&p1};
         }
      }

      template <class T, auto N>
      [[nodiscard]] consteval auto member_name()
      {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
         constexpr auto name = member_name_impl<get_ptr<N>(external<T>)>();
#pragma clang diagnostic pop
#else
         constexpr auto name = member_name_impl<get_ptr<N>(external<T>)>();
#endif
         return [&]<auto... Ns>(std::index_sequence<Ns...>) {
            return fixed_string<sizeof...(Ns)>{name[Ns]...};
         }(std::make_index_sequence<name.size()>{});
      }

      template <class T, size_t... I>
      [[nodiscard]] constexpr auto member_names_impl(std::index_sequence<I...>)
      {
         return std::make_tuple(named<member_name<T, I>()>{}...);
      }

      template <class T>
      [[nodiscard]] constexpr auto member_names()
      {
         return member_names_impl<T>(std::make_index_sequence<count_members<T>()>{});
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

                  const auto key = get<I>(members).name;

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
         [[maybe_unused]] constexpr auto members = member_names<T>();
         static_assert(count_members<T>() == n);

         using value_t = reflection_value_tuple_variant_t<V>;

         auto naive_or_normal_hash = [&] {
            if constexpr (n <= 20) {
               return glz::detail::naive_map<value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{get<I>(members).name, std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
            else {
               return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{get<I>(members).name, std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
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

            if constexpr (num_members == 0 && Options.error_on_unknown_keys) {
               if (*it == '}') [[likely]] {
                  ++it;
                  return;
               }
               ctx.error = error_code::unknown_key;
               return;
            }
            else {
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
                     skip_ws_no_pre_check<Opts>(ctx, it, end);
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
         }
      };
   }
}
