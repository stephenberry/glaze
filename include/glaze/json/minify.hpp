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
      template <auto Opts>
      inline void minify_json(is_context auto&& ctx, auto&& it, auto&& end, auto&& b, auto& ix) noexcept
      {
         using enum json_type;

         // Every write below goes through this, and the dump it makes is unchecked: the caller's
         // one-shot sizing covers a resizable destination, because minifying only ever removes
         // bytes, so the input's length is room enough.
         //
         // A bounded destination cannot be grown, and asking it for the input's length up front
         // refuses a buffer that fits the result -- sizing the output to the minified length is the
         // obvious thing to do, and `{ "a" : 1 }` into a 7 byte buffer has to work. So each write
         // asks for exactly what it stores instead; see emit_bytes for where that count comes from.
         // None of it survives for a resizable destination, which keeps the unchecked dumps and
         // nothing more.
         const auto emit = [&](const auto& x) { return emit_bytes<false>(ctx, b, ix, x); };

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
               // non-empty: the scanner reports an empty view as an error
               if (not emit(value)) [[unlikely]] {
                  return;
               }
               skip_whitespace();
               break;
            }
            case Comma: {
               if (not emit(',')) [[unlikely]] {
                  return;
               }
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Number: {
               const auto value = read_json_number<Opts.null_terminated>(it, end);
               // non-empty: a Number match is one valid character
               if (not emit(value)) [[unlikely]] {
                  return;
               }
               skip_whitespace();
               break;
            }
            case Colon: {
               if (not emit(':')) [[unlikely]] {
                  return;
               }
               ++it;
               skip_whitespace();
               break;
            }
            case Array_Start: {
               if (not emit('[')) [[unlikely]] {
                  return;
               }
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Array_End: {
               if (not emit(']')) [[unlikely]] {
                  return;
               }
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
               if (not emit("null")) [[unlikely]] {
                  return;
               }
               skip_whitespace();
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
                  skip_whitespace();
                  break;
               }
               else {
                  if (not match_literal<"false">(ctx, it, end)) [[unlikely]] {
                     return;
                  }
                  if (not emit("false")) [[unlikely]] {
                     return;
                  }
                  skip_whitespace();
                  break;
               }
            }
            case Object_Start: {
               if (not emit('{')) [[unlikely]] {
                  return;
               }
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Object_End: {
               if (not emit('}')) [[unlikely]] {
                  return;
               }
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
                     // non-empty: an empty view comes with an error
                     if (not emit(comment.text)) [[unlikely]] {
                        return;
                     }
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
            // Minifying only ever removes bytes, so this is room enough for whatever comes out.
            // A bounded output is checked per write instead; see the scan above for why.
            out.resize(in.size() + 2 * padding_bytes);
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
   // they wrote, which is the only way a bounded output learns where its result ends. That count is
   // an offset into the output, not the input, so glz::format_error(ec, source) -- which reads it
   // as a position in the buffer it is handed -- does not point at the offending byte here. Not
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
      if (bool(ctx.error)) [[unlikely]] {
         // Nothing here can report the failure, and the prefix written so far is not a document:
         // it is whatever the scan managed before it stopped, routinely an unterminated string or
         // an unclosed brace. Handing that back is the silent truncation this whole change is
         // about, one layer up, so it comes back empty instead.
         return {};
      }
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
      if (bool(ctx.error)) [[unlikely]] {
         // Nothing here can report the failure, and the prefix written so far is not a document:
         // it is whatever the scan managed before it stopped, routinely an unterminated string or
         // an unclosed brace. Handing that back is the silent truncation this whole change is
         // about, one layer up, so it comes back empty instead.
         return {};
      }
      return out;
   }
}
