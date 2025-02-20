// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/parse.hpp"

namespace glz
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
         if constexpr (not Opts.null_terminated) {
            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
         }
         skip_until_closed<Opts, '{', '}'>(ctx, it, end);
      }
      else {
         if constexpr (not Opts.null_terminated) {
            ++ctx.indentation_level;
         }
         ++it;
         if constexpr (not Opts.null_terminated) {
            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
         }
         if (skip_ws<Opts>(ctx, it, end)) {
            return;
         }
         if (*it == '}') {
            if constexpr (not Opts.null_terminated) {
               --ctx.indentation_level;
            }
            ++it;
            if constexpr (not Opts.null_terminated) {
               if (it == end) {
                  ctx.error = error_code::end_reached;
                  return;
               }
            }
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
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
            if (match_invalid_end<':', Opts>(ctx, it, end)) {
               return;
            }
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
            skip_value<JSON>::op<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
            if (*it != ',') break;
            ++it;
            if constexpr (not Opts.null_terminated) {
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
         }
         match<'}'>(ctx, it);
         if constexpr (not Opts.null_terminated) {
            --ctx.indentation_level;
         }
         if constexpr (not Opts.null_terminated) {
            if (it == end) {
               ctx.error = error_code::end_reached;
               return;
            }
         }
      }
   }
   
   template <opts Opts>
   requires(Opts.format == JSON || Opts.format == NDJSON)
   void skip_array(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.validate_skipped) {
         ++it;
         if constexpr (not Opts.null_terminated) {
            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
         }
         skip_until_closed<Opts, '[', ']'>(ctx, it, end);
      }
      else {
         if constexpr (not Opts.null_terminated) {
            ++ctx.indentation_level;
         }
         ++it;
         if constexpr (not Opts.null_terminated) {
            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
         }
         if (skip_ws<Opts>(ctx, it, end)) {
            return;
         }
         if (*it == ']') {
            if constexpr (not Opts.null_terminated) {
               --ctx.indentation_level;
            }
            ++it;
            if constexpr (not Opts.null_terminated) {
               if (it == end) {
                  ctx.error = error_code::end_reached;
                  return;
               }
            }
            return;
         }
         while (true) {
            skip_value<JSON>::op<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
            if (*it != ',') break;
            ++it;
            if constexpr (not Opts.null_terminated) {
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
         }
         match<']'>(ctx, it);
         if constexpr (not Opts.null_terminated) {
            --ctx.indentation_level;
         }
         if constexpr (not Opts.null_terminated) {
            if (it == end) {
               ctx.error = error_code::end_reached;
               return;
            }
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
   
   template <opts Opts>
   requires(not Opts.comments)
   GLZ_ALWAYS_INLINE void skip_value<JSON>::op(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      using namespace glz::detail;
      
      if constexpr (not Opts.validate_skipped) {
         if constexpr (not has_ws_handled(Opts)) {
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
         }
         while (true) {
            switch (*it) {
               case '{':
                  ++it;
                  if constexpr (not Opts.null_terminated) {
                     if (it == end) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                  }
                  skip_until_closed<Opts, '{', '}'>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  break;
               case '[':
                  ++it;
                  if constexpr (not Opts.null_terminated) {
                     if (it == end) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                  }
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
                  if constexpr (not Opts.null_terminated) {
                     if (it == end) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                  }
                  continue;
               }
            }
            
            break;
         }
      }
      else {
         if constexpr (not has_ws_handled(Opts)) {
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
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
      using namespace glz::detail;
      
      if constexpr (not has_ws_handled(Opts)) {
         if (skip_ws<Opts>(ctx, it, end)) {
            return;
         }
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
