// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "glaze/core/context.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/simd/structural.hpp"
#include "glaze/simd/utf8_validation.hpp"
#include "glaze/util/atoi.hpp"
#include "glaze/util/bit.hpp"
#include "glaze/util/compare.hpp"
#include "glaze/util/convert.hpp"
#include "glaze/util/expected.hpp"
#include "glaze/util/inline.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   // How many bytes must remain before `end` for a `Width` byte load at the cursor to stay inside
   // the buffer.
   //
   // The readers scan in fixed width chunks, and bounding every chunk against `end` exactly would
   // mean a compare per byte rather than per chunk. Instead each loop runs while
   // `end - it >= chunk_min<Width>(padded)` and finishes whatever is left a byte at a time. An
   // input carrying `padding_bytes` of readable slack past `end` can let the last chunk straddle
   // the end of the document, so its answer is 1 -- the loop condition collapses to `it < end` and
   // the byte tail is unreachable. An input without that slack answers `Width`, which costs the
   // final chunk of the buffer its fast path and nothing else.
   //
   // Stated as a count rather than a limit pointer on purpose: `end - Width` is undefined when the
   // buffer is shorter than a chunk, which is exactly the case a caller reaching for this has.
   //
   // Signed, and compared against a signed `end - it`, because a scanner that lands on an escaped
   // quote can step past `end` before the next test. An unsigned difference wraps there and reads
   // the whole address space as "room left".
   template <size_t Width>
   GLZ_ALWAYS_INLINE constexpr std::ptrdiff_t chunk_min(const bool padded) noexcept
   {
      static_assert(Width <= padding_bytes);
      return padded ? 1 : std::ptrdiff_t(Width);
   }

   inline constexpr std::array<bool, 256> numeric_table = [] {
      std::array<bool, 256> t{};
      t['0'] = true;
      t['1'] = true;
      t['2'] = true;
      t['3'] = true;
      t['4'] = true;
      t['5'] = true;
      t['6'] = true;
      t['7'] = true;
      t['8'] = true;
      t['9'] = true;
      t['.'] = true;
      t['+'] = true;
      t['-'] = true;
      t['e'] = true;
      t['E'] = true;
      return t;
   }();

   inline constexpr std::array<char, 256> char_unescape_table = [] {
      std::array<char, 256> t{};
      t['"'] = '"';
      t['/'] = '/';
      t['\\'] = '\\';
      t['b'] = '\b';
      t['f'] = '\f';
      t['n'] = '\n';
      t['r'] = '\r';
      t['t'] = '\t';
      return t;
   }();

   inline constexpr std::array<bool, 256> valid_escape_table = [] {
      std::array<bool, 256> t{};
      t['"'] = true;
      t['/'] = true;
      t['\\'] = true;
      t['b'] = true;
      t['f'] = true;
      t['n'] = true;
      t['r'] = true;
      t['t'] = true;
      t['u'] = true;
      return t;
   }();

   inline constexpr std::array<bool, 256> whitespace_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\t'] = true;
      t['\r'] = true;
      t[' '] = true;
      return t;
   }();

   // Whitespace plus JSON separators (comma, colon)
   inline constexpr std::array<bool, 256> whitespace_separator_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\t'] = true;
      t['\r'] = true;
      t[' '] = true;
      t[','] = true;
      t[':'] = true;
      return t;
   }();

   // Character classification for lazy JSON value skipping
   enum class lazy_char_type : uint8_t {
      other = 0, // whitespace, separators, literals - just advance
      quote = 1, // " - skip string
      open = 2, // { or [ - increase depth
      close = 3, // } or ] - decrease depth
      number = 4 // - or 0-9 - skip number
   };

   inline constexpr std::array<lazy_char_type, 256> lazy_char_class = [] {
      using enum lazy_char_type;
      std::array<lazy_char_type, 256> t{};
      t['"'] = quote;
      t['{'] = open;
      t['['] = open;
      t['}'] = close;
      t[']'] = close;
      t['-'] = number;
      t['0'] = number;
      t['1'] = number;
      t['2'] = number;
      t['3'] = number;
      t['4'] = number;
      t['5'] = number;
      t['6'] = number;
      t['7'] = number;
      t['8'] = number;
      t['9'] = number;
      return t;
   }();

   inline constexpr std::array<bool, 256> whitespace_comment_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\t'] = true;
      t['\r'] = true;
      t[' '] = true;
      t['/'] = true;
      return t;
   }();

   inline constexpr std::array<uint8_t, 256> digit_hex_table = [] {
      std::array<uint8_t, 256> t;
      std::fill(t.begin(), t.end(), uint8_t(255));
      t['0'] = 0;
      t['1'] = 1;
      t['2'] = 2;
      t['3'] = 3;
      t['4'] = 4;
      t['5'] = 5;
      t['6'] = 6;
      t['7'] = 7;
      t['8'] = 8;
      t['9'] = 9;
      t['a'] = 0xA;
      t['b'] = 0xB;
      t['c'] = 0xC;
      t['d'] = 0xD;
      t['e'] = 0xE;
      t['f'] = 0xF;
      t['A'] = 0xA;
      t['B'] = 0xB;
      t['C'] = 0xC;
      t['D'] = 0xD;
      t['E'] = 0xE;
      t['F'] = 0xF;
      return t;
   }();

   inline constexpr std::array<uint16_t, 256> char_escape_table = [] {
      // Build uint16_t so that memcpy produces chars in correct order.
      // On LE: chars[0] in low byte, chars[1] in high byte -> memcpy writes [chars[0]][chars[1]]
      // On BE: chars[0] in high byte, chars[1] in low byte -> memcpy writes [chars[0]][chars[1]]
      auto combine = [](const char chars[2]) -> uint16_t {
         if constexpr (std::endian::native == std::endian::big) {
            return (uint16_t(uint8_t(chars[0])) << 8) | uint16_t(uint8_t(chars[1]));
         }
         else {
            return uint16_t(uint8_t(chars[0])) | (uint16_t(uint8_t(chars[1])) << 8);
         }
      };

      std::array<uint16_t, 256> t{};
      t['\b'] = combine(R"(\b)");
      t['\t'] = combine(R"(\t)");
      t['\n'] = combine(R"(\n)");
      t['\f'] = combine(R"(\f)");
      t['\r'] = combine(R"(\r)");
      t['\"'] = combine(R"(\")");
      t['\\'] = combine(R"(\\)");
      return t;
   }();

#if defined(__SIZEOF_INT128__)
   consteval __uint128_t repeat_byte16(const uint8_t repeat)
   {
      __uint128_t multiplier = (__uint128_t(0x0101010101010101ull) << 64) | 0x0101010101010101ull;
      return multiplier * repeat;
   }
