// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/parse.hpp"

namespace glz::detail
{
   enum struct json_type : char {
      Unset = 'x',
      String = '"',
      Comma = ',',
      Number = '-',
      Colon = ':',
      Array_Start = '[',
      Array_End = ']',
      Null = 'n',
      Bool = 't',
      Object_Start = '{',
      Object_End = '}',
      Comment = '/',
      Whitespace = ' '
   };

   inline constexpr std::array<json_type, 256> json_types = [] {
      std::array<json_type, 256> t{};
      using enum json_type;
      t['"'] = String;
      t[','] = Comma;
      t['0'] = Number;
      t['1'] = Number;
      t['2'] = Number;
      t['3'] = Number;
      t['4'] = Number;
      t['5'] = Number;
      t['6'] = Number;
      t['7'] = Number;
      t['8'] = Number;
      t['9'] = Number;
      t['-'] = Number;
      t[':'] = Colon;
      t['['] = Array_Start;
      t[']'] = Array_End;
      t['n'] = Null;
      t['t'] = Bool;
      t['f'] = Bool;
      t['{'] = Object_Start;
      t['}'] = Object_End;
      t['/'] = Comment;
      t[' '] = Whitespace;
      t['\t'] = Whitespace;
      t['\n'] = Whitespace;
      t['\r'] = Whitespace;
      return t;
   }();

   template <bool use_tabs, uint8_t indentation_width>
   inline void append_new_line(auto&& b, auto&& ix, const int64_t indent)
   {
      dump('\n', b, ix);
      if constexpr (use_tabs) {
         dumpn('\t', indent, b, ix);
      }
      else {
         dumpn(' ', indent * indentation_width, b, ix);
      }
   };

