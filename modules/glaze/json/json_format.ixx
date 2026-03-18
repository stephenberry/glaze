// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.json.json_format;

import glaze.core.common;
import glaze.core.opts;

import glaze.util.dump;
import glaze.util.parse;
import glaze.util.string_literal;

import std;

#include "glaze/util/inline.hpp"

export namespace glz::detail
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

   template <bool use_tabs, std::uint8_t indentation_width>
   inline void append_new_line(auto&& b, auto&& ix, const std::int64_t indent)
   {
      dump('\n', b, ix);
      if constexpr (use_tabs) {
         dumpn('\t', indent, b, ix);
      }
      else {
         dumpn(' ', indent * indentation_width, b, ix);
      }
   };

   template <auto Opts>
      requires(check_is_padded(Opts))
   sv read_json_string(auto&& it, auto end) noexcept
   {
      auto start = it;
      ++it; // skip quote
      while (it < end) [[likely]] {
         std::uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const std::uint64_t quote = has_quote(chunk);
         if (quote) {
            it += (std::countr_zero(quote) >> 3);

            auto* prev = it - 1;
            while (*prev == '\\') {
               --prev;
            }
            if (std::size_t(it - prev) % 2) {
               ++it; // add quote
               return {start, std::size_t(it - start)};
            }
            ++it; // skip escaped quote and continue
         }
         else {
            it += 8;
         }
      }

      return {};
   }

   template <auto Opts>
      requires(!check_is_padded(Opts))
   sv read_json_string(auto&& it, auto end) noexcept
   {
      auto start = it;
      ++it; // skip quote
      for (const auto end_m7 = end - 7; it < end_m7;) {
         std::uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const std::uint64_t quote = has_quote(chunk);
         if (quote) {
            it += (std::countr_zero(quote) >> 3);

            auto* prev = it - 1;
            while (*prev == '\\') {
               --prev;
            }
            if (std::size_t(it - prev) % 2) {
               ++it; // add quote
               return {start, std::size_t(it - start)};
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
            if (std::size_t(it - prev) % 2) {
               ++it; // add quote
               return {start, std::size_t(it - start)};
            }
         }
         ++it;
      }

      return {};
   }

   // Reads /* my comment */ style comments
   inline sv read_jsonc_comment(auto&& it, auto end) noexcept
   {
      auto start = it;
      it += 2; // skip /*
      for (const auto end_m7 = end - 7; it < end_m7;) {
         std::uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const std::uint64_t slash = has_char<'/'>(chunk);
         if (slash) {
            it += (std::countr_zero(slash) >> 3);

            if (it[-1] == '*') {
               ++it; // add slash
               return {start, std::size_t(it - start)};
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
         if (it[-1] == '*' && *it == '/') {
            ++it; // add slash
            return {start, std::size_t(it - start)};
         }
         ++it;
      }

      return {};
   }

   template <bool null_terminated>
   GLZ_ALWAYS_INLINE sv read_json_number(auto&& it, auto end) noexcept
   {
      auto start = it;
      if constexpr (null_terminated) {
         while (numeric_table[std::uint8_t(*it)]) {
            ++it;
         }
      }
      else {
         while ((it < end) && numeric_table[std::uint8_t(*it)]) {
            ++it;
         }
      }
      return {start, std::size_t(it - start)};
   }
}