#endif

   consteval uint64_t not_repeat_byte8(const uint8_t repeat) { return ~(0x0101010101010101ull * repeat); }

   [[nodiscard]] GLZ_ALWAYS_INLINE uint32_t hex_to_u32(const char* c) noexcept
   {
      constexpr auto& t = digit_hex_table;
      const uint8_t arr[4]{t[uint8_t(c[3])], t[uint8_t(c[2])], t[uint8_t(c[1])], t[uint8_t(c[0])]};
      auto chunk = std::bit_cast<uint32_t>(arr);
      // On big-endian, bit_cast produces bytes in opposite order than expected
      // byteswap to get consistent little-endian representation
      if constexpr (std::endian::native == std::endian::big) {
         chunk = std::byteswap(chunk);
      }
      // check that all hex characters are valid
      if (chunk & repeat_byte4(0b11110000u)) [[unlikely]] {
         return 0xFFFFFFFFu;
      }

      // now pack into first four bytes of uint32_t
      uint32_t packed{};
      packed |= (chunk & 0x0000000F);
      packed |= (chunk & 0x00000F00) >> 4;
      packed |= (chunk & 0x000F0000) >> 8;
      packed |= (chunk & 0x0F000000) >> 12;
      return packed;
   }

   template <class Char>
   [[nodiscard]] GLZ_ALWAYS_INLINE uint32_t code_point_to_utf8(const uint32_t code_point, Char* c) noexcept
   {
      if (code_point <= 0x7F) {
         c[0] = Char(code_point);
         return 1;
      }
      if (code_point <= 0x7FF) {
         c[0] = Char(0xC0 | ((code_point >> 6) & 0x1F));
         c[1] = Char(0x80 | (code_point & 0x3F));
         return 2;
      }
      if (code_point <= 0xFFFF) {
         c[0] = Char(0xE0 | ((code_point >> 12) & 0x0F));
         c[1] = Char(0x80 | ((code_point >> 6) & 0x3F));
         c[2] = Char(0x80 | (code_point & 0x3F));
         return 3;
      }
      if (code_point <= 0x10FFFF) {
         c[0] = Char(0xF0 | ((code_point >> 18) & 0x07));
         c[1] = Char(0x80 | ((code_point >> 12) & 0x3F));
         c[2] = Char(0x80 | ((code_point >> 6) & 0x3F));
         c[3] = Char(0x80 | (code_point & 0x3F));
         return 4;
      }
      return 0;
   }

   [[nodiscard]] GLZ_ALWAYS_INLINE uint32_t skip_code_point(const uint32_t code_point) noexcept
   {
      if (code_point <= 0x7F) {
         return 1;
      }
      if (code_point <= 0x7FF) {
         return 2;
      }
      if (code_point <= 0xFFFF) {
         return 3;
      }
      if (code_point <= 0x10FFFF) {
         return 4;
      }
      return 0;
   }

   namespace unicode
   {
      inline constexpr uint32_t generic_surrogate_mask = 0xF800;
      inline constexpr uint32_t generic_surrogate_value = 0xD800;

      inline constexpr uint32_t surrogate_mask = 0xFC00;
      inline constexpr uint32_t high_surrogate_value = 0xD800;
      inline constexpr uint32_t low_surrogate_value = 0xDC00;

      inline constexpr uint32_t surrogate_codepoint_offset = 0x10000;
      inline constexpr uint32_t surrogate_codepoint_mask = 0x03FF;
      inline constexpr uint32_t surrogate_codepoint_bits = 10;
   }

   // What a \uXXXX escape produced, or why it produced nothing.
   //
   // `truncated` separates "the characters this escape needs are past the end of the buffer" from
   // "this escape is malformed", which the caller reports differently: the first says the document
   // was cut and more input would settle it, which is what an incremental reader has to hear, while
   // the second is a document that will never parse. Deliberately not convertible to bool, so that
   // a caller cannot test it without deciding which of the two it is looking at.
   struct unicode_result
   {
      uint32_t written{}; // UTF-8 bytes written to dst, zero unless the escape was valid
      bool truncated{};
   };

   // Whether what is left of the buffer could still grow into the escape being read.
   //
   // Reached only when too few bytes remain to finish one, which on its own does not say the
   // document was cut: `"\uD83D"` is a whole document holding a lone surrogate, and what follows
   // the escape is the closing quote rather than the second half of a pair. A caller told the
   // input ran out goes looking for more, and no more of it settles that document. So the bytes
   // that are there decide -- a clean prefix is a truncation, anything already contradicting the
   // escape is malformed.
   template <bool ExpectOpener>
   [[nodiscard]] GLZ_ALWAYS_INLINE bool escape_prefix_intact(const auto* it, const auto* end) noexcept
   {
      auto n = size_t(end - it);
      if constexpr (ExpectOpener) {
         if (n > 0 && it[0] != '\\') return false;
         if (n > 1 && it[1] != 'u') return false;
         if (n < 2) return true;
         it += 2;
         n -= 2;
      }
      for (size_t i = 0; i < n; ++i) {
         if (digit_hex_table[uint8_t(it[i])] == 255) return false;
      }
      return true;
   }

   template <class SrcChar, class DstChar = SrcChar>
   [[nodiscard]] GLZ_ALWAYS_INLINE unicode_result handle_unicode_code_point(const SrcChar*& it, DstChar*& dst,
                                                                            const SrcChar* end) noexcept
   {
      using namespace unicode;

      if (end - it <= 4) [[unlikely]] {
         return {0, escape_prefix_intact<false>(it, end)};
      }
      const uint32_t high = hex_to_u32(it);
      if (high == 0xFFFFFFFFu) [[unlikely]] {
         return {};
      }
      it += 4; // skip the code point characters

      uint32_t code_point;

      if ((high & generic_surrogate_mask) == generic_surrogate_value) {
         // surrogate pair code points
         if ((high & surrogate_mask) != high_surrogate_value) {
            return {};
         }

         if (end - it <= 6) [[unlikely]] {
            return {0, escape_prefix_intact<true>(it, end)};
         }
         // The next two characters must be `\u`
         uint16_t u;
         std::memcpy(&u, it, 2);
         if (u != to_uint16_t(R"(\u)")) [[unlikely]] {
            return {};
         }
         it += 2;
         // verify that second unicode escape sequence is present
         const uint32_t low = hex_to_u32(it);
         if (low == 0xFFFFFFFFu) [[unlikely]] {
            return {};
         }
         it += 4;

         if ((low & surrogate_mask) != low_surrogate_value) [[unlikely]] {
            return {};
         }

         code_point = (high & surrogate_codepoint_mask) << surrogate_codepoint_bits;
         code_point |= (low & surrogate_codepoint_mask);
         code_point += surrogate_codepoint_offset;
      }
      else {
         code_point = high;
      }
      const uint32_t offset = code_point_to_utf8(code_point, dst);
      dst += offset;
      return {offset, false};
   }

   template <class Char>
   [[nodiscard]] GLZ_ALWAYS_INLINE bool skip_unicode_code_point(const Char*& it, const Char* end) noexcept
   {
      using namespace unicode;
      if (it + 4 >= end) [[unlikely]] {
         return false;
      }

      const uint32_t high = hex_to_u32(it);
      if (high == 0xFFFFFFFFu) [[unlikely]] {
         return false;
      }
      it += 4; // skip the code point characters

      uint32_t code_point;

      if ((high & generic_surrogate_mask) == generic_surrogate_value) {
         // surrogate pair code points
         if ((high & surrogate_mask) != high_surrogate_value) [[unlikely]] {
            return false;
         }

         if (it + 6 >= end) [[unlikely]] {
            return false;
         }
         // The next two characters must be `\u`
         uint16_t u;
         std::memcpy(&u, it, 2);
         if (u != to_uint16_t(R"(\u)")) [[unlikely]] {
            return false;
         }
         it += 2;
         // verify that second unicode escape sequence is present
         const uint32_t low = hex_to_u32(it);
         if (low == 0xFFFFFFFFu) [[unlikely]] {
            return false;
         }
         it += 4;

         if ((low & surrogate_mask) != low_surrogate_value) [[unlikely]] {
            return false;
         }

         code_point = (high & surrogate_codepoint_mask) << surrogate_codepoint_bits;
         code_point |= (low & surrogate_codepoint_mask);
         code_point += surrogate_codepoint_offset;
      }
      else {
         code_point = high;
      }
      return skip_code_point(code_point) > 0;
   }

   // Options struct for match_invalid_end - reduces template instantiations
   struct match_invalid_end_opts
   {
      bool null_terminated;

      // Convert from any opts-like type
      template <typename T>
      consteval match_invalid_end_opts(const T& opts) noexcept : null_terminated{opts.null_terminated}
      {}

      // Direct construction
      explicit consteval match_invalid_end_opts(bool null_terminated_) noexcept : null_terminated{null_terminated_} {}
   };

   // Checks for a character and validates that we are not at the end (considered an error)
   template <char C, match_invalid_end_opts Opts>
   GLZ_ALWAYS_INLINE bool match_invalid_end(is_context auto& ctx, auto&& it, auto end) noexcept
   {
      if (*it != C) [[unlikely]] {
         if constexpr (C == '"') {
            ctx.error = error_code::expected_quote;
         }
         else if constexpr (C == ',') {
            ctx.error = error_code::expected_comma;
         }
         else if constexpr (C == ':') {
            ctx.error = error_code::expected_colon;
         }
         else if constexpr (C == '[' || C == ']') {
            ctx.error = error_code::expected_bracket;
         }
         else if constexpr (C == '{' || C == '}') {
            ctx.error = error_code::expected_brace;
         }
         else {
            ctx.error = error_code::syntax_error;
         }
         return true;
      }
      else [[likely]] {
         ++it;
      }
      if constexpr (not Opts.null_terminated) {
         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return true;
         }
      }
      return false;
   }

   template <char C>
   GLZ_ALWAYS_INLINE bool match(is_context auto& ctx, auto&& it) noexcept
   {
      if (*it != C) [[unlikely]] {
         if constexpr (C == '"') {
            ctx.error = error_code::expected_quote;
         }
         else if constexpr (C == ',') {
            ctx.error = error_code::expected_comma;
         }
         else if constexpr (C == ':') {
            ctx.error = error_code::expected_colon;
         }
         else if constexpr (C == '[' || C == ']') {
            ctx.error = error_code::expected_bracket;
         }
         else if constexpr (C == '{' || C == '}') {
            ctx.error = error_code::expected_brace;
         }
         else {
            ctx.error = error_code::syntax_error;
         }
         return true;
      }
      else [[likely]] {
         ++it;
         return false;
      }
   }

   // Always bounded. A padded input could skip the length test, but it is one predicted compare
   // against a value already in a register, next to a literal compare that has to happen anyway --
   // far too little to be worth a second instantiation of every reader that matches a keyword.
   template <string_literal str, auto Opts>
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr auto S = str.sv();
      if ((end - it < std::ptrdiff_t(str.size())) || not comparitor<S>(it)) [[unlikely]] {
         ctx.error = error_code::syntax_error;
      }
      else [[likely]] {
         it += str.size();
      }
   }

   GLZ_ALWAYS_INLINE void skip_comment(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      ++it;
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
      }
      else if (*it == '/') {
         while (++it != end && *it != '\n');
      }
      else if (*it == '*') {
         while (++it != end) {
            if (*it == '*') [[unlikely]] {
               if (++it == end) [[unlikely]]
                  break;
               else if (*it == '/') [[likely]] {
                  ++it;
                  break;
               }
            }
         }
      }
      else [[unlikely]] {
         ctx.error = error_code::expected_end_comment;
      }
   }

   // Advance to the first byte equal to any of Chars, or to end if there is none.
   //
   // Eight bytes at a time via the has_char SWAR test above, which is the same technique the
   // skip_until_closed loops use inline; this is the bounded, reusable form for callers that
   // have an explicit end rather than a padded buffer. Never reads past end, so it is safe on
   // buffers that are neither padded nor null terminated.
   template <char... Chars>
      requires(sizeof...(Chars) > 0)
   GLZ_ALWAYS_INLINE const char* find_first_of(const char* p, const char* const end) noexcept
   {
      // end - p rather than p + 8 <= end: the latter forms a pointer past one-past-the-end for
      // short ranges, and is diagnosable UB on a null range. Pointer difference is well defined
      // for both, including two null pointers.
      while (end - p >= 8) {
         uint64_t chunk;
         std::memcpy(&chunk, p, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         // has_char marks the low bit-group of each matching byte, so the first match is the
         // lowest set bit regardless of how many bytes in the chunk match.
         const uint64_t test = (has_char<Chars>(chunk) | ...);
         if (test) {
            return p + (countr_zero(test) >> 3);
         }
         p += 8;
      }
      while (p < end && not((*p == Chars) || ...)) {
         ++p;
      }
      return p;
   }

   GLZ_ALWAYS_INLINE constexpr uint64_t is_less_32(const uint64_t chunk) noexcept
   {
      return has_zero(chunk & repeat_byte8(0b11100000u));
   }

   GLZ_ALWAYS_INLINE constexpr uint64_t is_greater_15(const uint64_t chunk) noexcept
   {
      return (chunk & repeat_byte8(0b11110000u));
   }
}