   // Steps `it` over one of JSON's three literals, which the type table above matched on its first
   // byte alone.
   //
   // The formatters write the literal from their own spelling rather than copying it out of the
   // input, so without checking the rest of it they invent the bytes that were not there:
   // {"a":tru } minified to {"a":true}, a well-formed document that the input never said. Reports
   // unexpected_end for an input that stops inside the literal and syntax_error for one that spells
   // something else.
   template <string_literal literal>
   GLZ_ALWAYS_INLINE bool match_literal(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr auto s = literal.sv();
      if (end - it < std::ptrdiff_t(s.size())) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return false;
      }
      if (not comparitor<s>(it)) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return false;
      }
      it += s.size();
      return true;
   }

   // Returns the quoted span starting at `it`, including both quotes.
   //
   // Chunked while eight bytes remain, then a byte at a time. The chunk reads past the character it
   // is looking at, so it has to stop short of the end of the buffer; the tail covers whatever is
   // left. A padded caller would find the tail unreachable, which is not worth a second copy of
   // this for minify and prettify.
   //
   // An empty view comes back only alongside `ctx.error`, so a caller that checks the error is
   // never left deciding what an empty view meant. It used to mean either "an empty string" or
   // "the buffer ran out", and both writers guessed the first: minify dropped everything after an
   // unterminated string and prettify handed the view to a memcpy that read from nullptr.
   sv read_json_string(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      auto start = it;
      ++it; // skip quote
      // The bound as a pointer, so each chunk costs one compare. Where eight bytes are not left,
      // the fallback is the opening quote, which sits before `it` and fails the test on the first
      // look; never `it` itself, which would pass and read a chunk past `end`.
      const auto* const chunk_limit = (end - it >= 8) ? end - 8 : it - 1;
      while (it <= chunk_limit) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const uint64_t quote = has_quote(chunk);
         if (quote) {
            it += (countr_zero(quote) >> 3);

            auto* prev = it - 1;
            while (*prev == '\\') {
               --prev;
            }
            if (size_t(it - prev) % 2) {
               ++it; // add quote
               return {start, size_t(it - start)};
            }
            ++it; // skip escaped quote and continue
         }
         else {
            it += 8;
         }
      }

      while (it < end) {
         if (*it == '"') {
            auto* prev = it - 1;
            while (*prev == '\\') {
               --prev;
            }
            if (size_t(it - prev) % 2) {
               ++it; // add quote
               return {start, size_t(it - start)};
            }
         }
         ++it;
      }

      ctx.error = error_code::unexpected_end;
      return {};
   }

   // What read_jsonc_comment found.
   struct jsonc_comment
   {
      // The comment as written, delimiters included. Empty only alongside `ctx.error`.
      sv text{};
      // A `//` comment. Its terminating line break is not part of `text`, which is what makes the
      // two styles behave differently in the writers: minifying is what removes that break, so the
      // comment cannot be carried into minified output, and prettifying has to emit one of its own.
      bool line{};
   };

   // Reads either JSONC comment style, `it` pointing at the opening '/':
   //
   //    // line comment      ends at a line terminator, which is left behind, or at the end of the
   //                         buffer, which ends it just as well
   //    /* block comment */  ends at the closing delimiter
   //
   // Reports expected_end_comment for a block comment that is never closed and for a '/' that opens
   // no comment at all, and unexpected_end for a '/' that is the last byte of the buffer. As with
   // read_json_string above, an empty view never comes back without an error to explain it.
   inline jsonc_comment read_jsonc_comment(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      const auto start = it;
      ++it; // past the opening '/'

      // The caller matched one '/'; that says nothing about a second byte being there at all.
      // Stepping over both regardless leaves `it` past `end`, and every scan the caller runs after
      // this one starts from there.
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return {};
      }

      if (*it == '/') {
         ++it; // past the second '/'
         while (it != end && *it != '\n' && *it != '\r') {
            ++it;
         }
         // The terminator is left where it is: minify drops it along with the rest of the
         // whitespace and prettify emits its own break. A carriage return ends the comment on its
         // own, so a file with CR line endings is not swallowed by its first comment; a line feed
         // behind it is then ordinary whitespace.
         return {sv{start, size_t(it - start)}, true};
      }

      if (*it != '*') [[unlikely]] {
         ctx.error = error_code::expected_end_comment;
         return {};
      }
      ++it; // past the '*' of the opener

      // Only a '*' after the opening delimiter can close the comment, which is what the `content`
      // guards below are for: without them the '*' of the opener closes the comment it opened, so
      // "/*/" reads as complete and swallows whatever follows it.
      const auto content = it;

      // The bound as a pointer, so each chunk costs one compare. `end - 8` only points into the
      // buffer once that much of it is left; where it is not, the fallback is the byte before the
      // cursor, which is inside the opener and so fails the test on the first look. Never `it`
      // itself -- that passes, and the chunk behind it would read past the end of the buffer.
      const auto* const chunk_limit = (end - it >= 8) ? end - 8 : it - 1;
      while (it <= chunk_limit) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const uint64_t slash = has_char<'/'>(chunk);
         if (slash) {
            it += (countr_zero(slash) >> 3);

            if (it > content && it[-1] == '*') {
               ++it; // add slash
               return {sv{start, size_t(it - start)}, false};
            }
            // skip slash and continue
            ++it;
         }
         else {
            it += 8;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         if (it > content && it[-1] == '*' && *it == '/') {
            ++it; // add slash
            return {sv{start, size_t(it - start)}, false};
         }
         ++it;
      }

      ctx.error = error_code::expected_end_comment;
      return {};
   }

   template <bool null_terminated>
   GLZ_ALWAYS_INLINE sv read_json_number(auto&& it, auto end) noexcept
   {
      auto start = it;
      if constexpr (null_terminated) {
         while (numeric_table[uint8_t(*it)]) {
            ++it;
         }
      }
      else {
         while ((it < end) && numeric_table[uint8_t(*it)]) {
            ++it;
         }
      }
      return {start, size_t(it - start)};
   }
}
