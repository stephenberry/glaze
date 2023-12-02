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
      return a <= std::numeric_limits<uint64_t>::max() - b;
   }

   GLZ_ALWAYS_INLINE constexpr bool is_safe_multiplication10(uint64_t a) noexcept
   {
      constexpr auto b = std::numeric_limits<uint64_t>::max() / 10;
      return a <= b;
   }

   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, const char*& c, [[maybe_unused]] const char* end) noexcept
   {
      if (!is_digit(*c)) [[unlikely]] {
         return false;
      }

      // maximum number of digits need is: 3, 5, 10, 20, for byte sizes of 1, 2, 4, 8
      // we need to store one extra space for a digit for sizes of 1, 2, and 4 because we avoid checking for overflow
      // since we store in a uint64_t
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
      
      const auto buffer_available = std::distance(c, end);
      
      bool prepared_data = false;
      if (buffer_available >= N) [[likely]] {
         prepared_data = true;
         std::memcpy(digits.data(), c, N); // copy buffer into digits
         
         constexpr uint32_t iters = N / 8;
         for_each<iters>([&](auto I) {
            uint64_t* ptr = reinterpret_cast<uint64_t*>(digits.data() + I * 8);
            (*ptr) -= 0x3030303030303030ULL; // subtract 48 from bytes
         });
      }
      
      auto next_digit = digits.begin();

      if (*c == '0') {
         // digits[i] = 0; already set to zero
         ++c;
         ++next_digit;

         if (*c == '0') [[unlikely]] {
            return false;
         }
      }
      
      const auto end_digit = digits.begin() + digits_length;
      auto consume_digits = [&]() {
         if (prepared_data) [[likely]] {
            while (*next_digit < 10 && next_digit < end_digit) {
               ++next_digit;
               ++c;
            }
         }
         else [[unlikely]] {
            while (is_digit(*c)) {
               if (next_digit < end_digit) [[likely]] {
                  *next_digit = (*c - '0');
                  ++next_digit;
               }
               ++c;
            }
         }
      };

      consume_digits();
      auto n = std::distance(digits.begin(), next_digit);

      if (*c == '.') {
         if (prepared_data) [[likely]] {
            // we need to memmove to cover over the period
            std::memmove(next_digit, next_digit + 1, size_t(end_digit - (next_digit + 1)));
         }
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