namespace glz
{
   // Options struct for skip_ws - reduces template instantiations
   struct ws_opts
   {
      bool minified;
      bool null_terminated;
      bool comments;

      // Convert from any opts-like type
      template <typename T>
      consteval ws_opts(const T& opts) noexcept
         : minified{opts.minified}, null_terminated{opts.null_terminated}, comments{opts.comments}
      {}

      // Direct construction - all values required
      consteval ws_opts(bool minified_, bool null_terminated_, bool comments_) noexcept
         : minified{minified_}, null_terminated{null_terminated_}, comments{comments_}
      {}
   };

   // skip whitespace
   template <ws_opts Opts>
   GLZ_ALWAYS_INLINE bool skip_ws(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      using namespace glz::detail;

      if constexpr (not Opts.minified) {
         if constexpr (Opts.null_terminated) {
            if constexpr (Opts.comments) {
               while (whitespace_comment_table[uint8_t(*it)]) {
                  if (*it == '/') [[unlikely]] {
                     skip_comment(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return true;
                     }
                  }
                  else [[likely]] {
                     ++it;
                  }
               }
            }
            else {
               while (whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
         }
         else {
            if constexpr (Opts.comments) {
               while (it < end && whitespace_comment_table[uint8_t(*it)]) {
                  if (*it == '/') [[unlikely]] {
                     skip_comment(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return true;
                     }
                  }
                  else [[likely]] {
                     ++it;
                  }
               }
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::end_reached;
                  return true;
               }
            }
            else {
               while (it < end && whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::end_reached;
                  return true;
               }
            }
         }
      }
      else if constexpr (not Opts.null_terminated) {
         // Minified input has no whitespace to skip, but a non-null-terminated buffer can still
         // be exhausted at this point. The non-minified branch above performs the same check; a
         // null-terminated buffer relies on the trailing sentinel and keeps this a pure no-op.
         if (it == end) [[unlikely]] {
            ctx.error = error_code::end_reached;
            return true;
         }
      }

      return false;
   }

   GLZ_ALWAYS_INLINE void skip_matching_ws(const auto* ws, auto&& it, uint64_t length) noexcept
   {
      if (length > 7) {
         uint64_t v[2];
         while (length > 8) {
            std::memcpy(v, ws, 8);
            std::memcpy(v + 1, it, 8);
            if (v[0] != v[1]) {
               return;
            }
            length -= 8;
            ws += 8;
            it += 8;
         }

         const auto shift = 8 - length;
         ws -= shift;
         it -= shift;

         std::memcpy(v, ws, 8);
         std::memcpy(v + 1, it, 8);
         if (v[0] != v[1]) {
            return;
         }
         it += 8;
         return;
      }
      {
         constexpr uint64_t n{sizeof(uint32_t)};
         if (length >= n) {
            uint32_t v[2];
            std::memcpy(v, ws, n);
            std::memcpy(v + 1, it, n);
            if (v[0] != v[1]) {
               return;
            }
            length -= n;
            ws += n;
            it += n;
         }
      }
      {
         constexpr uint64_t n{sizeof(uint16_t)};
         if (length >= n) {
            uint16_t v[2];
            std::memcpy(v, ws, n);
            std::memcpy(v + 1, it, n);
            if (v[0] != v[1]) {
               return;
            }
            // length -= n;
            // ws += n;
            it += n;
         }
      }
      // We have to call a whitespace check after this function
      // in case the whitespace is mismatching.
      // So, we forgo this check as to not duplicate.
      /*if (length && *ws == *it) {
         ++it;
      }*/
   }

   inline bool validate_utf8_scalar(const uint8_t* it, const uint8_t* end) noexcept
   {
      while (it < end) {
         // Optimistic SWAR check for ASCII
         if (it + 8 <= end) {
            uint64_t chunk;
            std::memcpy(&chunk, it, 8);
            if ((chunk & glz::repeat_byte8(0x80)) == 0) {
               it += 8;
               continue;
            }
         }

         // Byte-by-byte validation (standard conformant)
         uint8_t byte = *it;

         if (byte < 0x80) {
            it++;
         }
         else if ((byte & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (it + 2 > end || (it[1] & 0xC0) != 0x80) return false;
            if (byte < 0xC2) return false; // Overlong
            it += 2;
         }
         else if ((byte & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (it + 3 > end || (it[1] & 0xC0) != 0x80 || (it[2] & 0xC0) != 0x80) return false;
            if (byte == 0xE0 && it[1] < 0xA0) return false; // Overlong
            if (byte == 0xED && it[1] >= 0xA0) return false; // Surrogate
            it += 3;
         }
         else if ((byte & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (it + 4 > end || (it[1] & 0xC0) != 0x80 || (it[2] & 0xC0) != 0x80 || (it[3] & 0xC0) != 0x80)
               return false;
            if (byte == 0xF0 && it[1] < 0x90) return false; // Overlong
            if (byte == 0xF4 && it[1] >= 0x90) return false; // > U+10FFFF
            if (byte > 0xF4) return false; // > U+10FFFF
            it += 4;
         }
         else {
            return false;
         }
      }

      return true;
   }

   inline bool validate_utf8(const auto* str, const size_t size) noexcept
   {
      const uint8_t* it = reinterpret_cast<const uint8_t*>(str);
      const uint8_t* const end = it + size;
#if defined(GLZ_UTF8_SIMD)
      // 16 is a measured compromise, not a register-size coincidence. The scalar path skips ASCII 8
      // bytes at a time, so on ASCII it stays ahead of the vector path's fixed setup cost until the
      // input is long enough for the 64 byte ASCII step to engage; on non-ASCII the vector path
      // wins from roughly 8 bytes. Raising the threshold buys a few percent on short ASCII and
      // costs up to 2.3x on mid length non-ASCII strings, so it stays low.
      if (size >= 16) {
         return detail::utf8_simd::validate(it, end);
      }
#endif
      return validate_utf8_scalar(it, end);
   }

   // Validates the raw bytes of a JSON string. RFC 8259 section 8.1 requires JSON text to be UTF-8,
   // and read input is by definition someone else's, so this is on by default; the inheritable
   // `validate_utf8` option turns it off. Only the raw span needs checking: escape sequences are
   // ASCII, and handle_unicode_code_point independently rejects unpaired surrogates in \uXXXX
   // escapes.
   //
   // ascii_acc lets a caller skip the pass entirely for pure ASCII strings, which is the common
   // case. Scan loops already load the string in 8 byte chunks to find the closing quote, so they
   // OR those chunks together for free and pass the result here. The accumulator may cover more
   // bytes than the string itself (a chunk can overrun the closing quote); that only costs a
   // needless validation pass, it never skips one. Callers with no accumulator take the default
   // and always validate.
   template <auto Opts>
   GLZ_ALWAYS_INLINE bool validate_utf8_span(is_context auto&& ctx, const auto* start, const auto* fin,
                                             const uint64_t ascii_acc = repeat_byte8(0b10000000)) noexcept
   {
      if constexpr (not check_validate_utf8(Opts)) {
         // Everything the caller computed for us is dead, which lets its scan loop drop the
         // accumulator entirely.
         (void)ctx, (void)start, (void)fin, (void)ascii_acc;
         return false;
      }
      else {
         if ((ascii_acc & repeat_byte8(0b10000000)) == 0) {
            return false; // pure ASCII is trivially well formed UTF-8
         }
         if (!validate_utf8(start, size_t(fin - start))) [[unlikely]] {
            ctx.error = error_code::invalid_utf8;
            return true;
         }
         return false;
      }
   }

   GLZ_ALWAYS_INLINE void skip_till_quote(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      const auto* pc = std::memchr(it, '"', size_t(end - it));
      if (pc) [[likely]] {
         it = reinterpret_cast<std::decay_t<decltype(it)>>(pc);
         return;
      }

      ctx.error = error_code::expected_quote;
   }

   // Advances to a string's closing quote, the opening quote having already been consumed.
   //
   // ascii_acc receives the OR of every string byte walked, which lets the caller hand the result
   // to validate_utf8_span and skip the UTF-8 pass outright when the span is pure ASCII. That is
   // why this scans inline rather than calling memchr: memchr reports only where the quote is, so
   // every caller had to make a second pass over bytes this one has already loaded.
   GLZ_ALWAYS_INLINE void skip_string_view(is_context auto&& ctx, auto&& it, auto end, uint64_t& ascii_acc) noexcept
   {
      // The bound as a pointer, so each chunk costs one compare rather than a subtract and a
      // compare. `end - 8` is only a pointer into the buffer once eight bytes are left; where they
      // are not, the fallback is the opening quote, which sits before `it` and so fails the test on
      // the first look. Never `it` itself -- that passes, and the chunk behind it would read past
      // `end`. Reading eight at a time never passes `end`, so this is safe on buffers that are
      // neither padded nor null terminated.
      const auto* const chunk_limit = (end - it >= 8) ? end - 8 : it - 1;
      while (it <= chunk_limit) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const uint64_t test = has_quote(chunk);
         if (test) [[unlikely]] {
            const size_t offset = size_t(countr_zero(test)) >> 3;
            // Only bytes up to and including the quote belong to the string. Masking the rest off
            // keeps a non-ASCII byte in whatever follows from forcing a needless validation pass.
            ascii_acc |= chunk & (~uint64_t(0) >> (56 - 8 * offset));
            it += offset;
            auto* prev = it - 1;
            while (*prev == '\\') {
               --prev;
            }
            if (size_t(it - prev) % 2) {
               return;
            }
            ++it; // skip the escaped quote
         }
         else {
            ascii_acc |= chunk;
            it += 8;
         }
      }

      // Fewer than eight bytes left in the buffer, so finish one at a time.
      while (it < end) {
         ascii_acc |= uint64_t(uint8_t(*it));
         if (*it == '"') {
            auto* prev = it - 1;
            while (*prev == '\\') {
               --prev;
            }
            if (size_t(it - prev) % 2) {
               return;
            }
         }
         ++it;
      }

      ctx.error = error_code::expected_quote;
   }

   GLZ_ALWAYS_INLINE void skip_string_view(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      uint64_t ascii_acc{};
      skip_string_view(ctx, it, end, ascii_acc);
   }

   // Options struct for skip_string - reduces template instantiations
   struct skip_string_opts
   {
      bool opening_handled;
      bool validate_skipped;
      bool validate_utf8;
      bool null_terminated;

      // Convert from any opts-like type (consteval because check_* functions are consteval)
      template <typename T>
      consteval skip_string_opts(const T& opts) noexcept
         : opening_handled{check_opening_handled(opts)},
           validate_skipped{check_validate_skipped(opts)},
           validate_utf8{check_validate_utf8(opts)},
           null_terminated{check_null_terminated(opts)}
      {}

      // Direct construction - all values required. No defaults on purpose: a caller that forgets
      // validate_utf8_ would silently validate against the user's explicit choice to turn it off,
      // and a caller that forgets null_terminated_ would silently read past the end of a bounded
      // buffer. Both are invisible at the call site, so make omission a compile error instead.
      consteval skip_string_opts(bool opening_handled_, bool validate_skipped_, bool validate_utf8_,
                                 bool null_terminated_) noexcept
         : opening_handled{opening_handled_},
           validate_skipped{validate_skipped_},
           validate_utf8{validate_utf8_},
           null_terminated{null_terminated_}
      {}
   };

   // Skips over the body of a string, leaving `it` just past its closing quote.
   //
   // Chunked while eight bytes remain and a byte at a time after that. The chunk test flags quotes,
   // backslashes and control characters together, so a chunk it clears is eight bytes that need no
   // decision at all; what it cannot cover is the end of the buffer, where an eight byte load would
   // reach past it. A padded input has room for that load and never reaches the tail.
   //
   // The handover is always on a character boundary: the chunked loop advances either by a whole
   // clear chunk or to just past a character it has finished with, never into the middle of an
   // escape.
   template <skip_string_opts Opts>
   GLZ_ALWAYS_INLINE void skip_string(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      if constexpr (not Opts.opening_handled) {
         ++it;
      }

      const auto* const utf8_start = it;

      if constexpr (Opts.validate_skipped) {
         // The bound as a pointer, so each chunk costs one compare. `end - scan_min` is only a
         // pointer into the buffer once at least that much of it is left, and a string can start
         // within a few bytes of the end; where it is not, the fallback is the opening quote, which
         // sits before `it` and so fails the test on the first look. Never `it` itself -- that
         // passes, and the chunk behind it would read past the end of the buffer.
         const std::ptrdiff_t scan_min = chunk_min<8>(ctx.padded_input);
         const auto* const chunk_limit = (end - it >= scan_min) ? end - scan_min : it - 1;

         while (it <= chunk_limit) {
            uint64_t swar;
            std::memcpy(&swar, it, 8);
            if constexpr (std::endian::native == std::endian::big) {
               swar = std::byteswap(swar);
            }

            constexpr uint64_t lo7_mask = repeat_byte8(0b01111111);
            const uint64_t lo7 = swar & lo7_mask;
            const uint64_t backslash = (lo7 ^ repeat_byte8('\\')) + lo7_mask;
            const uint64_t quote = (lo7 ^ repeat_byte8('"')) + lo7_mask;
            const uint64_t less_32 = (swar & repeat_byte8(0b01100000)) + lo7_mask;
            uint64_t next = ~((backslash & quote & less_32) | swar);
            next &= repeat_byte8(0b10000000);

            if (next == 0) {
               // No special characters in this chunk
               it += 8;
               continue;
            }

            // Find the first occurrence
            size_t offset = (countr_zero(next) >> 3);
            it += offset;

            const auto c = *it;
            if ((c & 0b11100000) == 0) [[unlikely]] {
               // Invalid control character (<0x20)
               ctx.error = error_code::syntax_error;
               return;
            }
            else if (c == '"') {
               // Check if this quote is escaped
               const auto* p = it - 1;
               int backslash_count{};
               // We don't have to worry about rewinding too far because we started with a quote
               while (*p == '\\') {
                  ++backslash_count;
                  --p;
               }
               if ((backslash_count & 1) == 0) {
                  // Even number of backslashes => not escaped => closing quote found
                  validate_utf8_span<Opts>(ctx, utf8_start, it);
                  ++it;
                  return;
               }
               else {
                  // Odd number of backslashes => escaped quote
                  ++it;
                  continue;
               }
            }
            else if (c == '\\') {
               // Handle escape sequence
               ++it;

               // The backslash can be the last byte the chunk covers, which lands `it` on `end`.
               // A null-terminated buffer answers that read with its terminator, which the escape
               // table below rejects; a bounded one has nothing there to read.
               if constexpr (not Opts.null_terminated) {
                  if (it == end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
               }

               if (*it == 'u') {
                  ++it;
                  if (not skip_unicode_code_point(it, end)) [[unlikely]] {
                     ctx.error = error_code::unicode_escape_conversion_failure;
                     return;
                  }
               }
               else {
                  if (not char_unescape_table[uint8_t(*it)]) [[unlikely]] {
                     ctx.error = error_code::invalid_escape;
                     return;
                  }
                  ++it;
               }
            }
         }

         while (true) {
            // A null-terminated buffer terminates on the trailing '\0' (caught by the control
            // character check below); a non-null-terminated buffer has no sentinel, so bound the
            // scan before each dereference.
            if constexpr (not Opts.null_terminated) {
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }
            if ((*it & 0b11100000) == 0) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            switch (*it) {
            case '"': {
               validate_utf8_span<Opts>(ctx, utf8_start, it);
               ++it;
               return;
            }
            case '\\': {
               ++it;
               if constexpr (not Opts.null_terminated) {
                  if (it == end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
               }
               if (char_unescape_table[uint8_t(*it)]) {
                  ++it;
                  continue;
               }
               else if (*it == 'u') {
                  ++it;
                  if (skip_unicode_code_point(it, end)) [[likely]] {
                     continue;
                  }
                  else [[unlikely]] {
                     ctx.error = error_code::unicode_escape_conversion_failure;
                     return;
                  }
               }
               // Same code the chunked loop above reports. Both loops run over one buffer now, so
               // an escape that lands in the last few bytes must be diagnosed the same as one that
               // does not.
               ctx.error = error_code::invalid_escape;
               return;
            }
            }
            ++it;
         }
      }
      else {
         uint64_t ascii_acc{};
         skip_string_view(ctx, it, end, ascii_acc);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         if (validate_utf8_span<Opts>(ctx, utf8_start, it, ascii_acc)) [[unlikely]] {
            return;
         }
         ++it; // skip the quote
      }
   }

   // Options struct for skip_until_closed - reduces template instantiations
   struct skip_until_closed_opts
   {
      bool comments;
      bool validate_utf8;

      // Convert from any opts-like type (consteval because check_validate_utf8 is consteval)
      template <typename T>
      consteval skip_until_closed_opts(const T& opts) noexcept
         : comments{opts.comments}, validate_utf8{check_validate_utf8(opts)}
      {}

      // Direct construction - all values required
      consteval skip_until_closed_opts(bool comments_, bool validate_utf8_) noexcept
         : comments{comments_}, validate_utf8{validate_utf8_}
      {}
   };

#if defined(GLZ_STRUCTURAL_SIMD)
   // Prefix xor: bit i becomes the xor of bits 0..i. Applied to a mask of unescaped quotes it
   // yields the bytes that sit inside a string -- opening quote included, closing quote excluded --
   // because a byte is inside exactly when an odd number of quotes precede it.
   GLZ_ALWAYS_INLINE constexpr uint64_t prefix_xor(uint64_t x) noexcept
   {
      x ^= x << 1;
      x ^= x << 2;
      x ^= x << 4;
      x ^= x << 8;
      x ^= x << 16;
      x ^= x << 32;
      return x;
   }

   // Marks the bytes a backslash escapes. Only backslashes that start an odd numbered run escape
   // anything, so the run parity is what the arithmetic below computes; `carry` hands a run that
   // straddles the window boundary to the next call (1 when the next window opens on an escaped
   // byte). This is the derivation from Langdale & Lemire's "Parsing Gigabytes of JSON per Second".
   GLZ_ALWAYS_INLINE uint64_t escaped_mask(uint64_t backslash, uint64_t& carry) noexcept
   {
      if (backslash == 0) {
         const uint64_t escaped = carry;
         carry = 0;
         return escaped;
      }
      backslash &= ~carry;
      const uint64_t follows_escape = (backslash << 1) | carry;
      constexpr uint64_t even_bits = 0x5555555555555555ull;
      const uint64_t odd_starts = backslash & ~even_bits & ~follows_escape;
      const uint64_t even_sequences = odd_starts + backslash;
      carry = uint64_t(even_sequences < odd_starts); // unsigned wraparound is the run crossing over
      return (even_bits ^ (even_sequences << 1)) & follows_escape;
   }

   // Settles whole 64 byte windows of a value being skipped, without looking at individual bytes.
   //
   // Returns true when the close matching the already consumed open was found, leaving `it` just
   // past it. Returns false when the caller's byte-at-a-time scan has to take over, leaving `it`
   // at a position that is not inside a string and `depth` correct for it: either a window
   // boundary, or the opening quote of a string the window pass declined to finish.
   //
   // It declines on any window holding a non-ASCII byte, because skipping still validates the
   // UTF-8 of the strings it passes over (see skip_string) and these masks say nothing about
   // encoding. Rewinding to the open quote costs one redundant window at most and keeps that
   // validation exactly where it was.
   template <skip_until_closed_opts Opts, char Open, char Close>
   GLZ_ALWAYS_INLINE bool skip_windows(auto&& it, auto end, size_t& depth) noexcept
   {
      uint64_t escape_carry{};
      uint64_t in_string_carry{}; // all ones while the next window opens inside a string
      auto string_open = it; // opening quote of the string still open, when there is one

      while (end - it >= 64) {
         const auto w = detail::structural::load_window(it, Open, Close);

         if constexpr (Opts.validate_utf8) {
            if (w.non_ascii) [[unlikely]] {
               if (in_string_carry) {
                  it = string_open;
               }
               return false;
            }
         }

         const uint64_t escaped = escaped_mask(w.backslash, escape_carry);
         const uint64_t quote = w.quote & ~escaped;
         const uint64_t in_string = prefix_xor(quote) ^ in_string_carry;

         // A backslash outside a string cannot occur in JSON, and the parity above takes it at
         // face value: it cancels the next quote, and every byte after that is on the wrong side
         // of the string boundary for the rest of the document. The byte scan reaches a backslash
         // only through a string, so it does not make that mistake, and a malformed document must
         // not be read one way here and another way on a target without these masks. Hand it over
         // instead. The test is exact where it has to be -- at the first stray backslash nothing
         // has diverged yet, so `in_string` is still right about it -- and anything it flags later
         // only costs a window.
         if (w.backslash & ~in_string) [[unlikely]] {
            if (in_string_carry) {
               it = string_open;
            }
            return false;
         }

         in_string_carry = uint64_t(int64_t(in_string) >> 63); // sign extend the last byte's state
         if (in_string_carry && quote) {
            // Ending inside a string means the window's last unescaped quote opened it.
            string_open = it + (63 - std::countl_zero(quote));
         }

         const uint64_t opens = w.open & ~in_string;
         const uint64_t closes = w.close & ~in_string;

         // Every close in the window drops the depth by one at most, so a depth above that count
         // cannot reach zero anywhere inside it and the net change is all that matters.
         if (depth > size_t(std::popcount(closes))) {
            depth += size_t(std::popcount(opens));
            depth -= size_t(std::popcount(closes));
            it += 64;
            continue;
         }

         uint64_t brackets = opens | closes;
         while (brackets) {
            const auto i = countr_zero(brackets);
            brackets &= brackets - 1;
            if ((closes >> i) & 1) {
               --depth;
               if (depth == 0) {
                  it += i + 1;
                  return true;
               }
            }
            else {
               ++depth;
            }
         }
         it += 64;
      }

      if (in_string_carry) {
         it = string_open;
      }
      return false;
   }
#endif

   template <skip_until_closed_opts Opts, char open, char close, size_t Depth = 1>
      requires(not Opts.comments)
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr bool opening_not_handled = false;
      static constexpr bool skip_validation = false;
      // skip_validation routes skip_string through the end-bounded skip_string_view path, which
      // stops on `end` rather than on a sentinel, so this flag has no effect from here.
      static constexpr bool null_terminated_unused = true;

      size_t depth = Depth;

#if defined(GLZ_STRUCTURAL_SIMD)
      if (skip_windows<Opts, open, close>(it, end, depth)) {
         return;
      }
#endif

      // Chunked while eight bytes remain, then a byte at a time; a padded input has room for the
      // load that straddles the end of the document and leaves the tail below unreachable. The
      // bound is a pointer so each chunk costs one compare rather than a subtract and a compare,
      // and is only formed once that much buffer is left for it to point into. Where it is not, the
      // fallback is the opening bracket, which sits before `it` and so fails the test on the first
      // look. Never `it` itself -- that passes, and the chunk behind it would read past `end`.
      const std::ptrdiff_t scan_min = chunk_min<8>(ctx.padded_input);
      const auto* const chunk_limit = (end - it >= scan_min) ? end - scan_min : it - 1;

      while (it <= chunk_limit) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const uint64_t test = has_quote(chunk) | has_char<open>(chunk) | has_char<close>(chunk);
         if (test) {
            it += (countr_zero(test) >> 3);

            switch (*it) {
            case '"': {
               skip_string<skip_string_opts{opening_not_handled, skip_validation, Opts.validate_utf8,
                                            null_terminated_unused}>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               break;
            }
            case open: {
               ++it;
               ++depth;
               break;
            }
            case close: {
               ++it;
               --depth;
               if (depth == 0) {
                  return;
               }
               break;
            }
            default: {
               ctx.error = error_code::unexpected_end;
               return;
            }
            }
         }
         else {
            it += 8;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         switch (*it) {
         case '"': {
            skip_string<skip_string_opts{opening_not_handled, skip_validation, Opts.validate_utf8,
                                         null_terminated_unused}>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            break;
         }
         case open: {
            ++it;
            ++depth;
            break;
         }
         case close: {
            ++it;
            --depth;
            if (depth == 0) {
               return;
            }
            break;
         }
         default: {
            ++it;
         }
         }
      }

      ctx.error = error_code::unexpected_end;
   }

   template <skip_until_closed_opts Opts, char open, char close, size_t Depth = 1>
      requires(Opts.comments)
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr bool opening_not_handled = false;
      static constexpr bool skip_validation = false;
      // skip_validation routes skip_string through the end-bounded skip_string_view path, which
      // stops on `end` rather than on a sentinel, so this flag has no effect from here.
      static constexpr bool null_terminated_unused = true;

      size_t depth = Depth;

      // Chunked while eight bytes remain, then a byte at a time; a padded input has room for the
      // load that straddles the end of the document and leaves the tail below unreachable. The
      // bound is a pointer so each chunk costs one compare rather than a subtract and a compare,
      // and is only formed once that much buffer is left for it to point into. Where it is not, the
      // fallback is the opening bracket, which sits before `it` and so fails the test on the first
      // look. Never `it` itself -- that passes, and the chunk behind it would read past `end`.
      const std::ptrdiff_t scan_min = chunk_min<8>(ctx.padded_input);
      const auto* const chunk_limit = (end - it >= scan_min) ? end - scan_min : it - 1;

      while (it <= chunk_limit) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         if constexpr (std::endian::native == std::endian::big) {
            chunk = std::byteswap(chunk);
         }
         const uint64_t test = has_quote(chunk) | has_char<'/'>(chunk) | has_char<open>(chunk) | has_char<close>(chunk);
         if (test) {
            it += (countr_zero(test) >> 3);

            switch (*it) {
            case '"': {
               skip_string<skip_string_opts{opening_not_handled, skip_validation, Opts.validate_utf8,
                                            null_terminated_unused}>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               break;
            }
            case '/': {
               skip_comment(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               break;
            }
            case open: {
               ++it;
               ++depth;
               break;
            }
            case close: {
               ++it;
               --depth;
               if (depth == 0) {
                  return;
               }
               break;
            }
            default: {
               ctx.error = error_code::unexpected_end;
               return;
            }
            }
         }
         else {
            it += 8;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         switch (*it) {
         case '"': {
            skip_string<skip_string_opts{opening_not_handled, skip_validation, Opts.validate_utf8,
                                         null_terminated_unused}>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            break;
         }
         case '/': {
            skip_comment(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            break;
         }
         case open: {
            ++it;
            ++depth;
            break;
         }
         case close: {
            ++it;
            --depth;
            if (depth == 0) {
               return;
            }
            break;
         }
         default: {
            ++it;
         }
         }
      }

      ctx.error = error_code::unexpected_end;
   }

   // Parses a JSON unsigned integer, returning nullopt if `s` does not start with one or if the
   // value is out of range for uint64_t. Trailing content is ignored: "12abc" reads as 12.
   //
   // `s` must be null terminated. Scanning stops on the first non-digit rather than at s.size(), so
   // without a terminator there is nothing to halt it inside the view: a std::string_view over a
   // bare character array reads past its end, and a view of the first few digits of a longer run
   // consumes the rest of them. String literals and std::string satisfy this; a subview of a larger
   // buffer does not, unless the character just past it is a non-digit.
   inline constexpr std::optional<uint64_t> stoui(const std::string_view s) noexcept
   {
      if (s.empty()) {
         return {};
      }

      uint64_t ret;
      auto* c = s.data();
      bool valid = detail::stoui64(ret, c);
      if (valid) {
         return ret;
      }
      return {};
   }

   GLZ_ALWAYS_INLINE void skip_number_with_validation(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      // Every standalone *it read below is guarded by it != end so the scan stays inside the
      // buffer for non-null-terminated input (the std::find_if_not calls are already bounded by
      // end). A null-terminated buffer is unaffected: it reaches end only once the number is
      // fully consumed, where these guards short-circuit exactly as the trailing '\0' sentinel
      // would have.
      it += (it != end) && (*it == '-');
      const auto sig_start_it = it;
      auto frac_start_it = end;
      if (it != end && *it == '0') {
         ++it;
         // RFC 8259 section 6: number = [ minus ] int [ frac ] [ exp ]. The exponent may follow
         // the integer part directly, with no fractional part in between (e.g. "0e4").
         if (it != end && (*it | ('E' ^ 'e')) == 'e') {
            ++it;
            goto exp_start;
         }
         if (it == end || *it != '.') {
            return;
         }
         ++it;
         goto frac_start;
      }
      it = std::find_if_not(it, end, is_digit);
      if (it == sig_start_it) {
         ctx.error = error_code::syntax_error;
         return;
      }
      if (it != end && (*it | ('E' ^ 'e')) == 'e') {
         ++it;
         goto exp_start;
      }
      if (it == end || *it != '.') return;
      ++it;
   frac_start:
      frac_start_it = it;
      it = std::find_if_not(it, end, is_digit);
      if (it == frac_start_it) {
         ctx.error = error_code::syntax_error;
         return;
      }
      if (it == end || (*it | ('E' ^ 'e')) != 'e') return;
      ++it;
   exp_start:
      it += (it != end) && (*it == '+' || *it == '-');
      const auto exp_start_it = it;
      it = std::find_if_not(it, end, is_digit);
      if (it == exp_start_it) {
         ctx.error = error_code::syntax_error;
         return;
      }
   }

   // Options struct for skip_number - reduces template instantiations
   struct skip_number_opts
   {
      bool validate;
      bool null_terminated;

      // Convert from any opts-like type (consteval because check_* functions are consteval)
      template <typename T>
      consteval skip_number_opts(const T& opts) noexcept
         : validate{check_validate_skipped(opts)}, null_terminated{check_null_terminated(opts)}
      {}

      // Direct construction
      explicit consteval skip_number_opts(bool validate_, bool null_terminated_ = true) noexcept
         : validate{validate_}, null_terminated{null_terminated_}
      {}
   };

   template <skip_number_opts Opts>
   GLZ_ALWAYS_INLINE void skip_number(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      if constexpr (not Opts.validate) {
         if constexpr (Opts.null_terminated) {
            // Relies on the trailing '\0' sentinel (numeric_table['\0'] == false) to terminate.
            while (numeric_table[uint8_t(*it)]) {
               ++it;
            }
         }
         else {
            while (it < end && numeric_table[uint8_t(*it)]) {
               ++it;
            }
         }
      }
      else {
         skip_number_with_validation(ctx, it, end);
      }
   }

   // expects opening whitespace to be handled
   GLZ_ALWAYS_INLINE sv parse_key(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      // TODO this assumes no escapes.
      if (bool(ctx.error)) [[unlikely]]
         return {};

      if (match<'"'>(ctx, it)) {
         return {};
      }
      auto start = it;
      skip_till_quote(ctx, it, end);
      if (bool(ctx.error)) [[unlikely]]
         return {};
      return sv{start, static_cast<size_t>(it++ - start)};
   }

   template <size_t multiple>
   GLZ_ALWAYS_INLINE constexpr auto round_up_to_multiple(const std::integral auto val) noexcept
   {
      return val + (multiple - (val % multiple)) % multiple;
   }

   struct utf8_stream_validator
   {
      GLZ_ALWAYS_INLINE void reset() noexcept
      {
         remaining_ = 0;
         lower_bound_ = 0x80;
         upper_bound_ = 0xBF;
         valid_ = true;
      }

      GLZ_ALWAYS_INLINE bool consume(const auto* str, const size_t size) noexcept
      {
         if (!valid_) [[unlikely]] {
            return false;
         }

         if (size == 0) {
            return true;
         }

         const uint8_t* it = reinterpret_cast<const uint8_t*>(str);
         const uint8_t* end = it + size;

         // Copy state to locals to avoid intermediate stores to 'this' in the hot loop.
         // When uint8_t == unsigned char, '*it' may alias the object representation of
         // this validator, so the compiler cannot reliably move direct member updates
         // out of the loop on its own since loop reads '*it' between writes
         uint32_t remaining = remaining_;
         uint32_t lower_bound = lower_bound_;
         uint32_t upper_bound = upper_bound_;
         const bool had_pending = remaining != 0;

         // First finish a codepoint that was saved from the previous .consume()
         if (remaining != 0) [[unlikely]] {
            if (!consume_pending(it, end, remaining, lower_bound, upper_bound)) {
               return fail();
            }

            if (it == end) {
               store_pending(remaining, lower_bound, upper_bound);
               return true;
            }
         }

         // Small chunks probably won't benefit much from the wider bulk loop
         if (static_cast<size_t>(end - it) <= 32) {
            if (!consume_small(it, end, remaining, lower_bound, upper_bound)) [[unlikely]] {
               return fail();
            }

            if (remaining != 0 || had_pending) {
               store_pending(remaining, lower_bound, upper_bound);
            }

            return true;
         }

         // Bulk path. Four bytes for checking any complete UTF-8 code point safely
         while (static_cast<size_t>(end - it) >= 4) {
            uint32_t byte = *it;

            // Avoid wide ASCII probes when already at a non-ASCII byte
            if (byte < 0x80) {
               it = skip_ascii_adaptive(it, end);
               if (static_cast<size_t>(end - it) < 4) {
                  break;
               }

               byte = *it;
            }

            // Non-ASCII, keep validating full codepoints until ASCII
            // appears or the safe bulk window ends
            do {
               if (!consume_full_non_ascii(it, byte)) [[unlikely]] {
                  return fail();
               }

               if (static_cast<size_t>(end - it) < 4) {
                  break;
               }

               byte = *it;
            } while (byte >= 0x80);
         }

         // Finish the last 0..3 bytes and save incomplete codepoint if present
         if (!consume_tail(it, end, remaining, lower_bound, upper_bound)) [[unlikely]] {
            return fail();
         }

         if (remaining != 0 || had_pending) {
            store_pending(remaining, lower_bound, upper_bound);
         }

         return true;
      }

      [[nodiscard]] GLZ_ALWAYS_INLINE bool complete() const noexcept { return valid_ && remaining_ == 0; }

     private:
      GLZ_ALWAYS_INLINE static bool is_continuation(const uint32_t byte) noexcept { return (byte & 0xC0) == 0x80; }

      GLZ_ALWAYS_INLINE static bool is_in_range(const uint32_t byte, const uint32_t lower_bound,
                                                const uint32_t upper_bound) noexcept
      {
         return byte - lower_bound <= upper_bound - lower_bound;
      }

      GLZ_ALWAYS_INLINE static uint64_t load_u64(const uint8_t* ptr) noexcept
      {
         uint64_t value;
         std::memcpy(&value, ptr, sizeof(value));
         return value;
      }

      GLZ_ALWAYS_INLINE static size_t first_non_ascii_offset(const uint64_t high_bits) noexcept
      {
         // Offset of the first non-ASCII byte within the 8-byte word
         if constexpr (std::endian::native == std::endian::big) {
            return static_cast<size_t>(std::countl_zero(high_bits) >> 3);
         }
         else {
            return static_cast<size_t>(std::countr_zero(high_bits) >> 3);
         }
      }

      GLZ_ALWAYS_INLINE static const uint8_t* skip_ascii8(const uint8_t* it, const uint8_t* end) noexcept
      {
         constexpr uint64_t mask = glz::repeat_byte8(0x80);

         // Cheap scanner for small buffers
         while (static_cast<size_t>(end - it) >= 8) {
            const uint64_t high_bits = load_u64(it) & mask;
            if (high_bits != 0) {
               return it + first_non_ascii_offset(high_bits);
            }

            it += 8;
         }

         while (it != end && *it < 0x80) {
            ++it;
         }

         return it;
      }

      GLZ_ALWAYS_INLINE static const uint8_t* skip_ascii_adaptive(const uint8_t* it, const uint8_t* end) noexcept
      {
         constexpr uint64_t mask = glz::repeat_byte8(0x80);

         // Probe the first few chunks one at a time. This will make short ASCII
         // runs cheaper, which matters for mixed cases
         for (uint32_t checked_chunks = 0; checked_chunks < 4; ++checked_chunks) {
            if (static_cast<size_t>(end - it) < 8) {
               while (it != end && *it < 0x80) {
                  ++it;
               }

               return it;
            }

            const uint64_t high_bits = load_u64(it) & mask;
            if (high_bits != 0) {
               return it + first_non_ascii_offset(high_bits);
            }

            it += 8;
         }

         // After 32 ASCII bytes, assume this is a longer ASCII run and use the
         // wider scanner to reduce loop overhead
         while (static_cast<size_t>(end - it) >= 32) {
            const uint64_t first_high_bits = load_u64(it) & mask;
            const uint64_t second_high_bits = load_u64(it + 8) & mask;
            const uint64_t third_high_bits = load_u64(it + 16) & mask;
            const uint64_t fourth_high_bits = load_u64(it + 24) & mask;

            if ((first_high_bits | second_high_bits | third_high_bits | fourth_high_bits) == 0) {
               it += 32;
               continue;
            }

            if (first_high_bits != 0) {
               return it + first_non_ascii_offset(first_high_bits);
            }

            if (second_high_bits != 0) {
               return it + 8 + first_non_ascii_offset(second_high_bits);
            }

            if (third_high_bits != 0) {
               return it + 16 + first_non_ascii_offset(third_high_bits);
            }

            return it + 24 + first_non_ascii_offset(fourth_high_bits);
         }

         return skip_ascii8(it, end);
      }

      GLZ_ALWAYS_INLINE static bool consume_full_non_ascii(const uint8_t*& it, const uint32_t byte0) noexcept
      {
         // Called only when at least four bytes remaining, so
         // no boundary checks are needed here

         if ((byte0 & 0xE0) == 0xC0) {
            const uint32_t byte1 = it[1];

            // C0/C1 would be overlong encodings for ASCII
            if (((byte1 & 0xC0) != 0x80) || ((byte0 & 0x1E) == 0)) [[unlikely]] {
               return false;
            }

            it += 2;
            return true;
         }

         if ((byte0 & 0xF0) == 0xE0) {
            const uint32_t byte1 = it[1];
            const uint32_t byte2 = it[2];

            // E0 requires A0-BF to reject overlong 3-byte sequences.
            // ED requires 80-9F to reject UTF-16 surrogate codepoints
            if (((byte1 & 0xC0) != 0x80) || ((byte2 & 0xC0) != 0x80) || (byte0 == 0xE0 && (byte1 & 0x20) == 0) ||
                (byte0 == 0xED && (byte1 & 0x20) != 0)) [[unlikely]] {
               return false;
            }

            it += 3;
            return true;
         }

         if ((byte0 & 0xF8) == 0xF0) {
            const uint32_t byte1 = it[1];
            const uint32_t byte2 = it[2];
            const uint32_t byte3 = it[3];

            // F5..FF are invalid. F0 requires 90-BF to reject overlong sequences.
            // F4 requires 80-8F to keep the decoded value within U+10FFFF
            if (((byte0 & 0x07) >= 0x05) || ((byte1 & 0xC0) != 0x80) || ((byte2 & 0xC0) != 0x80) ||
                ((byte3 & 0xC0) != 0x80) || (byte0 == 0xF0 && (byte1 & 0x30) == 0) || (byte0 == 0xF4 && byte1 > 0x8F))
               [[unlikely]] {
               return false;
            }

            it += 4;
            return true;
         }

         return false;
      }

      GLZ_ALWAYS_INLINE static bool consume_non_ascii_checked(const uint8_t*& it, const uint8_t* end,
                                                              uint32_t& remaining, uint32_t& lower_bound,
                                                              uint32_t& upper_bound) noexcept
      {
         // Boundary-safe version for small chunks and tails.
         // May leave pending state instead of failing at the buffer end

         const uint32_t byte0 = *it++;

         if ((byte0 & 0xE0) == 0xC0) {
            // C0/C1 would be overlong encodings for ASCII
            if ((byte0 & 0x1E) == 0) [[unlikely]] {
               return false;
            }

            if (it == end) {
               remaining = 1;
               lower_bound = 0x80;
               upper_bound = 0xBF;
               return true;
            }

            const uint32_t byte1 = *it++;

            if (!is_continuation(byte1)) [[unlikely]] {
               return false;
            }

            return true;
         }

         if ((byte0 & 0xF0) == 0xE0) {
            // Normal 3-byte starts use 80-BF for the first continuation.
            // E0 and ED are special to preserve shortest form and reject surrogates
            const uint32_t first_lower_bound = byte0 == 0xE0 ? 0xA0 : 0x80;
            const uint32_t first_upper_bound = byte0 == 0xED ? 0x9F : 0xBF;

            // Not enough bytes to finish this codepoint in the current buffer.
            // Validate what is available and keep the remaining bounds as state
            if (static_cast<size_t>(end - it) < 2) [[unlikely]] {
               remaining = 2;
               lower_bound = first_lower_bound;
               upper_bound = first_upper_bound;
               return consume_pending(it, end, remaining, lower_bound, upper_bound);
            }

            const uint32_t byte1 = *it++;
            const uint32_t byte2 = *it++;

            if (!is_in_range(byte1, first_lower_bound, first_upper_bound) || !is_continuation(byte2)) [[unlikely]] {
               return false;
            }

            return true;
         }

         if ((byte0 & 0xF8) == 0xF0) {
            // F5..FF are not valid UTF-8 lead bytes.
            if ((byte0 & 0x07) >= 0x05) [[unlikely]] {
               return false;
            }

            // Normal 4-byte starts use 80-BF for the first continuation.
            // F0 rejects overlong sequences
            const uint32_t first_lower_bound = byte0 == 0xF0 ? 0x90 : 0x80;
            // F4 rejects values above U+10FFFF
            const uint32_t first_upper_bound = byte0 == 0xF4 ? 0x8F : 0xBF;

            // Same split-sequence handling as the 3-byte path but with three
            // continuation bytes in total
            if (static_cast<size_t>(end - it) < 3) [[unlikely]] {
               remaining = 3;
               lower_bound = first_lower_bound;
               upper_bound = first_upper_bound;
               return consume_pending(it, end, remaining, lower_bound, upper_bound);
            }

            const uint32_t byte1 = *it++;
            const uint32_t byte2 = *it++;
            const uint32_t byte3 = *it++;

            if (!is_in_range(byte1, first_lower_bound, first_upper_bound) || !is_continuation(byte2) ||
                !is_continuation(byte3)) [[unlikely]] {
               return false;
            }

            return true;
         }

         return false;
      }

      GLZ_ALWAYS_INLINE static bool consume_small(const uint8_t*& it, const uint8_t* end, uint32_t& remaining,
                                                  uint32_t& lower_bound, uint32_t& upper_bound) noexcept
      {
         // Small-buffer path. Avoid the larger bulk-loop setup and still
         // use 8-byte ASCII skipping when useful
         while (it != end) {
            if (*it < 0x80) {
               it = skip_ascii8(it, end);

               if (it == end) {
                  return true;
               }
            }

            if (!consume_non_ascii_checked(it, end, remaining, lower_bound, upper_bound)) [[unlikely]] {
               return false;
            }

            if (remaining != 0) {
               return true;
            }
         }

         return true;
      }

      GLZ_ALWAYS_INLINE static bool consume_tail(const uint8_t*& it, const uint8_t* end, uint32_t& remaining,
                                                 uint32_t& lower_bound, uint32_t& upper_bound) noexcept
      {
         // Tail normally should be 0..3 bytes after bulk loop
         while (it != end) {
            if (*it < 0x80) {
               ++it;
               continue;
            }

            if (!consume_non_ascii_checked(it, end, remaining, lower_bound, upper_bound)) [[unlikely]] {
               return false;
            }

            if (remaining != 0) {
               return true;
            }
         }

         return true;
      }

      GLZ_ALWAYS_INLINE static bool consume_pending(const uint8_t*& it, const uint8_t* end, uint32_t& remaining,
                                                    uint32_t& lower_bound, uint32_t& upper_bound) noexcept
      {
         // Continue a partially consumed codepoint.
         // Only the first continuation byte may have a tightened bound
         while (remaining != 0 && it != end) {
            const uint32_t byte = *it++;
            if (!is_in_range(byte, lower_bound, upper_bound)) [[unlikely]] {
               return false;
            }

            --remaining;
            lower_bound = 0x80;
            upper_bound = 0xBF;
         }

         return true;
      }

      GLZ_ALWAYS_INLINE void store_pending(const uint32_t remaining, const uint32_t lower_bound,
                                           const uint32_t upper_bound) noexcept
      {
         remaining_ = static_cast<uint8_t>(remaining);
         lower_bound_ = static_cast<uint8_t>(lower_bound);
         upper_bound_ = static_cast<uint8_t>(upper_bound);
      }

      GLZ_ALWAYS_INLINE bool fail() noexcept
      {
         valid_ = false;
         return false;
      }

      uint8_t remaining_{};
      uint8_t lower_bound_{0x80};
      uint8_t upper_bound_{0xBF};
      bool valid_{true};
   };

}
