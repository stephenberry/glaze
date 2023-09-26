#pragma once

#include <array>
#include <cmath>
#include <iterator>

#include "glaze/util/inline.hpp"

namespace glz::detail
{
   GLZ_ALWAYS_INLINE constexpr bool is_digit(const char c) noexcept { return c >= '0' && c <= '9'; }

   GLZ_ALWAYS_INLINE constexpr bool is_safe_addition(uint64_t a, uint64_t b) noexcept
   {
      return a <= std::numeric_limits<uint64_t>::max() - b;
   }

   GLZ_ALWAYS_INLINE constexpr bool is_safe_multiplication10(uint64_t a) noexcept
   {
      constexpr auto b = std::numeric_limits<uint64_t>::max() / 10;
      return a <= b;
   }

   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, const char*& c) noexcept
   {
      if (!is_digit(*c)) [[unlikely]] {
         return false;
      }

      std::array<uint8_t, 20> digits{0};
      auto next_digit = digits.begin();
      auto consume_digit = [&c, &next_digit, &digits]() {
         if (next_digit < digits.cend()) [[likely]] {
            *next_digit = (*c - '0');
         }
         ++c;
         ++next_digit;
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
      return true;
   }
}
