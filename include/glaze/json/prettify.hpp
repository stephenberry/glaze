// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Prettifying JSONC preserves comments of both styles. A line comment runs to the end of its
// line, so whatever follows one is put on a new line rather than behind it.

#include "glaze/json/json_format.hpp"

namespace glz
{
   namespace detail
   {
      template <auto Opts>
      inline void prettify_json(is_context auto&& ctx, auto&& it, auto&& end, auto&& b, auto& ix)
      {
         constexpr bool use_tabs = check_indentation_char(Opts) == '\t';
         constexpr auto indent_width = check_indentation_width(Opts);

         using enum json_type;

         std::vector<json_type> state(64);
         int64_t indent{};

         // Set while the output cursor sits on a line that a `//` comment has commented out. Only
         // the comment-enabled path can set it, so the plain JSON writer keeps the code it had.
         [[maybe_unused]] bool line_comment_open = false;

         // Every line break the writer emits goes through this, because a break is also what ends a
         // line comment: doing it in one place is what keeps the two from doubling up.
         auto new_line = [&] {
            append_new_line<use_tabs, indent_width>(b, ix, indent);
            if constexpr (Opts.comments) {
               line_comment_open = false;
            }
         };

         // Anything written while a line comment is open has to start a line of its own first, or
         // the comment swallows it. Deferring the break to the next write rather than emitting it
         // with the comment is what keeps a comment at the end of the input from leaving a trailing
         // newline behind, and lets a break the writer was going to emit anyway serve as this one.
         auto close_line_comment = [&] {
            if constexpr (Opts.comments) {
               if (line_comment_open) [[unlikely]] {
                  new_line();
               }
            }
         };

         while (it < end) {
            switch (json_types[uint8_t(*it)]) {
            case String: {
               const auto value = read_json_string(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               close_line_comment();
               dump(value, b, ix); // non-empty: the scanner reports an empty view as an error
               break;
            }
            case Comma: {
               close_line_comment();
               dump(',', b, ix);
               ++it;
               if constexpr (check_new_lines_in_arrays(Opts)) {
                  new_line();
               }
               else {
                  if (state[indent] == Object_Start) {
                     new_line();
                  }
                  else {
                     if constexpr (use_tabs) {
                        dump('\t', b, ix);
                     }
                     else {
                        dump(' ', b, ix);
                     }
                  }
               }
               break;
            }
            case Number: {
               const auto value = read_json_number<Opts.null_terminated>(it, end);
               close_line_comment();
               dump_not_empty(value, b, ix); // non-empty: a Number match is one valid character
               break;
            }
            case Colon: {
               close_line_comment();
               if constexpr (use_tabs) {
                  dump(":\t", b, ix);
               }
               else {
                  dump(": ", b, ix);
               }
               ++it;
               break;
            }
            case Array_Start: {
               close_line_comment();
               dump('[', b, ix);
               ++it;
               ++indent;
               if (size_t(indent) >= state.size()) [[unlikely]] {
                  state.resize(state.size() * 2);
                  if (state.size() >= max_recursive_depth_limit) [[unlikely]] {
                     ctx.error = error_code::exceeded_max_recursive_depth;
                     return;
                  }
               }
               state[indent] = Array_Start;
               if constexpr (check_new_lines_in_arrays(Opts)) {
                  if constexpr (not Opts.null_terminated) {
                     if (it != end && *it != ']') {
                        new_line();
                     }
                  }
                  else {
                     if (*it != ']') {
                        new_line();
                     }
                  }
               }
               break;
            }
            case Array_End: {
               --indent;
               if (indent < 0) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if constexpr (check_new_lines_in_arrays(Opts)) {
                  if (it[-1] != '[') {
                     new_line();
                  }
               }
               close_line_comment();
               dump(']', b, ix);
               ++it;
               break;
            }
            case Null: {
               // The type table matched on the first byte alone; see match_literal for why writing
               // the literal from the writer's own spelling needs the rest of it checked.
               if (not match_literal<"null">(ctx, it, end)) [[unlikely]] {
                  return;
               }
               close_line_comment();
               dump("null", b, ix);
               break;
            }
            case Bool: {
               if (*it == 't') {
                  if (not match_literal<"true">(ctx, it, end)) [[unlikely]] {
                     return;
                  }
                  close_line_comment();
                  dump("true", b, ix);
                  break;
               }
               else {
                  if (not match_literal<"false">(ctx, it, end)) [[unlikely]] {
                     return;
                  }
                  close_line_comment();
                  dump("false", b, ix);
                  break;
               }
            }
            case Object_Start: {
               close_line_comment();
               dump('{', b, ix);
               ++it;
               ++indent;
               if (size_t(indent) >= state.size()) [[unlikely]] {
                  state.resize(state.size() * 2);
                  if (state.size() >= max_recursive_depth_limit) [[unlikely]] {
                     ctx.error = error_code::exceeded_max_recursive_depth;
                     return;
                  }
               }
               state[indent] = Object_Start;
               if constexpr (not Opts.null_terminated) {
                  if (it != end && *it != '}') [[unlikely]] {
                     new_line();
                  }
               }
               else {
                  if (*it != '}') {
                     new_line();
                  }
               }
               break;
            }
            case Object_End: {
               --indent;
               if (indent < 0) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if (it[-1] != '{') {
                  new_line();
               }
               close_line_comment();
               dump('}', b, ix);
               ++it;
               break;
            }
            case Comment: {
               if constexpr (Opts.comments) {
                  const auto comment = read_jsonc_comment(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  close_line_comment();
                  dump(comment.text, b, ix); // non-empty: an empty view comes with an error
                  line_comment_open = comment.line;
                  break;
               }
               else {
                  // A '/' opens a comment, and plain JSON has none. Skipping it as though it were
                  // whitespace silently rewrote the document: "{\"a\":1} // tail" came out as
                  // "{...}true", the 't' of "tail" taken for a literal. minify_json has always
                  // reported this, and the two had no business disagreeing.
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            case Whitespace: {
               // Skip whitespace characters that are part of raw JSON content
               ++it;
               break;
            }
            [[unlikely]] default: {
               ctx.error = error_code::syntax_error;
               return;
            }
            }
         }
      }

      template <auto Opts, contiguous In, output_buffer Out>
      inline void prettify_json(is_context auto&& ctx, In&& in, Out&& out)
      {
         if constexpr (resizable<Out>) {
            if (in.size() == 0) {
               out.resize(0); // resize is what `resizable` promises; clear() is not
               return;
            }
            out.resize(in.size() * 2);
         }
         size_t ix = 0;
         auto [it, end] = read_iterators<Opts>(in);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         if constexpr (self_terminating<In>) {
            prettify_json<opt_true<Opts, &opts::null_terminated>>(ctx, it, end, out, ix);
         }
         else {
            prettify_json<opt_false<Opts, &opts::null_terminated>>(ctx, it, end, out, ix);
         }

         if constexpr (resizable<Out>) {
            out.resize(ix);
         }
      }
   }

   // The overloads that write into a caller's buffer report what went wrong. Not [[nodiscard]]:
   // prettifying auto-generated JSON does not fail, so the callers that have always ignored the
   // outcome are right to, and warning at all of them would say nothing useful. The overloads that
   // return the text have nowhere to put an error and stay silent.
   //
   // Prettifying reports what it actually parses, which is strings, comments and literals. It does
   // not check that the document is structurally valid JSON -- `[1 2]` prettifies to `[12]` -- so a
   // document that may not be well formed wants glz::validate_json or glz::validate_jsonc.

   template <auto Opts = opts{}>
   inline error_ctx prettify_json(const auto& in, auto& out)
   {
      context ctx{};
      detail::prettify_json<Opts>(ctx, in, out);
      return {0, ctx.error, ctx.custom_error_message};
   }

   /// <summary>
   /// allocating version of prettify
   /// </summary>
   template <auto Opts = opts{}>
   inline std::string prettify_json(const auto& in)
   {
      context ctx{};
      std::string out{};
      detail::prettify_json<Opts>(ctx, in, out);
      return out;
   }

   template <auto Opts = opts{}>
   inline error_ctx prettify_jsonc(const auto& in, auto& out)
   {
      context ctx{};
      detail::prettify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return {0, ctx.error, ctx.custom_error_message};
   }

   /// <summary>
   /// allocating version of prettify
   /// </summary>
   template <auto Opts = opts{}>
   inline std::string prettify_jsonc(const auto& in)
   {
      context ctx{};
      std::string out{};
      detail::prettify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return out;
   }
}
