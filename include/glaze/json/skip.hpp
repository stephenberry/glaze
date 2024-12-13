// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/parse.hpp"

namespace glz::detail
{
   template <>
   struct skip_value<JSON>
   {
      template <opts Opts>
         requires(not Opts.comments)
      GLZ_ALWAYS_INLINE static void op(is_context auto&& ctx, auto&& it, auto&& end) noexcept;

      template <opts Opts>
         requires(bool(Opts.comments))
      GLZ_ALWAYS_INLINE static void op(is_context auto&& ctx, auto&& it, auto&& end) noexcept;
   };

   template <opts Opts>
   void skip_object(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.validate_skipped) {
         ++it;
         GLZ_INVALID_END();
         skip_until_closed<Opts, '{', '}'>(ctx, it, end);
      }
      else {
         GLZ_ADD_LEVEL;
         ++it;
         GLZ_INVALID_END();
         GLZ_SKIP_WS();
         if (*it == '}') {
            GLZ_SUB_LEVEL;
            ++it;
            GLZ_VALID_END();
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
            GLZ_SKIP_WS();
            GLZ_MATCH_COLON();
            GLZ_SKIP_WS();
            skip_value<JSON>::op<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            GLZ_SKIP_WS();
            if (*it != ',') break;
            ++it;
            GLZ_INVALID_END();
            GLZ_SKIP_WS();
         }
         match<'}'>(ctx, it);
         GLZ_SUB_LEVEL;
         GLZ_VALID_END();
      }
   }

   template <opts Opts>
      requires(Opts.format == JSON || Opts.format == NDJSON)
   void skip_array(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.validate_skipped) {
         ++it;
         GLZ_INVALID_END();
         skip_until_closed<Opts, '[', ']'>(ctx, it, end);
      }
      else {
         GLZ_ADD_LEVEL;
         ++it;
         GLZ_INVALID_END();
         GLZ_SKIP_WS();
         if (*it == ']') {
            GLZ_SUB_LEVEL;
            ++it;
            GLZ_VALID_END();
            return;
         }
         while (true) {
            skip_value<JSON>::op<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            GLZ_SKIP_WS();
            if (*it != ',') break;
            ++it;
            GLZ_INVALID_END();
            GLZ_SKIP_WS();
         }
         match<']'>(ctx, it);
         GLZ_SUB_LEVEL;
         GLZ_VALID_END();
      }
   }

   template <opts Opts>
      requires(not Opts.comments)
   GLZ_ALWAYS_INLINE void skip_value<JSON>::op(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (not Opts.validate_skipped) {
         if constexpr (not has_ws_handled(Opts)) {
            GLZ_SKIP_WS();
         }
         while (true) {
            switch (*it) {
            case '{':
               ++it;
               GLZ_INVALID_END();
               skip_until_closed<Opts, '{', '}'>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               break;
            case '[':
               ++it;
               GLZ_INVALID_END();
               skip_until_closed<Opts, '[', ']'>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               break;
            case '"':
               skip_string<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               break;
            case ',':
            case '}':
            case ']':
               break;
            case '\0':
               ctx.error = error_code::unexpected_end;
               return;
            default: {
               ++it;
               GLZ_INVALID_END();
               continue;
            }
            }

            break;
         }
      }
      else {
         if constexpr (not has_ws_handled(Opts)) {
            GLZ_SKIP_WS();
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

   template <opts Opts>
      requires(bool(Opts.comments))
   GLZ_ALWAYS_INLINE void skip_value<JSON>::op(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (not has_ws_handled(Opts)) {
         GLZ_SKIP_WS();
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
      case '/': {
         skip_comment(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
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

   // parse_value is used for JSON pointer reading
   // we want the JSON pointer access to not care about trailing whitespace
   // so we use validate_skipped for precise validation and value skipping
   // expects opening whitespace to be handled
   template <opts Opts>
   GLZ_ALWAYS_INLINE auto parse_value(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      auto start = it;
      skip_value<JSON>::op<opt_true<Opts, &opts::validate_skipped>>(ctx, it, end);
      return std::span{start, size_t(it - start)};
   }
}
