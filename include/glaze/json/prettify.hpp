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
         // line comment: doing it in one place is what keeps the two from doubling up. The newline
         // and the indentation behind it are also what make the output unbounded -- there is no
         // bound on prettified output that a single reservation up front could use -- so this is
         // the write a bounded destination is likeliest to run out on.
         const auto new_line = [&] {
            if constexpr (has_bounded_capacity<decltype(b)>) {
               const size_t n = use_tabs ? size_t(indent) : size_t(indent) * indent_width;
               if (not ensure_space(ctx, b, ix + 1 + n)) [[unlikely]] {
                  return false;
               }
            }
            append_new_line<use_tabs, indent_width>(b, ix, indent);
            if constexpr (Opts.comments) {
               line_comment_open = false;
            }
            return true;
         };

         // Every write below goes through this, which is what fixes their order in one place: an
         // open line comment is closed first, because anything written into one is commented out,
         // and emit_bytes then takes the reservation from the argument rather than from a byte
         // count spelled out beside the write.
         //
         // Deferring that break to the next write rather than emitting it with the comment is what
         // keeps a comment at the end of the input from leaving a trailing newline behind, and lets
         // a break the writer was going to emit anyway serve as this one.
         //
         // A break that fails is reported rather than swallowed: on a bounded destination that ran
         // out, carrying on would leave `line_comment_open` set and put this write *inside* the
         // comment, which is not a truncated document but a wrong one.
         const auto emit = [&](const auto& x) {
            if constexpr (Opts.comments) {
               if (line_comment_open) [[unlikely]] {
                  if (not new_line()) [[unlikely]] {
                     return false;
                  }
               }
            }
            return emit_bytes<true>(ctx, b, ix, x);
         };

         while (it < end) {
            switch (json_types[uint8_t(*it)]) {
            case String: {
               const auto value = read_json_string(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               // non-empty: the scanner reports an empty view as an error
               if (not emit(value)) [[unlikely]] {
                  return;
               }
               break;
            }
            case Comma: {
               if (not emit(',')) [[unlikely]] {
                  return;
               }
               ++it;
               if constexpr (check_new_lines_in_arrays(Opts)) {
                  if (not new_line()) [[unlikely]] {
                     return;
                  }
               }
               else {
                  if (state[indent] == Object_Start) {
                     if (not new_line()) [[unlikely]] {
                        return;
                     }
                  }
                  else if (not emit(use_tabs ? '\t' : ' ')) [[unlikely]] {
                     return;
                  }
               }
               break;
            }
            case Number: {
               const auto value = read_json_number<Opts.null_terminated>(it, end);
               // non-empty: a Number match is one valid character
               if (not emit(value)) [[unlikely]] {
                  return;
               }
               break;
            }
            case Colon: {
               static constexpr sv colon = use_tabs ? sv{":\t"} : sv{": "};
               if (not emit(colon)) [[unlikely]] {
                  return;
               }
               ++it;
               break;
            }
            case Array_Start: {
               if (not emit('[')) [[unlikely]] {
                  return;
               }
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
                        if (not new_line()) [[unlikely]] {
                           return;
                        }
                     }
                  }
                  else {
                     if (*it != ']') {
                        if (not new_line()) [[unlikely]] {
                           return;
                        }
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
               // This break is the one the bracket wanted anyway, and it closes an open line
               // comment on its way, leaving the emit below nothing to close.
               if constexpr (check_new_lines_in_arrays(Opts)) {
                  if (it[-1] != '[') {
                     if (not new_line()) [[unlikely]] {
                        return;
                     }
                  }
               }
               if (not emit(']')) [[unlikely]] {
                  return;
               }
               ++it;
               break;
            }
            case Null: {
               // The type table matched on the first byte alone; see match_literal for why writing
               // the literal from the writer's own spelling needs the rest of it checked.
               if (not match_literal<"null">(ctx, it, end)) [[unlikely]] {
                  return;
               }
               if (not emit("null")) [[unlikely]] {
                  return;
               }
               break;
            }
            case Bool: {
               if (*it == 't') {
                  if (not match_literal<"true">(ctx, it, end)) [[unlikely]] {
                     return;
                  }
                  if (not emit("true")) [[unlikely]] {
                     return;
                  }
                  break;
               }
               else {
                  if (not match_literal<"false">(ctx, it, end)) [[unlikely]] {
                     return;
                  }
                  if (not emit("false")) [[unlikely]] {
                     return;
                  }
                  break;
               }
            }
            case Object_Start: {
               if (not emit('{')) [[unlikely]] {
                  return;
               }
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
                  if (it != end && *it != '}') {
                     if (not new_line()) [[unlikely]] {
                        return;
                     }
                  }
               }
               else {
                  if (*it != '}') {
                     if (not new_line()) [[unlikely]] {
                        return;
                     }
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
               // As in Array_End, this break doubles as the close of an open line comment.
               if (it[-1] != '{') {
                  if (not new_line()) [[unlikely]] {
                     return;
                  }
               }
               if (not emit('}')) [[unlikely]] {
                  return;
               }
               ++it;
               break;
            }
            case Comment: {
               if constexpr (Opts.comments) {
                  const auto comment = read_jsonc_comment(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  // non-empty: an empty view comes with an error
                  if (not emit(comment.text)) [[unlikely]] {
                     return;
                  }
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

      // Returns the number of bytes written, which is what a bounded output has no other way to
      // learn: it has no size to be shrunk to the result the way a resizable one does.
      template <auto Opts, contiguous In, output_buffer Out>
      inline size_t prettify_json(is_context auto&& ctx, In&& in, Out&& out)
      {
         if constexpr (resizable<Out>) {
            if (in.size() == 0) {
               out.resize(0); // resize is what `resizable` promises; clear() is not
               return 0;
            }
            out.resize(in.size() * 2);
         }
         size_t ix = 0;
         auto [it, end] = read_iterators<Opts>(in);
         if (bool(ctx.error)) [[unlikely]] {
            return 0;
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
         return ix;
      }
   }

   // The overloads that write into a caller's buffer report what went wrong, and how many bytes
   // they wrote, which is the only way a bounded output learns where its result ends. That count is
   // an offset into the output, not the input, so glz::format_error(ec, source) -- which reads it
   // as a position in the buffer it is handed -- does not point at the offending byte here. Not
   // [[nodiscard]]:
   // prettifying auto-generated JSON does not fail, so the callers that have always ignored the
   // outcome are right to, and warning at all of them would say nothing useful. The overloads that
   // return the text have nowhere to put an error, so they come back empty rather than hand over
   // the prefix the scan managed before it stopped -- routinely an unterminated string or an
   // unclosed brace, which is not a document but a silent truncation of one.
   //
   // Prettifying reports what it actually parses, which is strings, comments and literals. It does
   // not check that the document is structurally valid JSON -- `[1 2]` prettifies to `[12]` -- so a
   // document that may not be well formed wants glz::validate_json or glz::validate_jsonc.

   template <auto Opts = opts{}>
   inline error_ctx prettify_json(const auto& in, auto& out)
   {
      context ctx{};
      const auto n = detail::prettify_json<Opts>(ctx, in, out);
      return {n, ctx.error, ctx.custom_error_message};
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
      if (bool(ctx.error)) [[unlikely]] {
         return {}; // an incomplete result is not a document; see above
      }
      return out;
   }

   template <auto Opts = opts{}>
   inline error_ctx prettify_jsonc(const auto& in, auto& out)
   {
      context ctx{};
      const auto n = detail::prettify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return {n, ctx.error, ctx.custom_error_message};
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
      if (bool(ctx.error)) [[unlikely]] {
         return {}; // an incomplete result is not a document; see above
      }
      return out;
   }
}
