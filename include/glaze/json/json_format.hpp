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

   // Reads a JSON string, returning the view including both quotes. Every scanner in this header
   // reports failure through `ctx` and returns an empty view alongside it, so an empty view is
   // never returned without an error that explains it.
   template <auto Opts>
      requires(check_is_padded(Opts))
   sv read_json_string(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      auto start = it;
      ++it; // skip quote
      while (it < end) [[likely]] {
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

      ctx.error = error_code::unexpected_end;
      return {};
   }

   template <auto Opts>
      requires(!check_is_padded(Opts))
   sv read_json_string(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      auto start = it;
      ++it; // skip quote
      // The scan reads eight bytes at a time, so it only runs while at least that many remain. The
      // end marker below is otherwise formed before the start of the buffer, which is undefined.
      const auto chunk_end = (size_t(end - it) > 7) ? end - 7 : it;
      for (; it < chunk_end;) {
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

      // Tail end of buffer. Should be rare we even get here
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

   // A comment view returned by read_jsonc_comment starts with its delimiter, so this tells the two
   // styles apart. It matters because a line comment's terminating newline is significant: minifying
   // removes it, and prettifying has to put it back.
   inline constexpr bool is_block_comment(const sv comment) noexcept
   {
      return comment.size() > 1 && comment[1] == '*';
   }

   // Reads either JSONC comment style, returning the comment text including its delimiters:
   //    // line comment      terminated by a newline, or by the end of the buffer
   //    /* block comment */  terminated by the closing delimiter
   // An empty view is only ever returned alongside an error:
   //    error_code::expected_end_comment  a block comment that is never closed, or a '/' that opens
   //                                      no comment
   //    error_code::unexpected_end        a '/' as the last byte of the buffer
   inline sv read_jsonc_comment(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      auto start = it;
      ++it; // skip the opening '/'

      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return {};
      }

      if (*it == '/') {
         ++it; // skip the second '/'
         // The line terminator that ends a line comment is not part of it, and the end of the buffer
         // ends it just as a terminator would. A carriage return ends it on its own, so that a file
         // with CR line endings does not have the rest of the document swallowed; a following line
         // feed is then ordinary whitespace.
         while (it < end && *it != '\n' && *it != '\r') {
            ++it;
         }
         return {start, size_t(it - start)};
      }

      if (*it != '*') [[unlikely]] {
         ctx.error = error_code::expected_end_comment;
         return {};
      }
      ++it; // skip the opening '*'

      // Only a '*' after the opening delimiter can close the comment, hence the `it > content`
      // guards: without them a comment such as "/" "*" "/" would look closed by the '*' of its own
      // opening delimiter and swallow whatever followed.
      const auto content = start + 2;
      // As above: the chunked scan needs eight bytes to read, and the marker must stay inside the
      // buffer for input such as "/*/" that is shorter than that.
      const auto chunk_end = (size_t(end - it) > 7) ? end - 7 : it;
      for (; it < chunk_end;) {
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
               return {start, size_t(it - start)};
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
            return {start, size_t(it - start)};
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
