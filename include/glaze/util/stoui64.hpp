#pragma once

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <iterator>

#include "glaze/util/inline.hpp"

namespace glz::detail
{
   GLZ_ALWAYS_INLINE constexpr bool is_digit(const char c) noexcept { return c >= '0' && c <= '9'; }

   GLZ_ALWAYS_INLINE constexpr bool is_safe_addition(uint64_t a, uint64_t b) noexcept
   {
      return a <= (std::numeric_limits<uint64_t>::max)() - b;
   }

   GLZ_ALWAYS_INLINE constexpr bool is_safe_multiplication10(uint64_t a) noexcept
   {
      constexpr auto b = (std::numeric_limits<uint64_t>::max)() / 10;
      return a <= b;
   }
   
   GLZ_ALWAYS_INLINE constexpr auto has_zero(const uint64_t chunk) noexcept
   {
      return (((chunk - 0x0101010101010101) & ~chunk) & 0x8080808080808080);
   }
   
   GLZ_ALWAYS_INLINE constexpr uint64_t is_greater_7(const uint64_t c) noexcept {
      return (c & 0b1111100011111000111110001111100011111000111110001111100011111000);
   }
   
   GLZ_ALWAYS_INLINE constexpr uint64_t has_period(const uint64_t c) noexcept {
      return has_zero(c ^ 0b0010111000101110001011100010111000101110001011100010111000101110);
   }
   
   GLZ_ALWAYS_INLINE constexpr uint64_t has_e(const uint64_t c) noexcept {
      return has_zero(c ^ 0b0110010101100101011001010110010101100101011001010110010101100101);
   }
   
   GLZ_ALWAYS_INLINE constexpr uint64_t has_E(const uint64_t c) noexcept {
      return has_zero(c ^ 0b0100010101000101010001010100010101000101010001010100010101000101);
   }
   
