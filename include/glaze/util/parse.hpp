// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstring>
#include <iterator>
#include <locale>
#include <span>

#include "glaze/api/name.hpp"
#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/expected.hpp"
#include "glaze/util/inline.hpp"
#include "glaze/util/stoui64.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/string_view.hpp"

namespace glz::detail
{
   // clang-format off
   constexpr std::array<char, 256> char_unescape_table = { //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
      0, 0, 0, 0, '"', 0, 0, 0, 0, 0, //
      0, 0, 0, 0, 0, 0, 0, '/', 0, 0, //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
      0, 0, '\\', 0, 0, 0, 0, 0, '\b', 0, //
      0, 0, '\f', 0, 0, 0, 0, 0, 0, 0, //
      '\n', 0, 0, 0, '\r', 0, '\t', 0, 0, 0, //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0 //
   };
   // clang-format on

   // assumes null terminated
   template <char c>
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it) noexcept
   {
      if (*it != c) [[unlikely]] {
         ctx.error = error_code::syntax_error;
      }
      else [[likely]] {
         ++it;
      }
   }

   // assumes null terminated
   template <char c>
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it, auto&&) noexcept
   {
      if (*it != c) [[unlikely]] {
         ctx.error = error_code::syntax_error;
      }
      else [[likely]] {
         ++it;
      }
   }

   template <string_literal str>
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      const auto n = size_t(end - it);
      if ((n < str.size()) || std::memcmp(&*it, str.value, str.size())) [[unlikely]] {
         ctx.error = error_code::syntax_error;
      }
      else [[likely]] {
         it += str.size();
      }
   }

   GLZ_ALWAYS_INLINE void skip_comment(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (bool(ctx.error)) [[unlikely]] {
         return;
      }

      ++it;
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
      }
      else if (*it == '/') {
         while (++it != end && *it != '\n')
            ;
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

   consteval uint64_t repeat_byte(char c)
   {
      const auto byte = uint8_t(c);
      uint64_t res{};
      res |= uint64_t(byte) << 56;
      res |= uint64_t(byte) << 48;
      res |= uint64_t(byte) << 40;
      res |= uint64_t(byte) << 32;
      res |= uint64_t(byte) << 24;
      res |= uint64_t(byte) << 16;
      res |= uint64_t(byte) << 8;
      res |= uint64_t(byte);
      return res;
   }

   GLZ_ALWAYS_INLINE constexpr auto has_zero(const uint64_t chunk) noexcept
   {
      return (((chunk - 0x0101010101010101) & ~chunk) & 0x8080808080808080);
   }

   GLZ_ALWAYS_INLINE constexpr auto has_quote(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte('"'));
   }

   GLZ_ALWAYS_INLINE constexpr auto has_escape(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte('\\'));
   }

   GLZ_ALWAYS_INLINE constexpr auto has_space(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte(' '));
   }

   GLZ_ALWAYS_INLINE constexpr auto has_forward_slash(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte('/'));
   }

   GLZ_ALWAYS_INLINE constexpr uint64_t is_less_16(const uint64_t c) noexcept
   {
      return has_zero(c & 0b1111000011110000111100001111000011110000111100001111000011110000);
   }

   GLZ_ALWAYS_INLINE constexpr uint64_t is_greater_15(const uint64_t c) noexcept
   {
      return (c & 0b1111000011110000111100001111000011110000111100001111000011110000);
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_ws_no_pre_check(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.force_conformance) {
         while (true) {
            switch (*it) {
            case '\t':
            case '\n':
            case '\r':
            case ' ':
               ++it;
               break;
            case '/': {
               skip_comment(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               break;
            }
            default:
               return;
            }
         }
      }
      else {
         while (true) {
            switch (*it) {
            case '\t':
            case '\n':
            case '\r':
            case ' ':
               ++it;
               break;
            default:
               return;
            }
         }
      }
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_ws(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (ctx.error == error_code::none) [[likely]] {
         skip_ws_no_pre_check<Opts>(ctx, it, end);
      }
   }

   GLZ_ALWAYS_INLINE void skip_till_escape_or_quote(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      for (const auto end_m7 = end - 7; it < end_m7; it += 8) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         const uint64_t test_chars = has_quote(chunk) | has_escape(chunk);
         if (test_chars) {
            it += (std::countr_zero(test_chars) >> 3);
            return;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         switch (*it) {
         case '\\':
         case '"':
            return;
         }
         ++it;
      }

      ctx.error = error_code::expected_quote;
   }

   GLZ_ALWAYS_INLINE void skip_till_quote(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      auto* pc = std::memchr(it, '"', std::distance(it, end));
      if (pc) [[likely]] {
         it = reinterpret_cast<std::decay_t<decltype(it)>>(pc);
         return;
      }

      ctx.error = error_code::expected_quote;
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE bool skip_till_unescaped_quote(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      if constexpr (!Opts.force_conformance) {
         bool escaped = false;
         for (const auto fin = end - 7; it < fin;) {
            uint64_t chunk;
            std::memcpy(&chunk, it, 8);
            uint64_t test_chars = has_quote(chunk);
            escaped |= has_escape(chunk);
            if (test_chars) {
               it += (std::countr_zero(test_chars) >> 3);

               auto* prev = it - 1;
               while (*prev == '\\') {
                  --prev;
               }
               if (size_t(it - prev) % 2) {
                  return escaped;
               }
               ++it; // skip the escaped quote
            }
            else {
               it += 8;
            }
         }

         // Tail end of buffer. Should be rare we even get here
         while (it < end) {
            switch (*it) {
            case '\\': {
               escaped = true;
               ++it;
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::expected_quote;
                  return true;
               }
               ++it;
               break;
            }
            case '"': {
               auto* prev = it - 1;
               while (*prev == '\\') {
                  --prev;
                  escaped = true;
               }
               if (size_t(it - prev) % 2) {
                  return escaped;
               }
               ++it; // skip the escaped quote
               break;
            }
            default: {
               ++it;
            }
            }
         }
      }
      else {
         bool escaped = false;
         for (const auto fin = end - 7; it < fin;) {
            uint64_t chunk;
            std::memcpy(&chunk, it, 8);
            uint64_t test_chars = has_quote(chunk) | has_escape(chunk) | is_less_16(chunk);
            if (test_chars) {
               it += (std::countr_zero(test_chars) >> 3);

               if (*it == '"') {
                  return escaped;
               }
               else if (*it == '\\') [[likely]] {
                  ++it;
                  if (char_unescape_table[*it]) [[likely]] {
                     ++it;
                     continue;
                  }
                  else if (*it == 'u') [[unlikely]] {
                     ++it;
                     if ((end - it) < 4) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return true;
                     }
                     else if (std::all_of(it, it + 4, ::isxdigit)) [[likely]] {
                        it += 4;
                        continue;
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return true;
                     }
                  }
               }
               ctx.error = error_code::syntax_error;
               return true;
            }
            else {
               it += 8;
            }
         }

         // Tail end of buffer. Should be rare we even get here
         while (it < end) {
            if (*it < 16) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return true;
            }

            switch (*it) {
            case '\\': {
               escaped = true;
               ++it;
               if (char_unescape_table[*it]) [[likely]] {
                  ++it;
               }
               else if (*it == 'u') {
                  ++it;
                  if ((end - it) < 4) [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return true;
                  }
                  else if (std::all_of(it, it + 4, ::isxdigit)) [[likely]] {
                     it += 4;
                     continue;
                  }
                  else [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return true;
                  }
               }
               else [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return true;
               }

               break;
            }
            case '"': {
               return escaped;
            }
            default: {
               ++it;
            }
            }
         }
      }

      ctx.error = error_code::expected_quote;
      return false;
   }

   // very similar code to skip_till_quote, but it consumes the iterator and returns the key
   [[nodiscard]] GLZ_ALWAYS_INLINE const sv parse_unescaped_key(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      auto start = it;

      for (const auto end_m7 = end - 7; it < end_m7; it += 8) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         uint64_t test_chars = has_quote(chunk);
         if (test_chars) {
            it += (std::countr_zero(test_chars) >> 3);

            sv ret{start, size_t(it - start)};
            ++it;
            return ret;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         if (*it == '"') {
            sv ret{start, size_t(it - start)};
            ++it;
            return ret;
         }
         ++it;
      }
      ctx.error = error_code::expected_quote;
      return {};
   }

   struct key_stats_t
   {
      uint32_t min_length = (std::numeric_limits<uint32_t>::max)();
      uint32_t max_length{};
      uint32_t length_range{};
   };

   // consumes the iterator and returns the key
   // for error_on_unknown_keys = false, this may not return a valid key, in which case the iterator will not point to a
   // quote (")
   template <opts Opts, key_stats_t stats>
      requires(stats.length_range < 24)
   [[nodiscard]] GLZ_ALWAYS_INLINE const sv parse_key_cx(auto&& it) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      static constexpr auto LengthRange = stats.length_range;

      auto start = it;

      if constexpr (Opts.error_on_unknown_keys) {
         it += stats.min_length; // immediately skip minimum length
      }
      else {
         // unknown keys must be searched for within the min_length

         // I don't want to support unknown keys having escaped quotes.
         // This would make everyone pay significant performance losses for an edge case that can be handled with known
         // keys.

         if constexpr (stats.min_length <= 8) {
            uint64_t chunk{};
            std::memcpy(&chunk, it, stats.min_length);
            const uint64_t test_chunk = has_quote(chunk);
            if (test_chunk) [[likely]] {
               it += (std::countr_zero(test_chunk) >> 3);
               return {start, size_t(it - start)};
            }
            it += stats.min_length;
         }
         else {
            auto e = it + stats.min_length;
            uint64_t chunk;
            for (const auto end_m7 = e - 7; it < end_m7; it += 8) {
               std::memcpy(&chunk, it, 8);
               const uint64_t test_chars = has_quote(chunk);
               if (test_chars) {
                  it += (std::countr_zero(test_chars) >> 3);
                  return {start, size_t(it - start)};
               }
            }

            while (it < e) {
               if (*it == '"') {
                  return {start, size_t(it - start)};
               }
               ++it;
            }
         }
      }

      if constexpr (LengthRange == 0) {
         return {start, stats.min_length};
      }
      else if constexpr (LengthRange == 1) {
         if (*it != '"') {
            ++it;
         }
         return {start, size_t(it - start)};
      }
      else if constexpr (LengthRange < 4) {
         for (const auto e = it + stats.length_range + 1; it < e; ++it) {
            if (*it == '"') {
               break;
            }
         }
         return {start, size_t(it - start)};
      }
      else if constexpr (LengthRange == 7) {
         uint64_t chunk; // no need to default initialize
         std::memcpy(&chunk, it, 8);
         const uint64_t test_chunk = has_quote(chunk);
         if (test_chunk) [[likely]] {
            it += (std::countr_zero(test_chunk) >> 3);
         }
         return {start, size_t(it - start)};
      }
      else if constexpr (LengthRange > 15) {
         uint64_t chunk; // no need to default initialize
         std::memcpy(&chunk, it, 8);
         uint64_t test_chunk = has_quote(chunk);
         if (test_chunk) {
            goto finish;
         }

         it += 8;
         std::memcpy(&chunk, it, 8);
         test_chunk = has_quote(chunk);
         if (test_chunk) {
            goto finish;
         }

         it += 8;
         static constexpr auto rest = LengthRange + 1 - 16;
         chunk = 0; // must zero out the chunk
         std::memcpy(&chunk, it, rest);
         test_chunk = has_quote(chunk);
         // If our chunk is zero, we have an invalid key (for error_on_unknown_keys = true)
         // We set the chunk to 1 so that we increment it by 0
         if (!test_chunk) {
            test_chunk = 1;
         }

      finish:
         it += (std::countr_zero(test_chunk) >> 3);
         return {start, size_t(it - start)};
      }
      else if constexpr (LengthRange > 7) {
         uint64_t chunk; // no need to default initialize
         std::memcpy(&chunk, it, 8);
         uint64_t test_chunk = has_quote(chunk);
         if (test_chunk) {
            it += (std::countr_zero(test_chunk) >> 3);
         }
         else {
            it += 8;
            static constexpr auto rest = LengthRange + 1 - 8;
            chunk = 0; // must zero out the chunk
            std::memcpy(&chunk, it, rest);
            test_chunk = has_quote(chunk);
            if (test_chunk) {
               it += (std::countr_zero(test_chunk) >> 3);
            }
         }
         return {start, size_t(it - start)};
      }
      else {
         uint64_t chunk{};
         std::memcpy(&chunk, it, LengthRange + 1);
         const uint64_t test_chunk = has_quote(chunk);
         if (test_chunk) [[likely]] {
            it += (std::countr_zero(test_chunk) >> 3);
         }
         return {start, size_t(it - start)};
      }
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_string(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.opening_handled) {
         ++it;
      }

      if constexpr (Opts.force_conformance) {
         while (true) {
            if (*it < 16) [[unlikely]] {
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
               if (char_unescape_table[*it]) {
                  ++it;
                  continue;
               }
               else if (*it == 'u') {
                  ++it;
                  if ((end - it) < 4) [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  else if (std::all_of(it, it + 4, ::isxdigit)) [[likely]] {
                     it += 4;
                     continue;
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
         for (const auto fin = end - 7; it < fin;) {
            uint64_t chunk;
            std::memcpy(&chunk, it, 8);
            const uint64_t test_chars = has_quote(chunk);
            if (test_chars) {
               it += (std::countr_zero(test_chars) >> 3);

               auto* prev = it - 1;
               while (*prev == '\\') {
                  --prev;
               }
               if (size_t(it - prev) % 2) {
                  ++it; // skip the quote
                  return;
               }
               ++it; // skip the escaped quote
            }
            else {
               it += 8;
            }
         }

         // Tail end of buffer. Should be rare we even get here
         while (it < end) {
            switch (*it) {
            case '\\': {
               ++it;
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::expected_quote;
                  return;
               }
               ++it;
               break;
            }
            case '"': {
               auto* prev = it - 1;
               while (*prev == '\\') {
                  --prev;
               }
               if (size_t(it - prev) % 2) {
                  ++it; // skip the quote
                  return;
               }
               ++it; // skip the escaped quote
               break;
            }
            default: {
               ++it;
            }
            }
         }
      }
   }

   template <char open, char close>
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      ++it;
      size_t open_count = 1;
      size_t close_count = 0;
      while (true) {
         switch (*it) {
         case '\0':
            ctx.error = error_code::unexpected_end;
            return;
         case '/':
            skip_comment(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            break;
         case '"':
            skip_string<opts{}>(ctx, it, end);
            break;
         case open:
            ++open_count;
            ++it;
            break;
         case close:
            ++close_count;
            ++it;
            if (close_count >= open_count) {
               return;
            }
            break;
         default:
            ++it;
         }
      }
   }

   GLZ_ALWAYS_INLINE constexpr bool is_numeric(const auto c) noexcept
   {
      switch (c) {
      case '0':
      case '1':
      case '2':
      case '3': //
      case '4':
      case '5':
      case '6':
      case '7': //
      case '8':
      case '9': //
      case '.':
      case '+':
      case '-': //
      case 'e':
      case 'E': //
         return true;
      }
      return false;
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

   GLZ_ALWAYS_INLINE void skip_number_with_validation(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      it += *it == '-';
      const auto sig_start_it = it;
      auto frac_start_it = end;
      if (*it == '0') {
         ++it;
         if (*it != '.') {
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
      if ((*it | ('E' ^ 'e')) == 'e') {
         ++it;
         goto exp_start;
      }
      if (*it != '.') return;
      ++it;
   frac_start:
      frac_start_it = it;
      it = std::find_if_not(it, end, is_digit);
      if (it == frac_start_it) {
         ctx.error = error_code::syntax_error;
         return;
      }
      if ((*it | ('E' ^ 'e')) != 'e') return;
      ++it;
   exp_start:
      it += *it == '+' || *it == '-';
      const auto exp_start_it = it;
      it = std::find_if_not(it, end, is_digit);
      if (it == exp_start_it) {
         ctx.error = error_code::syntax_error;
         return;
      }
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_number(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if constexpr (!Opts.force_conformance) {
         it = std::find_if_not(it + 1, end, is_numeric<char>);
      }
      else {
         skip_number_with_validation(ctx, it, end);
      }
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_value(is_context auto&& ctx, auto&& it, auto&& end) noexcept;

   // expects opening whitespace to be handled
   GLZ_ALWAYS_INLINE sv parse_key(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      // TODO this assumes no escapes.
      if (bool(ctx.error)) [[unlikely]]
         return {};

      match<'"'>(ctx, it);
      if (bool(ctx.error)) [[unlikely]]
         return {};
      auto start = it;
      skip_till_quote(ctx, it, end);
      if (bool(ctx.error)) [[unlikely]]
         return {};
      return sv{start, static_cast<size_t>(it++ - start)};
   }

   /* Copyright (c) 2022 Tero 'stedo' Liukko, MIT License */
   GLZ_ALWAYS_INLINE unsigned char hex2dec(char hex) { return ((hex & 0xf) + (hex >> 6) * 9); }

   GLZ_ALWAYS_INLINE char32_t hex4_to_char32(const char* hex)
   {
      uint32_t value = hex2dec(hex[3]);
      value |= hex2dec(hex[2]) << 4;
      value |= hex2dec(hex[1]) << 8;
      value |= hex2dec(hex[0]) << 12;
      return value;
   }

   GLZ_ALWAYS_INLINE bool handle_escaped_unicode(auto*& in, auto*& out)
   {
      in += 2;
      // This is slow but who is escaping unicode nowadays
      // codecvt is problematic on mingw hence mixing with the c character conversion functions
      if (!std::all_of(in, in + 4, ::isxdigit)) [[unlikely]] {
         return false;
      }

      char32_t codepoint = hex4_to_char32(in);

      in += 4;

      char8_t buffer[4];
      auto& facet = std::use_facet<std::codecvt<char32_t, char8_t, mbstate_t>>(std::locale());
      std::mbstate_t mbstate{};
      const char32_t* from_next;
      char8_t* to_next;
      const auto result = facet.out(mbstate, &codepoint, &codepoint + 1, from_next, buffer, buffer + 4, to_next);
      if (result != std::codecvt_base::ok) {
         return false;
      }

      const auto offset = size_t(to_next - buffer);
      std::memcpy(out, buffer, offset);
      out += offset;
      return true;
   }

   // errors return the 'in' pointer for better error reporting
   template <size_t Bytes>
      requires(Bytes == 8)
   GLZ_ALWAYS_INLINE const char* parse_string(const auto* in, auto* out, context& ctx) noexcept
   {
      uint64_t swar;
      while (true) {
         std::memcpy(&swar, in, Bytes);
         std::memcpy(out, in, Bytes);
         auto next = has_quote(swar) | has_escape(swar);

         if (next) {
            next = std::countr_zero(next) >> 3;
            auto escape_char = in[next];
            if (escape_char == '"') {
               return out + next;
            }
            else if (escape_char == '\\') {
               escape_char = in[next + 1];
               if (escape_char == 'u') [[unlikely]] {
                  in += next;
                  out += next;
                  if (!handle_escaped_unicode(in, out)) {
                     ctx.error = error_code::unicode_escape_conversion_failure;
                     return in;
                  }
                  continue;
               }
               escape_char = char_unescape_table[escape_char];
               if (escape_char == 0) [[unlikely]] {
                  ctx.error = error_code::invalid_escape;
                  return in;
               }
               out[next] = escape_char;
               out += next + 1;
               in += next + 2;
            }
         }
         else {
            out += 8;
            in += 8;
         }
      }
   }

   // errors return the 'in' pointer for better error reporting
   template <size_t Bytes>
      requires(Bytes == 1)
   GLZ_ALWAYS_INLINE const char* parse_string(const auto* in, auto* out, context& ctx) noexcept
   {
      while (true) {
         *out = *in;
         if (*in == '"' || *in == '\\') {
            auto escape_char = *in;
            if (escape_char == '"') {
               return out;
            }
            else if (escape_char == '\\') {
               escape_char = in[1];
               if (escape_char == 'u') {
                  if (!handle_escaped_unicode(in, out)) {
                     ctx.error = error_code::unicode_escape_conversion_failure;
                     return in;
                  }
                  continue;
               }
               escape_char = char_unescape_table[escape_char];
               if (escape_char == 0) {
                  ctx.error = error_code::invalid_escape;
                  return in;
               }
               out[0] = escape_char;
               out += 1;
               in += 2;
            }
         }
         else {
            ++out;
            ++in;
         }
      }
   }

   template <size_t multiple>
   GLZ_ALWAYS_INLINE constexpr auto round_up_to_multiple(const std::integral auto val) noexcept
   {
      return val + (multiple - (val % multiple)) % multiple;
   }
}
