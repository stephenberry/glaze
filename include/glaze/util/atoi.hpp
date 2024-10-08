#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iterator>

#include "glaze/util/inline.hpp"

namespace glz::detail
{
   constexpr std::array<uint64_t, 20> powers_of_ten_int{1ull,
                                                        10ull,
                                                        100ull,
                                                        1000ull,
                                                        10000ull,
                                                        100000ull,
                                                        1000000ull,
                                                        10000000ull,
                                                        100000000ull,
                                                        1000000000ull,
                                                        10000000000ull,
                                                        100000000000ull,
                                                        1000000000000ull,
                                                        10000000000000ull,
                                                        100000000000000ull,
                                                        1000000000000000ull,
                                                        10000000000000000ull,
                                                        100000000000000000ull,
                                                        1000000000000000000ull,
                                                        10000000000000000000ull};

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

   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, const char*& c) noexcept
   {
      if (!is_digit(*c)) [[unlikely]] {
         return false;
      }

      // maximum number of digits need is: 3, 5, 10, 20, for byte sizes of 1, 2, 4, 8
      // we need to store one extra space for a digit for sizes of 1, 2, and 4 because we avoid checking for overflow
      // since we store in a uint64_t
      constexpr std::array<int64_t, 4> max_digits_from_size = {4, 6, 11, 20};
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
      auto n = int64_t(std::distance(digits.begin(), next_digit));

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
            for (size_t k = 0; k < 19; ++k) {
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
            for (int64_t k = 0; k < n; ++k) {
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
            for (int64_t k = 0; k < n; ++k) {
               res = 10 * res + digits[k];
            }
         }
      }

      return true;
   }

   template <class T = uint64_t>
   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, auto& it) noexcept
   {
      static_assert(sizeof(*it) == sizeof(char));
      const char* cur = reinterpret_cast<const char*>(it);
      const char* beg = cur;
      if (stoui64(res, cur)) {
         it += (cur - beg);
         return true;
      }
      return false;
   }
   
   // We don't allow decimals in integer parsing
   // We don't allow negative exponents
   // Thse cases can produce decimals which slower performance and add confusion
   // to how the integer should be parsed (truncation, rounding, etc.)
   // We allow parsing as a float and casting elsewhere when needed
   // But, this integer parsing is designed to be straightfoward and fast
   // Values like 1e6 are allowed because it enables less typing from the user
   // We allow only two exponent digits, as the JSON specification only requires support up to e53
   
   // *** We ensure that a decimal value being parsed will result in an error
   // 1.2 should not produce 1, but rather an error, even when a single field is parsed
   // This ensures that we get proper errors when parsing and don't get confusing errors
   // It isn't technically required, because end validation would handle it, but it produces
   // much clearer errors
   
   template <std::integral T, class Char>
      requires(std::is_unsigned_v<T> && (sizeof(T) == 1 || sizeof(T) == 2))
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      
      if (c[-1] == '0') {
         if (is_digit(*c) || *c == '.') [[unlikely]] {
            return false;
         }
         if (*c == 'e' || *c == 'E') [[unlikely]] {
            ++c;
            if (*c == '-') {
               return false;
            }
            c += (*c == '+');
            if (not is_digit(*c)) [[unlikely]] {
               return false;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               return false;
            }
            v = 0;
            return true;
         }
         v = 0;
         return true;
      }
      
      uint32_t i = c[-1] - '0';
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }

      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if constexpr (sizeof(T) == 2) {
         if (is_digit(*c)) {
            i = i * 10 + (*c - '0');
            ++c;
         } else {
            goto finish;
         }

         if (is_digit(*c)) {
            i = i * 10 + (*c - '0');
            ++c;
         } else {
            goto finish;
         }
      }

      if (is_digit(*c)) [[unlikely]] {
         return false;
      }
      
   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         const bool valid = i <= (std::numeric_limits<T>::max)();
         v = T(i);
         return valid && (*c != '.');
      }
      
      if (*c == '-') {
         return false;
      }
      c += (*c == '+');
      
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (is_digit(*c)) {
         exp = exp * 10 + (*c - '0');
      }
      if constexpr (sizeof(T) == 1) {
         if (exp > 2) [[unlikely]] {
            return false;
         }
      }
      else if constexpr (sizeof(T) == 2) {
         if (exp > 4) [[unlikely]] {
            return false;
         }
      }
      
      if constexpr (sizeof(T) == 1) {
         static constexpr std::array<uint8_t, 3> powers_of_ten{1, 10, 100};
         i *= powers_of_ten[exp];
      }
      else {
         static constexpr std::array<uint16_t, 5> powers_of_ten{1, 10, 100, 1000, 10000};
         i *= powers_of_ten[exp];
      }
      const bool valid = i <= (std::numeric_limits<T>::max)();
      v = T(i);
      return valid;
   }
   
   template <std::integral T, class Char>
      requires(std::is_unsigned_v<T> && sizeof(T) == 4)
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      
      if (c[-1] == '0') {
         if (is_digit(*c) || *c == '.') [[unlikely]] {
            return false;
         }
         if (*c == 'e' || *c == 'E') [[unlikely]] {
            ++c;
            if (*c == '-') {
               return false;
            }
            c += (*c == '+');
            if (not is_digit(*c)) [[unlikely]] {
               return false;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               return false;
            }
            v = 0;
            return true;
         }
         v = 0;
         return true;
      }
      
      uint64_t i = c[-1] - '0';
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }

      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }

      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }

      if (is_digit(*c)) [[unlikely]] {
         return false;
      }
      
   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         const bool valid = i <= (std::numeric_limits<T>::max)();
         v = T(i);
         return valid && (*c != '.');
      }
      
      if (*c == '-') {
         return false;
      }
      c += (*c == '+');
      
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (is_digit(*c)) {
         exp = exp * 10 + (*c - '0');
      }
      if (exp > 9) [[unlikely]] {
         return false;
      }
      
      i *= powers_of_ten_int[exp];
      const bool valid = i <= (std::numeric_limits<T>::max)();
      v = T(i);
      return valid;
   }
   
   template <std::integral T, class Char>
      requires(std::is_unsigned_v<T> && sizeof(T) == 8)
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      
      if (c[-1] == '0') {
         if (is_digit(*c) || *c == '.') [[unlikely]] {
            return false;
         }
         if (*c == 'e' || *c == 'E') [[unlikely]] {
            ++c;
            if (*c == '-') {
               return false;
            }
            c += (*c == '+');
            if (not is_digit(*c)) [[unlikely]] {
               return false;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               return false;
            }
            v = 0;
            return true;
         }
         v = 0;
         return true;
      }
      
      v = c[-1] - '0';
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }

      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }

      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      } else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         const bool valid = v <= (std::numeric_limits<T>::max)() / 10;
         v = v * 10 + (*c - '0');
         if (not valid || (v < 16)) {
            return false;
         }
         ++c;
      } else {
         goto finish;
      }

      if (is_digit(*c)) [[unlikely]] {
         return false;
      }
      
   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         return (*c != '.');
      }
      
      if (*c == '-') {
         return false;
      }
      c += (*c == '+');
      
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (is_digit(*c)) {
         exp = exp * 10 + (*c - '0');
      }
      if (exp > 19) [[unlikely]] {
         return false;
      }
      
      const __uint128_t res = __uint128_t(v) * powers_of_ten_int[exp];
      v = T(res);
      return res <= (std::numeric_limits<T>::max)();
   }
   
   template <std::integral T, class Char>
      requires(std::is_signed_v<T> && (sizeof(T) == 1 || sizeof(T) == 2))
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      const bool sign = *c == '-';
      c += sign;
      if (not (is_digit(*c))) [[unlikely]] {
         return false;
      }
      ++c;
      
      if (c[-1] == '0') {
         if (is_digit(*c) || *c == '.') [[unlikely]] {
            return false;
         }
         if (*c == 'e' || *c == 'E') [[unlikely]] {
            ++c;
            if (*c == '-') {
               return false;
            }
            c += (*c == '+');
            if (not is_digit(*c)) [[unlikely]] {
               return false;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               return false;
            }
            v = 0;
            return true;
         }
         v = 0;
         return true;
      }
      
      uint32_t i = c[-1] - '0';
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if constexpr (sizeof(T) == 2) {
         if (is_digit(*c)) {
            i = i * 10 + (*c - '0');
            ++c;
         }
         else {
            goto finish;
         }
         
         if (is_digit(*c)) {
            i = i * 10 + (*c - '0');
            ++c;
         }
         else {
            goto finish;
         }
      }
      
      if (is_digit(*c)) [[unlikely]] {
         return false;
      }
      
   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         const bool valid = (i - sign) <= (std::numeric_limits<T>::max)();
         if constexpr (sizeof(T) == 1) {
            v = T((uint8_t(i) ^ -sign) + sign);
         }
         else if constexpr (sizeof(T) == 2) {
            v = T((uint16_t(i) ^ -sign) + sign);
         }
         return valid && (*c != '.');
      }
      
      if (*c == '-') {
         return false;
      }
      c += (*c == '+');
      
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (is_digit(*c)) {
         exp = exp * 10 + (*c - '0');
      }
      if constexpr (sizeof(T) == 1) {
         if (exp > 2) [[unlikely]] {
            return false;
         }
      }
      else if constexpr (sizeof(T) == 2) {
         if (exp > 4) [[unlikely]] {
            return false;
         }
      }
      
      if constexpr (sizeof(T) == 1) {
         static constexpr std::array<uint8_t, 3> powers_of_ten{1, 10, 100};
         i *= powers_of_ten[exp];
      }
      else if constexpr (sizeof(T) == 2) {
         static constexpr std::array<uint16_t, 5> powers_of_ten{1, 10, 100, 1000, 10000};
         i *= powers_of_ten[exp];
      }
      const bool valid = (i - sign) <= (std::numeric_limits<T>::max)();
      if constexpr (sizeof(T) == 1) {
         v = T((uint8_t(i) ^ -sign) + sign);
      }
      else if constexpr (sizeof(T) == 2) {
         v = T((uint16_t(i) ^ -sign) + sign);
      }
      return valid;
   }
   
   template <std::integral T, class Char>
      requires(std::is_signed_v<T> && (sizeof(T) == 4))
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      const bool sign = *c == '-';
      c += sign;
      if (not (is_digit(*c))) [[unlikely]] {
         return false;
      }
      ++c;
      
      if (c[-1] == '0') {
         if (is_digit(*c) || *c == '.') [[unlikely]] {
            return false;
         }
         if (*c == 'e' || *c == 'E') [[unlikely]] {
            ++c;
            if (*c == '-') {
               return false;
            }
            c += (*c == '+');
            if (not is_digit(*c)) [[unlikely]] {
               return false;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               return false;
            }
            v = 0;
            return true;
         }
         v = 0;
         return true;
      }
      
      uint64_t i = c[-1] - '0';
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) [[unlikely]] {
         return false;
      }
      
   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         const bool valid = (i - sign) <= (std::numeric_limits<T>::max)();
         v = T((uint32_t(i) ^ -sign) + sign);
         return valid && (*c != '.');
      }
      
      if (*c == '-') {
         return false;
      }
      c += (*c == '+');
      
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (is_digit(*c)) {
         exp = exp * 10 + (*c - '0');
      }
      if (exp > 9) [[unlikely]] {
         return false;
      }
      
      i *= powers_of_ten_int[exp];
      const bool valid = (i - sign) <= (std::numeric_limits<T>::max)();
      v = T((uint32_t(i) ^ -sign) + sign);
      return valid;
   }
   
   template <std::integral T, class Char>
      requires(std::is_signed_v<T> && (sizeof(T) == 8))
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      const bool sign = *c == '-';
      c += sign;
      if (not (is_digit(*c))) [[unlikely]] {
         return false;
      }
      ++c;
      
      if (c[-1] == '0') {
         if (is_digit(*c) || *c == '.') [[unlikely]] {
            return false;
         }
         if (*c == 'e' || *c == 'E') [[unlikely]] {
            ++c;
            if (*c == '-') {
               return false;
            }
            c += (*c == '+');
            if (not is_digit(*c)) [[unlikely]] {
               return false;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               ++c;
            }
            if (is_digit(*c)) {
               return false;
            }
            v = 0;
            return true;
         }
         v = 0;
         return true;
      }
      
      uint64_t i = c[-1] - '0';
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) {
         const bool valid = (i - sign) <= (std::numeric_limits<T>::max)() / 10;
         i = i * 10 + (*c - '0');
         if (not valid || ((i - sign) > (std::numeric_limits<T>::max)())) {
            return false;
         }
         ++c;
      }
      else {
         goto finish;
      }
      
      if (is_digit(*c)) [[unlikely]] {
         return false;
      }
      
   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         v = T((uint64_t(i) ^ -sign) + sign);
         return (*c != '.');
      }
      
      if (*c == '-') {
         return false;
      }
      c += (*c == '+');
      
      if (not is_digit(*c)) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (is_digit(*c)) {
         exp = exp * 10 + (*c - '0');
      }
      if (exp > 18) [[unlikely]] {
         return false;
      }
      
      const __uint128_t res = __uint128_t(i) * powers_of_ten_int[exp];
      v = T((uint64_t(res) ^ -sign) + sign);
      return (res - sign) <= (std::numeric_limits<T>::max)();
   }
   
   template <std::integral T, class Char>
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& it, const Char* end) noexcept
   {
      // We copy the rest of the buffer or 64 bytes into a null terminated buffer
      std::array<char, 65> data{};
      const auto n = size_t(end - it);
      if (n > 0) [[likely]] {
         if (n < 64) {
            std::memcpy(data.data(), it, n);
         }
         else {
            std::memcpy(data.data(), it, 64);
         }

         const auto start = data.data();
         const auto* c = start;
         const auto valid = glz::detail::atoi(v, c);
         it += size_t(c - start);
         return valid;
      }
      else [[unlikely]] {
         return false;
      }
   }
}
