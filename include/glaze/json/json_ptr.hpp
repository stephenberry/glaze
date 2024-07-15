// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <any>
#include <charconv>

#include "glaze/core/seek.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/skip.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   [[nodiscard]] inline constexpr bool maybe_numeric_key(const sv key)
   {
      return key.find_first_not_of("0123456789") == std::string_view::npos;
   }

   template <string_literal Str, auto Opts = opts{}>
   [[nodiscard]] inline auto get_view_json(contiguous auto&& buffer)
   {
      static constexpr auto s = chars<Str>;

      static constexpr auto tokens = split_json_ptr<s>();
      static constexpr auto N = tokens.size();

      context ctx{};
      auto p = read_iterators<Opts>(ctx, buffer);

      auto it = p.first;
      auto end = p.second;

      // using span_t = std::span<std::remove_pointer_t<std::remove_reference_t<decltype(it)>>>;
      using span_t = std::span<const char>; // TODO: should be more generic, but currently broken with mingw
      using result_t = expected<span_t, error_ctx>;

      auto start = it;

      if (bool(ctx.error)) [[unlikely]] {
         return result_t{unexpected(error_ctx{ctx.error})};
      }

      if constexpr (N == 0) {
         return result_t{span_t{it, end}};
      }
      else {
         using namespace glz::detail;

         skip_ws<Opts>(ctx, it, end);

         result_t ret;

         for_each<N>([&](auto I) {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            static constexpr auto key = std::get<I>(tokens);
            if constexpr (maybe_numeric_key(key)) {
               switch (*it) {
               case '{': {
                  ++it;
                  while (true) {
                     GLZ_SKIP_WS();
                     const auto k = parse_key(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (cx_string_cmp<key>(k)) {
                        GLZ_SKIP_WS();
                        GLZ_MATCH_COLON();
                        GLZ_SKIP_WS();

                        if constexpr (I == (N - 1)) {
                           ret = parse_value<Opts>(ctx, it, end);
                        }
                        return;
                     }
                     else {
                        skip_value<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        if (*it != ',') {
                           ctx.error = error_code::key_not_found;
                           return;
                        }
                        ++it;
                     }
                  }
               }
               case '[': {
                  ++it;
                  // Could optimize by counting commas
                  static constexpr auto n = stoui(key);
                  if constexpr (n) {
                     for_each<n.value()>([&](auto) {
                        skip_value<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        if (*it != ',') {
                           ctx.error = error_code::array_element_not_found;
                           return;
                        }
                        ++it;
                     });

                     GLZ_SKIP_WS();

                     if constexpr (I == (N - 1)) {
                        ret = parse_value<Opts>(ctx, it, end);
                     }
                     return;
                  }
                  else {
                     ctx.error = error_code::array_element_not_found;
                     return;
                  }
               }
               }
            }
            else {
               GLZ_MATCH_OPEN_BRACE;

               while (it < end) {
                  GLZ_SKIP_WS();
                  const auto k = parse_key(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  if (cx_string_cmp<key>(k)) {
                     GLZ_SKIP_WS();
                     GLZ_MATCH_COLON();
                     GLZ_SKIP_WS();

                     if constexpr (I == (N - 1)) {
                        ret = parse_value<Opts>(ctx, it, end);
                     }
                     return;
                  }
                  else {
                     skip_value<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (*it != ',') {
                        ctx.error = error_code::key_not_found;
                        return;
                     }
                     ++it;
                  }
               }
            }
         });

         if (bool(ctx.error)) [[unlikely]] {
            return result_t{unexpected(error_ctx{ctx.error, "", size_t(it - start)})};
         }

         return ret;
      }
   }

   template <class T, string_literal Str, auto Opts = opts{}>
   [[nodiscard]] inline expected<T, error_ctx> get_as_json(contiguous auto&& buffer)
   {
      const auto str = glz::get_view_json<Str>(buffer);
      if (str) {
         return glz::read_json<T>(*str);
      }
      return unexpected(str.error());
   }

   template <string_literal Str, auto Opts = opts{}>
   [[nodiscard]] inline expected<sv, error_ctx> get_sv_json(contiguous auto&& buffer)
   {
      const auto s = glz::get_view_json<Str>(buffer);
      if (s) {
         return sv{reinterpret_cast<const char*>(s->data()), s->size()};
      }
      return unexpected(s.error());
   }
}
