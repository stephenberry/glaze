#pragma once

#include <array>

#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/toml/common.hpp"

namespace glz::toml
{
   template <class It, class End>
   inline void skip_comment(It& it, End end) noexcept
   {
      while (it != end && *it != '\n' && *it != '\r') {
         ++it;
      }
   }

   template <class It, class End, class Ctx>
   inline void skip_toml_string(It& it, End end, Ctx& ctx) noexcept
   {
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }

      if ((it + 2) < end && *it == '"' && *(it + 1) == '"' && *(it + 2) == '"') {
         it += 3;
         if (it != end && *it == '\n') {
            ++it;
         }
         else if ((it + 1) < end && *it == '\r' && *(it + 1) == '\n') {
            it += 2;
         }

         while (true) {
            if ((it + 2) >= end) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (*it == '"' && *(it + 1) == '"' && *(it + 2) == '"') {
               it += 3;
               break;
            }
            if (*it == '\\') {
               ++it;
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }
            ++it;
         }
      }
      else {
         ++it; // skip opening quote
         while (it != end) {
            if (*it == '\\') {
               ++it;
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               ++it;
               continue;
            }
            if (*it == '"') {
               ++it;
               return;
            }
            if (*it == '\n' || *it == '\r') [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
         }
         ctx.error = error_code::syntax_error;
      }
   }

   template <class It, class End, class Ctx>
   inline void skip_literal_string(It& it, End end, Ctx& ctx) noexcept
   {
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }

      if ((it + 2) < end && *it == '\'' && *(it + 1) == '\'' && *(it + 2) == '\'') {
         it += 3;
         if (it != end && *it == '\n') {
            ++it;
         }
         else if ((it + 1) < end && *it == '\r' && *(it + 1) == '\n') {
            it += 2;
         }

         while (true) {
            if ((it + 2) >= end) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (*it == '\'' && *(it + 1) == '\'' && *(it + 2) == '\'') {
               it += 3;
               break;
            }
            ++it;
         }
      }
      else {
         ++it; // skip opening quote
         while (it != end && *it != '\'') {
            if (*it == '\n' || *it == '\r') [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
         }
         if (it == end) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         ++it; // skip closing quote
      }
   }

   // Skips a bracketed TOML value. Only the two TOML bracket pairs are supported, because nesting
   // is tracked across both of them: an array and an inline table may contain each other, so a
   // skipper that understood only one pair could not find the matching close.
   template <char Open, char Close, class It, class End, class Ctx>
   inline void skip_enclosed(It& it, End end, Ctx& ctx) noexcept
   {
      static_assert((Open == '[' && Close == ']') || (Open == '{' && Close == '}'),
                    "TOML encloses values in either [] or {}");

      if (it == end || *it != Open) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return;
      }

      // Nesting is tracked with an explicit stack of expected closing characters rather than by
      // recursing on the opposite bracket type, so that adversarial input such as `[{[{...` is
      // bounded by max_recursive_depth_limit instead of overflowing the call stack.
      std::array<char, max_recursive_depth_limit> closers{};
      size_t depth = 0;
      closers[depth++] = Close;
      ++it;

      while (depth > 0) {
         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const char c = *it;
         if (c == '"') {
            skip_toml_string(it, end, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;
            continue;
         }

         if (c == '\'') {
            skip_literal_string(it, end, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;
            continue;
         }

         if (c == '#') {
            skip_comment(it, end);
            continue;
         }

         if (c == closers[depth - 1]) {
            --depth;
            ++it;
            continue;
         }

         if (c == '[' || c == '{') {
            if (depth >= max_recursive_depth_limit) [[unlikely]] {
               ctx.error = error_code::exceeded_max_recursive_depth;
               return;
            }
            closers[depth++] = (c == '[') ? ']' : '}';
            ++it;
            continue;
         }

         ++it;
      }
   }

   template <auto Opts, class Ctx, class It, class End>
   inline void skip_value_impl(Ctx& ctx, It& it, End& end) noexcept
   {
      skip_ws_and_comments(it, end);
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }

      const char c = *it;
      if (c == '"') {
         skip_toml_string(it, end, ctx);
      }
      else if (c == '\'') {
         skip_literal_string(it, end, ctx);
      }
      else if (c == '[') {
         skip_enclosed<'[', ']'>(it, end, ctx);
      }
      else if (c == '{') {
         skip_enclosed<'{', '}'>(it, end, ctx);
      }
      else {
         while (it != end) {
            const char ch = *it;
            if (ch == '\n' || ch == '\r' || ch == ',' || ch == ']' || ch == '}' || ch == '#') {
               break;
            }
            ++it;
         }
      }
   }
} // namespace glz::toml

namespace glz
{
   template <>
   struct skip_value<TOML>
   {
      template <auto Opts>
      static void op(is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         toml::skip_value_impl<Opts>(ctx, it, end);
      }
   };
} // namespace glz
