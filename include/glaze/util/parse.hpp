// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstring>
#include <iterator>
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
      while (true) {
         switch (*it) {
         case '\t':
         case '\n':
         case '\r':
         case ' ':
            ++it;
            break;
         case '/': {
            if constexpr (Opts.force_conformance) {
               ctx.error = error_code::syntax_error;
               return;
            }
            else {
               skip_comment(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               break;
            }
         }
         default:
            return;
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

      const auto end_m7 = end - 7;
      for (; it < end_m7; it += 8) {
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

   // very similar code to skip_till_quote, but it consumes the iterator and returns the key
   [[nodiscard]] GLZ_ALWAYS_INLINE const sv parse_unescaped_key(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      auto start = it;

      const auto end_m7 = end - 7;
      for (; it < end_m7; it += 8) {
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

   // very similar code to skip_till_quote, but it consumes the iterator and returns the key
   template <uint32_t MinLength, uint32_t LengthRange>
   [[nodiscard]] GLZ_ALWAYS_INLINE const sv parse_key_cx(auto&& it) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      auto start = it;
      it += MinLength; // immediately skip minimum length

      static_assert(LengthRange < 16);
      if constexpr (LengthRange == 7) {
         uint64_t chunk; // no need to default initialize
         std::memcpy(&chunk, it, 8);
         const uint64_t test_chunk = has_quote(chunk);
         if (test_chunk != 0) [[likely]] {
            it += (std::countr_zero(test_chunk) >> 3);

            return {start, size_t(it - start)};
         }
      }
      else if constexpr (LengthRange > 7) {
         uint64_t chunk; // no need to default initialize
         std::memcpy(&chunk, it, 8);
         uint64_t test_chunk = has_quote(chunk);
         if (test_chunk != 0) {
            it += (std::countr_zero(test_chunk) >> 3);

            return {start, size_t(it - start)};
         }
         else {
            it += 8;
            static constexpr auto rest = LengthRange + 1 - 8;
            chunk = 0; // must zero out the chunk
            std::memcpy(&chunk, it, rest);
            test_chunk = has_quote(chunk);
            if (test_chunk != 0) {
               it += (std::countr_zero(test_chunk) >> 3);

               return {start, size_t(it - start)};
            }
         }
      }
      else {
         uint64_t chunk{};
         std::memcpy(&chunk, it, LengthRange + 1);
         const uint64_t test_chunk = has_quote(chunk);
         if (test_chunk != 0) [[likely]] {
            it += (std::countr_zero(test_chunk) >> 3);

            return {start, size_t(it - start)};
         }
      }

      return {start, size_t(it - start)};
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_string(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (bool(ctx.error)) [[unlikely]] {
         return;
      }

      ++it;

      if constexpr (Opts.force_conformance) {
         while (true) {
            switch (*it) {
            case '"':
               ++it;
               return;
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t': {
               ctx.error = error_code::syntax_error;
               return;
            }
            case '\\': {
               ++it;
               switch (*it) {
               case '"':
               case '\\':
               case '/':
               case 'b':
               case 'f':
               case 'n':
               case 'r':
               case 't': {
                  ++it;
                  continue;
               }
               case 'u': {
                  ++it;
                  if ((end - it) < 4) [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  else if (std::all_of(it, it + 4, ::isxdigit)) [[likely]] {
                     it += 4;
                  }
                  else [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  continue;
               }
                  [[unlikely]] default:
                  {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
            }
               [[likely]] default : ++it;
            }
         }
      }
      else {
         while (it < end) {
            if (*it == '"') {
               ++it;
               break;
            }
            else if (*it == '\\' && ++it == end) [[unlikely]]
               break;
            ++it;
         }
      }
   }

   template <char open, char close>
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (bool(ctx.error)) [[unlikely]] {
         return;
      }

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

      match<'"'>(ctx, it, end);
      if (bool(ctx.error)) [[unlikely]]
         return {};
      auto start = it;
      skip_till_quote(ctx, it, end);
      if (bool(ctx.error)) [[unlikely]]
         return {};
      return sv{start, static_cast<size_t>(it++ - start)};
   }
}
