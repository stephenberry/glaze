// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/parse.hpp"

namespace glz::detail
{
   template <opts Opts>
   GLZ_FLATTEN void skip_object(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.force_conformance) {
         skip_until_closed<Opts, '{', '}'>(ctx, it, end);
      }
      else {
         ++it;
         GLZ_SKIP_WS;
         if (*it == '}') {
            ++it;
            return;
         }
         while (true) {
            if (*it != '"') {
               ctx.error = error_code::syntax_error;
               return;
            }
            skip_string<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            GLZ_SKIP_WS;
            GLZ_MATCH_COLON;
            GLZ_SKIP_WS;
            skip_value<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            GLZ_SKIP_WS;
            if (*it != ',') break;
            ++it;
            GLZ_SKIP_WS;
         }
         match<'}'>(ctx, it);
      }
   }

   template <opts Opts>
   GLZ_FLATTEN void skip_array(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.force_conformance) {
         skip_until_closed<Opts, '[', ']'>(ctx, it, end);
      }
      else {
         ++it;
         GLZ_SKIP_WS;
         if (*it == ']') {
            ++it;
            return;
         }
         while (true) {
            skip_value<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            GLZ_SKIP_WS;
            if (*it != ',') break;
            ++it;
            GLZ_SKIP_WS;
         }
         match<']'>(ctx, it);
      }
   }

   template <opts Opts>
   GLZ_FLATTEN void skip_value(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.force_conformance) {
         if constexpr (!Opts.ws_handled) {
            GLZ_SKIP_WS;
         }
         while (true) {
            switch (*it) {
            case '{':
               skip_until_closed<Opts, '{', '}'>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               break;
            case '[':
               skip_until_closed<Opts, '[', ']'>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               break;
            case '"':
               skip_string<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               break;
            case '/':
               skip_comment(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               continue;
            case ',':
            case '}':
            case ']':
               break;
            case '\0':
               ctx.error = error_code::unexpected_end;
               return;
            default: {
               ++it;
               continue;
            }
            }

            break;
         }
      }
      else {
         if constexpr (!Opts.ws_handled) {
            GLZ_SKIP_WS;
         }
         switch (*it) {
         case '{': {
            skip_object<Opts>(ctx, it, end);
            break;
         }
         case '[': {
            skip_array<Opts>(ctx, it, end);
            break;
         }
         case '"': {
            skip_string<Opts>(ctx, it, end);
            break;
         }
         case 'n': {
            ++it;
            match<"ull", Opts>(ctx, it, end);
            break;
         }
         case 'f': {
            ++it;
            match<"alse", Opts>(ctx, it, end);
            break;
         }
         case 't': {
            ++it;
            match<"rue", Opts>(ctx, it, end);
            break;
         }
         case '\0': {
            ctx.error = error_code::unexpected_end;
            break;
         }
         default: {
            skip_number<Opts>(ctx, it, end);
         }
         }
      }
   }

   // expects opening whitespace to be handled
   template <opts Opts>
   GLZ_ALWAYS_INLINE auto parse_value(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      auto start = it;
      skip_value<Opts>(ctx, it, end);
      return std::span{start, size_t(it - start)};
   }
}
