// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>
#include <tuple>

#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail {
      template <class T, std::size_t N>
      struct static_vector {
         constexpr auto push_back(const T& elem) { elems[size++] = elem; }
         constexpr auto& operator[](auto i) const { return elems[i]; }

         T elems[N + 1]{};
         std::size_t size{};
      };

      constexpr auto to_names(auto& out, auto, auto... args) {
         if constexpr (sizeof...(args) > 1) {
            out.push_back(std::get<2>(std::tuple{args...}));
         }
      }

      struct any_t {
         template <class T>
         constexpr operator T();
      };

      template <class T, class... Args> requires (std::is_aggregate_v<std::remove_cvref_t<T>>)
      consteval auto count_members() {
         if constexpr (requires { T{{Args{}}..., {any_t{}}}; } == false) {
            return sizeof...(Args);
         } else {
            return count_members<T, Args..., any_t>();
         }
      }

      template <class T>
      constexpr auto member_names() {
         static constexpr auto N = count_members<T>();
         static_vector<std::string_view, N> v{};
         T t{};
         __builtin_dump_struct(&t, to_names, v);
         return v;
      }

      template <class T>
      constexpr decltype(auto) to_tuple(T& t) {
         static constexpr auto N = count_members<std::decay_t<T>>();
         if constexpr (N == 0) {
            return std::tuple{};
         } else if constexpr (N == 1) {
            auto&& [p] = t;
            return std::tuple{p};
         } else if constexpr (N == 2) {
            auto&& [p0, p1] = t;
            return std::tuple{p0, p1};
         } else if constexpr (N == 3) {
            auto&& [p0, p1, p2] = t;
            return std::tuple{p0, p1, p2};
         } else if constexpr (N == 4) {
            auto&& [p0, p1, p2, p3] = t;
            return std::tuple{p0, p1, p2, p3};
         }
      }

      template <class T> requires (!glaze_t<T> && !array_t<T> && std::is_aggregate_v<std::remove_cvref_t<T>>)
      struct to_json<T>
      {
         template <auto Options>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            auto members = member_names<T>();
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
   }
}
