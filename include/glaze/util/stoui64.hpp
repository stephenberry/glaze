#pragma once

#include <array>
#include <bit>
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
   
   GLZ_ALWAYS_INLINE constexpr uint64_t is_greater_7(const uint64_t c) noexcept {
      return (c & 0b1111100011111000111110001111100011111000111110001111100011111000);
   }

   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, const char*& c, [[maybe_unused]] const char* end) noexcept
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
      auto next_digit = digits.data();
      auto end_digit = digits.data() + digits_length;
      const auto buffer_available = std::distance(c, end);
      auto consume_digits = [&] {
         if (buffer_available > 31) {
            while (next_digit < end_digit) {
               uint64_t chunk;
               std::memcpy(&chunk, c, 8);
               chunk -= 0x3030303030303030ULL; // subtract 48 from bytes
               std::memcpy(next_digit, &chunk, 8);
               
               const uint64_t test_chars = is_greater_7(chunk);
               if (test_chars) {
                  const auto length = (std::countr_zero(test_chars) >> 3);
                  c += length;
                  next_digit += length;
                  if (*c == '8') {
                     ++c;
                     ++next_digit;
                  }
                  else if (*c == '9') {
                     ++c;
                     ++next_digit;
                  }
                  else {
                     std::memset(next_digit, 0, size_t(end_digit - next_digit));
                     break;
                  }
               }
               else {
                  c += 8;
                  next_digit += 8;
               }
            }
         }
         else {
            while (is_digit(*c)) {
               if (next_digit < end_digit) [[likely]] {
                  *next_digit = (*c - '0');
                  ++next_digit;
               }
               ++c;
            }
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
      auto n = std::distance(digits.data(), next_digit);

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
}
