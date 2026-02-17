// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/yaml/common.hpp"

namespace glz::yaml
{
   // Skip a double-quoted string
   template <class It, class End, class Ctx>
   inline void skip_double_quoted_string(It& it, End end, Ctx& ctx) noexcept
   {
      if (it == end || *it != '"') [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return;
      }

      ++it; // skip opening quote
      while (it != end) {
         if (*it == '\\') {
            ++it;
            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            ++it; // skip escaped character
            continue;
         }
         if (*it == '"') {
            ++it; // skip closing quote
            return;
         }
         ++it;
      }
      ctx.error = error_code::syntax_error; // unterminated string
   }

   // Skip a single-quoted string
   template <class It, class End, class Ctx>
   inline void skip_single_quoted_string(It& it, End end, Ctx& ctx) noexcept
   {
      if (it == end || *it != '\'') [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return;
      }

      ++it; // skip opening quote
      while (it != end) {
         if (*it == '\'') {
            // Check for escaped single quote ('')
            if ((it + 1) != end && *(it + 1) == '\'') {
               it += 2;
               continue;
            }
            ++it; // skip closing quote
            return;
         }
         ++it;
      }
      ctx.error = error_code::syntax_error; // unterminated string
   }

   // Skip a block scalar (| or >)
   template <class It, class End, class Ctx>
   inline void skip_block_scalar(It& it, End end, Ctx& ctx, int32_t base_indent) noexcept
   {
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }

      // Skip the indicator (| or >)
      ++it;

      // Skip optional chomping indicator and indentation indicator
      while (it != end && (*it == '+' || *it == '-' || (*it >= '1' && *it <= '9'))) {
         ++it;
      }

      // Skip to end of line
      skip_ws_and_comment(it, end);
      if (!skip_newline(it, end)) {
         // Empty block scalar
         return;
      }

      // Determine content indentation from first content line
      int32_t content_indent = -1;

      while (it != end) {
         // Measure indent of current line
         auto line_start = it;
         int32_t line_indent = measure_indent<false>(it, end, ctx);
         if (bool(ctx.error)) [[unlikely]]
            return;

         // Check if this is a blank line
         if (it != end && (*it == '\n' || *it == '\r')) {
            skip_newline(it, end);
            continue;
         }

         // First content line sets the indentation
         if (content_indent < 0) {
            if (line_indent <= base_indent) {
               // Dedented past base - end of block scalar
               it = line_start;
               return;
            }
            content_indent = line_indent;
         }

         // Check if we've dedented
         if (line_indent < content_indent) {
            it = line_start;
            return;
         }

         // Skip to end of line
         while (it != end && *it != '\n' && *it != '\r') {
            ++it;
         }
         skip_newline(it, end);
      }
   }

   // Skip flow-style content (enclosed in [] or {})
   template <class It, class End, class Ctx>
   inline void skip_flow_content(It& it, End end, Ctx& ctx, char open, char close) noexcept
   {
      if (it == end || *it != open) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return;
      }

      int depth = 1;
      ++it;

      while (depth > 0) {
         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const char c = *it;

         if (c == '"') {
            skip_double_quoted_string(it, end, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;
            continue;
         }

         if (c == '\'') {
            skip_single_quoted_string(it, end, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;
            continue;
         }

         if (c == '#') {
            // Skip comment to end of line
            while (it != end && *it != '\n' && *it != '\r') {
               ++it;
            }
            continue;
         }

         if (c == open) {
            ++depth;
            ++it;
            continue;
         }

         if (c == close) {
            --depth;
            ++it;
            continue;
         }

         // Handle nested structures
         if (c == '[') {
            skip_flow_content(it, end, ctx, '[', ']');
            if (bool(ctx.error)) [[unlikely]]
               return;
            continue;
         }

         if (c == '{') {
            skip_flow_content(it, end, ctx, '{', '}');
            if (bool(ctx.error)) [[unlikely]]
               return;
            continue;
         }

         ++it;
      }
   }

   // Skip a plain scalar (unquoted)
   template <class It, class End>
   inline void skip_plain_scalar(It& it, End end, bool in_flow) noexcept
   {
      while (it != end) {
         const char c = *it;

         // End of plain scalar
         if (c == '\n' || c == '\r') {
            break;
         }

         // Comment starts
         if (c == '#') {
            // Only if preceded by whitespace
            break;
         }

         // Flow indicators end plain scalars in flow context
         if (in_flow && (c == ',' || c == ']' || c == '}')) {
            break;
         }

         // Colon followed by space/newline ends plain scalar (key separator)
         if (c == ':') {
            if ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n' || *(it + 1) == '\r') {
               break;
            }
            if (in_flow && (*(it + 1) == ',' || *(it + 1) == ']' || *(it + 1) == '}')) {
               break;
            }
         }

         ++it;
      }
   }

   // Skip any YAML value (for skipping unknown keys)
   template <auto Opts, class Ctx, class It, class End>
   inline void skip_yaml_value(Ctx& ctx, It& it, End end, int32_t current_indent, bool in_flow) noexcept
   {
      skip_inline_ws(it, end);

      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }

      // Handle anchor: skip name, then continue to skip the actual value
      if (*it == '&') {
         ++it;
         parse_anchor_name(it, end);
         skip_inline_ws(it, end);
         if (it == end) return;
      }

      // Handle alias: skip name and we're done (alias is a leaf reference)
      if (*it == '*') {
         ++it;
         parse_anchor_name(it, end);
         return;
      }

      const char c = *it;

      // Double-quoted string
      if (c == '"') {
         skip_double_quoted_string(it, end, ctx);
         return;
      }

      // Single-quoted string
      if (c == '\'') {
         skip_single_quoted_string(it, end, ctx);
         return;
      }

      // Block scalar
      if (c == '|' || c == '>') {
         skip_block_scalar(it, end, ctx, current_indent);
         return;
      }

      // Flow sequence
      if (c == '[') {
         skip_flow_content(it, end, ctx, '[', ']');
         return;
      }

      // Flow mapping
      if (c == '{') {
         skip_flow_content(it, end, ctx, '{', '}');
         return;
      }

      // Block sequence (starts with -)
      if (c == '-' && ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n')) {
         // Skip entire block sequence
         while (it != end) {
            // Skip current line
            while (it != end && *it != '\n' && *it != '\r') {
               ++it;
            }
            if (!skip_newline(it, end)) break;

            // Check next line indent
            auto line_start = it;
            int32_t line_indent = measure_indent(it, end, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;

            if (it == end) break;

            // Check for document end marker
            if (at_document_end(it, end)) {
               it = line_start;
               break;
            }

            // If dedented or not a sequence item, we're done
            if (line_indent <= current_indent) {
               it = line_start;
               break;
            }

            // Continue with next line
         }
         return;
      }

      // Plain scalar or nested block mapping
      skip_plain_scalar(it, end, in_flow);

      // Check if this is followed by a colon (nested mapping)
      skip_inline_ws(it, end);
      if (it != end && *it == ':') {
         // This was a key - skip the value too
         ++it;
         skip_inline_ws(it, end);

         if (it != end && !at_newline_or_end(it, end)) {
            skip_yaml_value<Opts>(ctx, it, end, current_indent, in_flow);
         }
      }
   }

} // namespace glz::yaml

namespace glz
{
   template <>
   struct skip_value<YAML>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         yaml::skip_yaml_value<Opts>(ctx, it, end, 0, false);
      }
   };
} // namespace glz
