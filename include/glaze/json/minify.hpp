// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Minifying JSONC handles both comment styles. A block comment is carried through as written; a
// line comment is dropped, because minifying is what removes the newline that ends it and without
// that newline the comment would swallow everything written after it.

#include "glaze/json/json_format.hpp"

namespace glz
{
   namespace detail
   {
      // We can use unchecked dumping to the output because we know minifying will not make the output any larger
      template <auto Opts>
      inline void minify_json(is_context auto&& ctx, auto&& it, auto&& end, auto&& b, auto& ix) noexcept
      {
         using enum json_type;

         auto ws_start = it;
         uint64_t ws_size{};

         auto skip_expected_whitespace = [&] {
            auto new_ws_start = it;
            if (ws_size && std::ptrdiff_t(ws_size) < end - it) [[likely]] {
               skip_matching_ws(ws_start, it, ws_size);
            }

            if constexpr (Opts.null_terminated) {
               while (whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
            else {
               while ((it < end) && whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
            ws_start = new_ws_start;
            ws_size = size_t(it - new_ws_start);
         };

         auto skip_whitespace = [&] {
            if constexpr (Opts.null_terminated) {
               while (whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
            else {
               while ((it < end) && whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
         };

         skip_whitespace();

         while ([&]() -> bool {
            if constexpr (Opts.null_terminated) {
               return true;
            }
            else {
               return it < end;
            }
         }()) {
            switch (json_types[uint8_t(*it)]) {
            case String: {
               const auto value = read_json_string(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               dump<false>(value, b, ix); // non-empty: the scanner reports an empty view as an error
               skip_whitespace();
               break;
            }
            case Comma: {
               dump<false>(',', b, ix);
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Number: {
               const auto value = read_json_number<Opts.null_terminated>(it, end);
               dump<false>(value, b, ix); // we couldn't have gotten here without one valid character
               skip_whitespace();
               break;
            }
            case Colon: {
               dump<false>(':', b, ix);
               ++it;
               skip_whitespace();
               break;
            }
            case Array_Start: {
               dump<false>('[', b, ix);
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Array_End: {
               dump<false>(']', b, ix);
               ++it;
               skip_whitespace();
               break;
            }
            case Null: {
               // The type table matched on the first byte alone, so the rest of the literal still
               // has to be in the buffer, and it has to actually spell the literal. Stepping over
               // it regardless puts `it` past `end`, and from there the loop goes on minifying
               // whatever follows the document into an output sized for the document. Dumping it
               // regardless invents the bytes that were not there: {"a":nul } came out as
               // {"a":null}, a well-formed document the input never said.
               if (not match_literal<"null">(ctx, it, end)) [[unlikely]] {
                  return;
               }
               dump<false>("null", b, ix);
               skip_whitespace();
               break;
            }
            case Bool: {
               if (*it == 't') {
                  if (not match_literal<"true">(ctx, it, end)) [[unlikely]] {
                     return;
                  }
                  dump<false>("true", b, ix);
                  skip_whitespace();
                  break;
               }
               else {
                  if (not match_literal<"false">(ctx, it, end)) [[unlikely]] {
                     return;
                  }
                  dump<false>("false", b, ix);
                  skip_whitespace();
                  break;
               }
            }
            case Object_Start: {
               dump<false>('{', b, ix);
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Object_End: {
               dump<false>('}', b, ix);
               ++it;
               skip_whitespace();
               break;
            }
            case Comment: {
               if constexpr (Opts.comments) {
                  const auto comment = read_jsonc_comment(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  // A line comment is dropped rather than written out. Minifying is what removes
                  // the newline that ends it, and a line comment with no line break behind it
                  // comments out the whole rest of the output.
                  if (not comment.line) {
                     dump<false>(comment.text, b, ix);
                  }
                  skip_whitespace();
                  break;
               }
               else {
                  [[fallthrough]];
               }
            }
            [[unlikely]] default: {
               // A null terminated run carries no bound in its loop condition, so it ends here, on
               // the terminator, which is not a JSON token. Reaching it is the end of the document
               // rather than an error -- unclassified is how the type table reports both, and
               // without this the minifier left syntax_error behind on every input it was ever
               // given, valid or not, which is what made its error not worth returning.
               if constexpr (Opts.null_terminated) {
                  if (it >= end) {
                     return;
                  }
               }
               ctx.error = error_code::syntax_error;
               return;
            }
            }
         }
      }

      // Returns the number of bytes written, which is what a bounded output has no other way to
      // learn: it has no size to be shrunk to the result the way a resizable one does.
      template <auto Opts, class In, output_buffer Out>
         requires(contiguous<In> && resizable<In>)
      inline size_t minify_json(is_context auto&& ctx, In&& in, Out&& out)
      {
         if (in.size() == 0) {
            return 0;
         }

         if constexpr (resizable<Out>) {
            out.resize(in.size() + 2 * padding_bytes);
         }
         else {
            // Minifying only ever removes bytes, which is what lets the scan below dump into the
            // output without checking room for each token. A bounded output has to be told when
            // the input is more than it holds, rather than being written past the end of.
            if (not ensure_space(ctx, out, in.size())) [[unlikely]] {
               return 0;
            }
         }
         size_t ix = 0;
         auto [it, end] = read_iterators<Opts>(in);
         if (bool(ctx.error)) [[unlikely]] {
            return 0;
         }

         // The input is no longer grown and shrunk around this: everything the scan below reads
         // through -- read_json_string, read_json_number, skip_matching_ws -- bounds its own loads
         // against `end`, so there is nothing left for the padding to protect.
         if constexpr (self_terminating<In>) {
            minify_json<opt_true<Opts, &opts::null_terminated>>(ctx, it, end, out, ix);
         }
         else {
            minify_json<opt_false<Opts, &opts::null_terminated>>(ctx, it, end, out, ix);
         }

         if constexpr (resizable<Out>) {
            out.resize(ix);
         }
         return ix;
      }
   }

   // The overloads that write into a caller's buffer report what went wrong, and how many bytes
   // they wrote, which is the only way a bounded output learns where its result ends. Not
   // [[nodiscard]]:
   // minifying auto-generated JSON does not fail, so the callers that have always ignored the
   // outcome are right to, and warning at all of them would say nothing useful. The overloads that
   // return the text have nowhere to put an error and stay silent.
   //
   // Minifying reports what it actually parses, which is strings, comments and literals. It does
   // not check that the document is structurally valid JSON -- `[1 2]` minifies to `[12]` -- so a
   // document that may not be well formed wants glz::validate_json or glz::validate_jsonc.

   template <auto Opts = opts{}>
   inline error_ctx minify_json(resizable auto& in, auto& out)
   {
      context ctx{};
      const auto n = detail::minify_json<Opts>(ctx, in, out);
      return {n, ctx.error, ctx.custom_error_message};
   }

   template <auto Opts = opts{}>
   inline std::string minify_json(resizable auto& in)
   {
      context ctx{};
      std::string out{};
      detail::minify_json<Opts>(ctx, in, out);
      return out;
   }

   template <auto Opts = opts{}>
   inline error_ctx minify_jsonc(resizable auto& in, auto& out)
   {
      context ctx{};
      const auto n = detail::minify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return {n, ctx.error, ctx.custom_error_message};
   }

   template <auto Opts = opts{}>
   inline std::string minify_jsonc(resizable auto& in)
   {
      context ctx{};
      std::string out{};
      detail::minify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return out;
   }
}
