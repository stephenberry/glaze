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
#include "glaze/util/atoi.hpp"
#include "glaze/util/bit.hpp"
#include "glaze/util/compare.hpp"
#include "glaze/util/convert.hpp"
#include "glaze/util/expected.hpp"
#include "glaze/util/inline.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
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

   consteval uint32_t repeat_byte4(const auto repeat) { return uint32_t(0x01010101u) * uint8_t(repeat); }

   consteval uint64_t repeat_byte8(const uint8_t repeat) { return 0x0101010101010101ull * repeat; }

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

   template <class SrcChar, class DstChar = SrcChar>
   [[nodiscard]] GLZ_ALWAYS_INLINE uint32_t handle_unicode_code_point(const SrcChar*& it, DstChar*& dst,
                                                                      const SrcChar* end) noexcept
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
         if ((high & surrogate_mask) != high_surrogate_value) {
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
      const uint32_t offset = code_point_to_utf8(code_point, dst);
      dst += offset;
      return offset;
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

   template <string_literal str, auto Opts>
      requires(check_is_padded(Opts) && str.size() <= padding_bytes)
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it, auto) noexcept
   {
      static constexpr auto S = str.sv();
      if (not comparitor<S>(it)) [[unlikely]] {
         ctx.error = error_code::syntax_error;
      }
      else [[likely]] {
         it += str.size();
      }
   }

   template <string_literal str, auto Opts>
      requires(!check_is_padded(Opts))
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      const auto n = size_t(end - it);
      static constexpr auto S = str.sv();
      if ((n < str.size()) || not comparitor<S>(it)) [[unlikely]] {
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

   GLZ_ALWAYS_INLINE constexpr auto has_zero(const uint64_t chunk) noexcept
   {
      return (((chunk - 0x0101010101010101u) & ~chunk) & 0x8080808080808080u);
   }

   GLZ_ALWAYS_INLINE constexpr auto has_quote(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8('"'));
   }

   GLZ_ALWAYS_INLINE constexpr auto has_escape(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8('\\'));
   }

   GLZ_ALWAYS_INLINE constexpr auto has_space(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8(' '));
   }

   template <char Char>
   GLZ_ALWAYS_INLINE constexpr auto has_char(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8(Char));
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

   GLZ_ALWAYS_INLINE void skip_till_quote(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      const auto* pc = std::memchr(it, '"', size_t(end - it));
      if (pc) [[likely]] {
         it = reinterpret_cast<std::decay_t<decltype(it)>>(pc);
         return;
      }

      ctx.error = error_code::expected_quote;
   }

   GLZ_ALWAYS_INLINE void skip_string_view(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      while (it < end) [[likely]] {
         const auto* pc = std::memchr(it, '"', size_t(end - it));
         if (pc) [[likely]] {
            it = reinterpret_cast<std::decay_t<decltype(it)>>(pc);
            auto* prev = it - 1;
            while (*prev == '\\') {
               --prev;
            }
            if (size_t(it - prev) % 2) {
               return;
            }
            ++it; // skip the escaped quote
         }
         else [[unlikely]] {
            break;
         }
      }

      ctx.error = error_code::expected_quote;
   }

   // Options struct for skip_string - reduces template instantiations
   struct skip_string_opts
   {
      bool padded;
      bool opening_handled;
      bool validate_skipped;
      bool null_terminated;

      // Convert from any opts-like type (consteval because check_* functions are consteval)
      template <typename T>
      consteval skip_string_opts(const T& opts) noexcept
         : padded{check_is_padded(opts)},
           opening_handled{check_opening_handled(opts)},
           validate_skipped{check_validate_skipped(opts)},
           null_terminated{check_null_terminated(opts)}
      {}

      // Direct construction - null_terminated defaults to true for the structural (non-validating)
      // skip_until_closed call sites, which route through the end-bounded skip_string_view path
      // and so are independent of this flag.
      consteval skip_string_opts(bool padded_, bool opening_handled_, bool validate_skipped_,
                                 bool null_terminated_ = true) noexcept
         : padded{padded_},
           opening_handled{opening_handled_},
           validate_skipped{validate_skipped_},
           null_terminated{null_terminated_}
      {}
   };

   template <skip_string_opts Opts>
      requires(Opts.padded)
   GLZ_ALWAYS_INLINE void skip_string(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      if constexpr (not Opts.opening_handled) {
         ++it;
      }

      if constexpr (Opts.validate_skipped) {
         while (true) {
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

         // If we exit here, we never found a closing quote
         ctx.error = error_code::unexpected_end;
      }
      else {
         skip_string_view(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         ++it; // skip the quote
      }
   }

   template <skip_string_opts Opts>
      requires(not Opts.padded)
   GLZ_ALWAYS_INLINE void skip_string(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      if constexpr (not Opts.opening_handled) {
         ++it;
      }

      if constexpr (Opts.validate_skipped) {
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
               ctx.error = error_code::syntax_error;
               return;
            }
            }
            ++it;
         }
      }
      else {
         skip_string_view(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         ++it; // skip the quote
      }
   }

   // Options struct for skip_until_closed - reduces template instantiations
   struct skip_until_closed_opts
   {
      bool padded;
      bool comments;

      // Convert from any opts-like type (consteval because check_is_padded is consteval)
      template <typename T>
      consteval skip_until_closed_opts(const T& opts) noexcept : padded{check_is_padded(opts)}, comments{opts.comments}
      {}

      // Direct construction - all values required
      consteval skip_until_closed_opts(bool padded_, bool comments_) noexcept : padded{padded_}, comments{comments_} {}
   };

   template <skip_until_closed_opts Opts, char open, char close, size_t Depth = 1>
      requires(Opts.padded && not Opts.comments)
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr bool opening_not_handled = false;
      static constexpr bool skip_validation = false;

      size_t depth = Depth;

      while (it < end) [[likely]] {
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
               skip_string<skip_string_opts{Opts.padded, opening_not_handled, skip_validation}>(ctx, it, end);
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

      ctx.error = error_code::unexpected_end;
   }

   template <skip_until_closed_opts Opts, char open, char close, size_t Depth = 1>
      requires(Opts.padded && Opts.comments)
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr bool opening_not_handled = false;
      static constexpr bool skip_validation = false;

      size_t depth = Depth;

      while (it < end) [[likely]] {
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
               skip_string<skip_string_opts{Opts.padded, opening_not_handled, skip_validation}>(ctx, it, end);
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

      ctx.error = error_code::unexpected_end;
   }

   template <skip_until_closed_opts Opts, char open, char close, size_t Depth = 1>
      requires(not Opts.padded && not Opts.comments)
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr bool opening_not_handled = false;
      static constexpr bool skip_validation = false;

      size_t depth = Depth;

      for (const auto fin = end - 7; it < fin;) {
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
               skip_string<skip_string_opts{Opts.padded, opening_not_handled, skip_validation}>(ctx, it, end);
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
            skip_string<skip_string_opts{Opts.padded, opening_not_handled, skip_validation}>(ctx, it, end);
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

   template <skip_until_closed_opts Opts, char open, char close, size_t Depth = 1>
      requires(not Opts.padded && Opts.comments)
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      static constexpr bool opening_not_handled = false;
      static constexpr bool skip_validation = false;

      size_t depth = Depth;

      for (const auto fin = end - 7; it < fin;) {
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
               skip_string<skip_string_opts{Opts.padded, opening_not_handled, skip_validation}>(ctx, it, end);
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
            skip_string<skip_string_opts{Opts.padded, opening_not_handled, skip_validation}>(ctx, it, end);
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

   inline bool validate_utf8(const auto* str, const size_t size) noexcept
   {
      const uint8_t* it = reinterpret_cast<const uint8_t*>(str);
      const uint8_t* end = it + size;

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
}