   // https://lemire.me/blog/2022/01/21/swar-explained-parsing-eight-digits/
   GLZ_ALWAYS_INLINE constexpr uint32_t parse_eight_digits_unrolled(uint64_t val) noexcept {
     constexpr uint64_t mask = 0x000000FF000000FF;
     constexpr uint64_t mul1 = 0x000F424000000064; // 100 + (1000000ULL << 32)
     constexpr uint64_t mul2 = 0x0000271000000001; // 1 + (10000ULL << 32)
     val -= 0x3030303030303030;
     val = (val * 10) + (val >> 8); // val = (val * 2561) >> 8;
     return (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;
   }

   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE bool stoui64(uint64_t& res, const char*& c, const char* end) noexcept
   {
      if (!is_digit(*c)) [[unlikely]] {
         return false;
      }

      // Maximum number of digits need is: 3, 5, 10, 20, for byte sizes of 1, 2, 4, 8
      // We need to store one extra space for a digit for sizes of 1, 2, and 4 because we avoid checking for overflow
      // since we store in a uint64_t
      // However, use 8 byte alignment so we can use SWAR, and we want to have extra space for next_digit increments for '0' and '.'
      // we need to have 8 bytes available after the period
      constexpr std::array<uint32_t, 4> digits_end = {4, 6, 11, 20}; // for non SWAR approach
      constexpr auto digits_length = digits_end[std::bit_width(sizeof(T)) - 1];
      constexpr std::array<uint32_t, 4> max_digits_from_size = {16, 16, 24, 32};
      constexpr auto N = max_digits_from_size[std::bit_width(sizeof(T)) - 1];

      std::array<uint8_t, N> digits{0};
      auto next_digit = digits.begin();
      auto end_digit = digits.begin() + digits_length;
      
      if (std::distance(c, end) > N) [[likely]] {
         // check for fast path without '.', 'e', or 'E'
         
         auto start = c;
         while (next_digit < end_digit) {
            uint64_t chunk;
            std::memcpy(&chunk, c, 8);
            
            const uint64_t test_chars = has_period(chunk) | has_e(chunk) | has_E(chunk);
            if (test_chars) {
               c = start;
               goto slow_path;
            }
            else {
               c += 8;
               next_digit += 8;
            }
         }
         auto [ptr, ec] = std::from_chars(start, end, res);
         c = ptr;
         return ec == std::errc{};
      }
      
      slow_path:
      next_digit = digits.begin();
      
      auto consume_digits = [&] {
         while (is_digit(*c)) {
            if (next_digit < end_digit) [[likely]] {
               *next_digit = (*c - '0');
               ++next_digit;
            }
            ++c;
         }
      };

      if (*c == '0') {
         // digits[i] = 0; already set to zero
         ++c;
         ++next_digit;

         if (*c == '0') [[unlikely]] {
            return false;
         }
      }

      consume_digits();
      auto n = std::distance(digits.begin(), next_digit);

      if (*c == '.') {
         ++c;
         consume_digits();
      }
      
      if (*c == 'e' || *c == 'E') {
         ++c;

         bool negative = false;
         if (*c == '+' || *c == '-') {
            negative = (*c == '-');
            ++c;
         }
         uint8_t exp = 0;
         while (is_digit(*c) && exp < 128) {
            exp = 10 * exp + (*c - '0');
            ++c;
         }
         n += negative ? -exp : exp;
      }

      res = 0;
      if (n < 0) [[unlikely]] {
         return true;
      }

      if constexpr (std::same_as<T, uint64_t>) {
         if (n > 20) [[unlikely]] {
            return false;
         }

         if (n == 20) [[unlikely]] {
            for (auto k = 0; k < 19; ++k) {
               res = 10 * res + digits[k];
            }

            if (is_safe_multiplication10(res)) [[likely]] {
               res *= 10;
            }
            else [[unlikely]] {
               return false;
            }
            if (is_safe_addition(res, digits[19])) [[likely]] {
               res += digits[19];
            }
            else [[unlikely]] {
               return false;
            }
         }
         else [[likely]] {
            for (auto k = 0; k < n; ++k) {
               res = 10 * res + digits[k];
            }
         }
      }
      else {
         // a value of n == N would result in reading digits[N], which is invalid
         if (n >= N) [[unlikely]] {
            return false;
         }
         else [[likely]] {
            for (auto k = 0; k < n; ++k) {
               res = 10 * res + digits[k];
            }
         }
      }

      return true;
   }

   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, auto& it, auto& end) noexcept
   {
      static_assert(sizeof(*it) == sizeof(char));
      const char* cur = reinterpret_cast<const char*>(&*it);
      const char* e = reinterpret_cast<const char*>(&*end);
      const char* beg = cur;
      if (stoui64(res, cur, e)) {
         it += (cur - beg);
         return true;
      }
      return false;
   }
   
   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE constexpr bool stoui64_constexpr(uint64_t& res, const char*& c, [[maybe_unused]] const char* end) noexcept
   {
      if (!is_digit(*c)) [[unlikely]] {
         return false;
      }

      // maximum number of digits need is: 3, 5, 10, 20, for byte sizes of 1, 2, 4, 8
      // we need to store one extra space for a digit for sizes of 1, 2, and 4 because we avoid checking for overflow
      // since we store in a uint64_t
      constexpr std::array<uint32_t, 4> max_digits_from_size = {4, 6, 11, 20};
      constexpr auto N = max_digits_from_size[std::bit_width(sizeof(T)) - 1];

      std::array<uint8_t, N> digits{0};
      auto next_digit = digits.begin();
      auto consume_digit = [&c, &next_digit, &digits]() {
         if (next_digit < digits.cend()) [[likely]] {
            *next_digit = (*c - '0');
            ++next_digit;
         }
         ++c;
      };

      if (*c == '0') {
         // digits[i] = 0; already set to zero
         ++c;
         ++next_digit;

         if (*c == '0') [[unlikely]] {
            return false;
         }
      }

      while (is_digit(*c)) {
         consume_digit();
      }
      auto n = std::distance(digits.begin(), next_digit);

      if (*c == '.') {
         ++c;
         while (is_digit(*c)) {
            consume_digit();
         }
      }

      if (*c == 'e' || *c == 'E') {
         ++c;

         bool negative = false;
         if (*c == '+' || *c == '-') {
            negative = (*c == '-');
            ++c;
         }
         uint8_t exp = 0;
         while (is_digit(*c) && exp < 128) {
            exp = 10 * exp + (*c - '0');
            ++c;
         }
         n += negative ? -exp : exp;
      }

      res = 0;
      if (n < 0) [[unlikely]] {
         return true;
      }

      if constexpr (std::same_as<T, uint64_t>) {
         if (n > 20) [[unlikely]] {
            return false;
         }

         if (n == 20) [[unlikely]] {
            for (auto k = 0; k < 19; ++k) {
               res = 10 * res + digits[k];
            }

            if (is_safe_multiplication10(res)) [[likely]] {
               res *= 10;
            }
            else [[unlikely]] {
               return false;
            }
            if (is_safe_addition(res, digits.back())) [[likely]] {
               res += digits.back();
            }
            else [[unlikely]] {
               return false;
            }
         }
         else [[likely]] {
            for (auto k = 0; k < n; ++k) {
               res = 10 * res + digits[k];
            }
         }
      }
      else {
         // a value of n == N would result in reading digits[N], which is invalid
         if (n >= N) [[unlikely]] {
            return false;
         }
         else [[likely]] {
            for (auto k = 0; k < n; ++k) {
               res = 10 * res + digits[k];
            }
         }
      }

      return true;
   }

   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE constexpr bool stoui64_constexpr(uint64_t& res, auto& it, auto& end) noexcept
   {
      static_assert(sizeof(*it) == sizeof(char));
      const char* cur = reinterpret_cast<const char*>(&*it);
      const char* e = reinterpret_cast<const char*>(&*end);
      const char* beg = cur;
      if (stoui64(res, cur, e)) {
         it += (cur - beg);
         return true;
      }
      return false;
   }
}
