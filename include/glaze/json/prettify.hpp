// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Prettified JSONC preserves comments of both styles. A line comment always ends its line, so the
// following content is put on a new line rather than on the same one.

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

         // Whether the output cursor already sits at the start of a line.
         bool line_started = true;
         // A line comment has to be followed by a line break, or it would comment out whatever the
         // output put after it on the same line. The break is deferred to the next emission, so a
         // comment at the end of the input cannot leave a trailing newline behind: a flag still
         // pending when the input runs out is intended, not a break that went missing.
         bool line_comment_pending = false;

         // Line breaks are emitted where the formatter has always emitted them, whether or not the
         // cursor is already at the start of a line.
         auto break_line = [&] {
            append_new_line<use_tabs, indent_width>(b, ix, indent);
            line_started = true;
         };

         // Used for a break that a line comment deferred, which must not be doubled when the token
         // that follows has already started a line.
         auto end_line = [&] {
            if (not line_started) {
               append_new_line<use_tabs, indent_width>(b, ix, indent);
               line_started = true;
            }
         };

         auto emit = [&](auto&& value) {
            if (line_comment_pending) [[unlikely]] {
               line_comment_pending = false;
               end_line();
            }
            dump(value, b, ix);
            line_started = false;
         };

         while (it < end) {
            switch (json_types[uint8_t(*it)]) {
            case String: {
               const auto value = read_json_string<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               emit(value);
               break;
            }
            case Comma: {
               emit(',');
               ++it;
               if constexpr (check_new_lines_in_arrays(Opts)) {
                  break_line();
               }
               else {
                  if (state[indent] == Object_Start) {
                     break_line();
                  }
                  else {
                     if constexpr (use_tabs) {
                        emit('\t');
                     }
                     else {
                        emit(' ');
                     }
                  }
               }
               break;
            }
            case Number: {
               const auto value = read_json_number<Opts.null_terminated>(it, end);
               emit(value);
               break;
            }
            case Colon: {
               if constexpr (use_tabs) {
                  emit(":\t");
               }
               else {
                  emit(": ");
               }
               ++it;
               break;
            }
            case Array_Start: {
               emit('[');
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
                        break_line();
                     }
                  }
                  else {
                     if (*it != ']') {
                        break_line();
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
                     break_line();
                  }
               }
               emit(']');
               ++it;
               break;
            }
            case Null: {
               emit("null");
               it += 4;
               break;
            }
            case Bool: {
               if (*it == 't') {
                  emit("true");
                  it += 4;
                  break;
               }
               else {
                  emit("false");
                  it += 5;
                  break;
               }
            }
            case Object_Start: {
               emit('{');
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
                     break_line();
                  }
               }
               else {
                  if (*it != '}') {
                     break_line();
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
                  break_line();
               }
               emit('}');
               ++it;
               break;
            }
            case Comment: {
               if constexpr (Opts.comments) {
                  const auto value = read_jsonc_comment(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  emit(value);
                  if (not is_block_comment(value)) {
                     line_comment_pending = true;
                  }
                  break;
               }
               else {
                  [[fallthrough]];
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

         if constexpr (string_t<In>) {
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

   // These overloads drop the error because prettifying auto-generated JSON is not expected to fail.
   // The overloads taking a context report it instead, for input that may not be well formed, such as
   // a hand-written .jsonc file.
   //
   // Prettifying only reports what it actually parses, which is strings and comments. It does not
   // check that the document is structurally valid JSON, so use glz::validate_json for that.

   template <auto Opts = opts{}>
   inline void prettify_json(const auto& in, auto& out)
   {
      context ctx{};
      detail::prettify_json<Opts>(ctx, in, out);
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
   inline void prettify_jsonc(const auto& in, auto& out)
   {
      context ctx{};
      detail::prettify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
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

   /// Prettify, reporting failure through the returned error_ctx
   template <auto Opts = opts{}>
   [[nodiscard]] inline error_ctx prettify_json(context& ctx, const auto& in, auto& out)
   {
      detail::prettify_json<Opts>(ctx, in, out);
      return {0, ctx.error, ctx.custom_error_message};
   }

   /// Prettify JSONC, reporting failure through the returned error_ctx
   template <auto Opts = opts{}>
   [[nodiscard]] inline error_ctx prettify_jsonc(context& ctx, const auto& in, auto& out)
   {
      detail::prettify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return {0, ctx.error, ctx.custom_error_message};
   }
}
