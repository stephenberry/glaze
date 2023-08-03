#pragma once

#include <array>
#include <cmath>

#include "glaze/util/inline.hpp"

namespace glz::detail
{
   GLZ_ALWAYS_INLINE constexpr bool is_digit(const char c) noexcept { return c >= '0' && c <= '9'; }

   GLZ_ALWAYS_INLINE constexpr bool is_safe_addition(uint64_t a, uint64_t b) noexcept {
       return a <= std::numeric_limits<uint64_t>::max() - b;
   }
   
   inline constexpr bool stoui64(uint64_t& res, const char*& c) noexcept {
      std::array<uint8_t, 20> digits{};
      
      uint32_t i{};
      if (*c == '0') {
         digits[i] = 0;
         ++c; ++i;
         
         if (*c == '0') [[unlikely]] {
            return false;
         }
      }
       
       while (is_digit(*c)) {
          if (i < 20) [[likely]] {
             digits[i] = (*c - '0');
          }
          ++c; ++i;
       }
      uint32_t upper = i;
      
      if (*c == '.') {
          ++c;
          while (is_digit(*c)) {
             if (i < 20) [[likely]] {
                digits[i] = (*c - '0');
             }
             ++c; ++i;
          }
      }
      
      int32_t exp = 0;
      if (*c == 'e' || *c == 'E') {
          ++c;
         
          bool negative = false;
          if (*c == '+' || *c == '-') {
              negative = (*c == '-');
              ++c;
          }
          while (is_digit(*c)) {
              exp = 10 * exp + (*c - '0');
              ++c;
          }
         exp = negative ? -exp : exp;
      }
      
      res = 0;
      const int32_t n = upper + exp;
      if (n < 0) [[unlikely]] {
         return true;
      }
      if (n > 20) [[unlikely]] {
         return false;
      }
      
      if (n == 20) [[unlikely]] {
         for (int32_t k = 0; k < 19; ++k) {
            res = 10 * res + digits[k];
         }
         
         res *= 10;
         if (is_safe_addition(res, digits[19])) [[likely]] {
            res += digits[19];
         }
         else [[unlikely]] {
            return false;
         }
      }
      else [[likely]] {
         for (int32_t k = 0; k < n; ++k) {
            res = 10 * res + digits[k];
         }
      }
      return true;
   }
}
