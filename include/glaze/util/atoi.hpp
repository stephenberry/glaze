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

   // We don't allow decimals in integer parsing
   // We don't allow negative exponents
   // Thse cases can produce decimals which slow performance and add confusion
   // as to how the integer should be parsed (truncation, rounding, etc.)
   // We allow parsing as a float and casting elsewhere when needed
   // But, this integer parsing is designed to be straightfoward and fast
   // Values like 1e6 are allowed because it enables less typing from the user
   // We allow only two exponent digits, as the JSON specification only requires support up to e53

   // Valid JSON integer examples
   // 1234
   // 1234e1
   // 1e9

   // Invalid for this atoi algorithm
   // 1.234
   // 1234e-1
   // 0.0

   // The standard JSON specification for numbers and the associated rules apply

   // *** We ensure that a decimal value being parsed will result in an error
   // 1.2 should not produce 1, but rather an error, even when a single field is parsed
   // This ensures that we get proper errors when parsing and don't get confusing errors
   // It isn't technically required, because end validation would handle it, but it produces
   // much clearer errors

   inline constexpr std::array<bool, 256> exp_dec_table = [] {
      std::array<bool, 256> t{};
      t['.'] = true;
      t['E'] = true;
      t['e'] = true;
      return t;
   }();

   inline constexpr std::array<bool, 256> non_exp_table = [] {
      std::array<bool, 256> t{};
      t.fill(true);
      t['E'] = false;
      t['e'] = false;
      return t;
   }();

   inline constexpr std::array<bool, 256> digit_table = [] {
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
      return t;
   }();

   inline constexpr std::array<bool, 256> digit_or_dec_table = [] {
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
      return t;
   }();

   inline constexpr std::array<bool, 256> digit_exp_dec_table = [] {
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
      t['e'] = true;
      t['E'] = true;
      return t;
   }();
   
   template <class T>
   inline constexpr std::array<std::decay_t<T>, 256> peak_positive = [] {
      constexpr auto peak{ (std::numeric_limits<std::decay_t<T>>::max)() };
      std::array<std::decay_t<T>, 256> t{};
      t['0'] = (peak - 0) / 10;
      t['1'] = (peak - 1) / 10;
      t['2'] = (peak - 2) / 10;
      t['3'] = (peak - 3) / 10;
      t['4'] = (peak - 4) / 10;
      t['5'] = (peak - 5) / 10;
      t['6'] = (peak - 6) / 10;
      t['7'] = (peak - 7) / 10;
      t['8'] = (peak - 8) / 10;
      t['9'] = (peak - 9) / 10;
      return t;
   }();
   
   template <class T>
   inline constexpr std::array<std::decay_t<T>, 256> peak_negative = [] {
      constexpr auto peak{ size_t((std::numeric_limits<std::decay_t<T>>::max)()) + 1 };
      std::array<std::decay_t<T>, 256> t{};
      t['0'] = (peak - 0) / 10;
      t['1'] = (peak - 1) / 10;
      t['2'] = (peak - 2) / 10;
      t['3'] = (peak - 3) / 10;
      t['4'] = (peak - 4) / 10;
      t['5'] = (peak - 5) / 10;
      t['6'] = (peak - 6) / 10;
      t['7'] = (peak - 7) / 10;
      t['8'] = (peak - 8) / 10;
      t['9'] = (peak - 9) / 10;
      return t;
   }();

   template <std::integral T, class Char>
      requires(std::is_unsigned_v<T> && (sizeof(T) == 1 || sizeof(T) == 2))
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      if (digit_table[uint8_t(*c)]) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return false;
      }

      uint32_t i = v;
      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (c[-2] == '0') [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if constexpr (sizeof(T) == 2) {
         if (digit_table[uint8_t(*c)]) {
            i = i * 10 + (*c - '0');
            ++c;
         }
         else {
            goto finish;
         }

         if (digit_table[uint8_t(*c)]) {
            i = i * 10 + (*c - '0');
            ++c;
         }
      }

   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         if (digit_table[uint8_t(*c)] || (*c == '.')) [[unlikely]] {
            return false;
         }
         v = T(i);
         return i <= (std::numeric_limits<T>::max)();
      }

      c += (*c == '+');

      if (not digit_table[uint8_t(*c)]) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (digit_table[uint8_t(*c)]) {
         exp = exp * 10 + (*c - '0');
         ++c;
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
      v = T(i);
      return i <= (std::numeric_limits<T>::max)();
   }

   template <std::integral T, class Char>
      requires(std::is_unsigned_v<T> && sizeof(T) == 4)
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      if (digit_table[uint8_t(*c)]) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (c[-2] == '0') [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

   finish:
      uint64_t i = v;
      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
         if (digit_table[uint8_t(*c)]) [[unlikely]] {
            return false;
         }
      }
      if (*c == 'e' || *c == 'E') [[likely]] {
         ++c;
      }
      else [[unlikely]] {
         if (*c == '.') [[unlikely]] {
            return false;
         }
         v = T(i);
         return i <= (std::numeric_limits<T>::max)();
      }

      c += (*c == '+');

      if (not digit_table[uint8_t(*c)]) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (digit_table[uint8_t(*c)]) {
         exp = exp * 10 + (*c - '0');
         ++c;
      }
      if (exp > 9) [[unlikely]] {
         return false;
      }

      i *= powers_of_ten_int[exp];
      v = T(i);
      return i <= (std::numeric_limits<T>::max)();
   }

   struct value128 final
   {
      uint64_t low;
      uint64_t high;
   };

   // slow emulation routine for 32-bit
   GLZ_ALWAYS_INLINE constexpr uint64_t emulu(uint32_t x, uint32_t y) { return x * (uint64_t)y; }

   GLZ_ALWAYS_INLINE constexpr uint64_t umul128_generic(uint64_t ab, uint64_t cd, uint64_t* hi)
   {
      uint64_t ad = emulu((uint32_t)(ab >> 32), (uint32_t)cd);
      uint64_t bd = emulu((uint32_t)ab, (uint32_t)cd);
      uint64_t adbc = ad + emulu((uint32_t)ab, (uint32_t)(cd >> 32));
      uint64_t adbc_carry = (uint64_t)(adbc < ad);
      uint64_t lo = bd + (adbc << 32);
      *hi = emulu((uint32_t)(ab >> 32), (uint32_t)(cd >> 32)) + (adbc >> 32) + (adbc_carry << 32) + (uint64_t)(lo < bd);
      return lo;
   }

   // compute 64-bit a*b
   GLZ_ALWAYS_INLINE constexpr value128 full_multiplication(uint64_t a, uint64_t b)
   {
      if (std::is_constant_evaluated()) {
         value128 answer;
         answer.low = umul128_generic(a, b, &answer.high);
         return answer;
      }
      value128 answer;
#if defined(_M_ARM64) && !defined(__MINGW32__)
      // ARM64 has native support for 64-bit multiplications, no need to emulate
      // But MinGW on ARM64 doesn't have native support for 64-bit multiplications
      answer.high = __umulh(a, b);
      answer.low = a * b;
#elif defined(FASTFLOAT_32BIT) || (defined(_WIN64) && !defined(__clang__))
      answer.low = _umul128(a, b, &answer.high); // _umul128 not available on ARM64
#elif defined(FASTFLOAT_64BIT) && defined(__SIZEOF_INT128__)
      __uint128_t r = ((__uint128_t)a) * b;
      answer.low = uint64_t(r);
      answer.high = uint64_t(r >> 64);
#else
      answer.low = umul128_generic(a, b, &answer.high);
#endif
      return answer;
   }

   template <std::integral T, class Char>
      requires(std::is_unsigned_v<T> && sizeof(T) == 8)
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      if (digit_table[uint8_t(*c)]) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (c[-2] == '0') [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         if (v > peak_positive<T>[uint8_t(*c)]) [[unlikely]] {
            return false;
         }
         v = v * 10 + (*c - '0');
         ++c;
         if (digit_table[uint8_t(*c)]) [[unlikely]] {
            return false;
         }
      }

   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         if (*c == '.') [[unlikely]] {
            return false;
         }
         return true;
      }

      c += (*c == '+');

      if (not digit_table[uint8_t(*c)]) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (digit_table[uint8_t(*c)]) {
         exp = exp * 10 + (*c - '0');
         ++c;
      }
      if (exp > 19) [[unlikely]] {
         return false;
      }

#if defined(__SIZEOF_INT128__)
      const __uint128_t res = __uint128_t(v) * powers_of_ten_int[exp];
      v = T(res);
      return res <= (std::numeric_limits<T>::max)();
#else
      const auto res = full_multiplication(v, powers_of_ten_int[exp]);
      v = T(res.low);
      return res.high == 0;
#endif
   }

   template <std::integral T, class Char>
      requires(std::is_signed_v<T> && (sizeof(T) == 1 || sizeof(T) == 2))
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      const uint8_t sign = *c == '-';
      c += sign;

      if (digit_table[uint8_t(*c)]) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return false;
      }

      uint32_t i = v;
      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (c[-2] == '0') [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if constexpr (sizeof(T) == 2) {
         if (digit_table[uint8_t(*c)]) {
            i = i * 10 + (*c - '0');
            ++c;
         }
         else {
            goto finish;
         }

         if (digit_table[uint8_t(*c)]) {
            i = i * 10 + (*c - '0');
            ++c;
         }
      }

   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         if (digit_table[uint8_t(*c)] || (*c == '.')) [[unlikely]] {
            return false;
         }
         if constexpr (sizeof(T) == 1) {
            v = T((uint8_t(i) ^ -sign) + sign);
         }
         else if constexpr (sizeof(T) == 2) {
            v = T((uint16_t(i) ^ -sign) + sign);
         }
         return (i - sign) <= (std::numeric_limits<T>::max)();
      }

      c += (*c == '+');

      if (not digit_table[uint8_t(*c)]) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (digit_table[uint8_t(*c)]) {
         exp = exp * 10 + (*c - '0');
         ++c;
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
      if constexpr (sizeof(T) == 1) {
         v = T((uint8_t(i) ^ -sign) + sign);
      }
      else if constexpr (sizeof(T) == 2) {
         v = T((uint16_t(i) ^ -sign) + sign);
      }
      return (i - sign) <= (std::numeric_limits<T>::max)();
   }

   template <std::integral T, class Char>
      requires(std::is_signed_v<T> && (sizeof(T) == 4))
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      const uint8_t sign = *c == '-';
      c += sign;

      if (digit_table[uint8_t(*c)]) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return false;
      }

      uint64_t i = v;
      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (c[-2] == '0') [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }
      else {
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         i = i * 10 + (*c - '0');
         ++c;
      }

   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         if (digit_table[uint8_t(*c)] || (*c == '.')) [[unlikely]] {
            return false;
         }
         v = T((uint32_t(i) ^ -sign) + sign);
         return (i - sign) <= (std::numeric_limits<T>::max)();
      }

      c += (*c == '+');

      if (not digit_table[uint8_t(*c)]) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (digit_table[uint8_t(*c)]) {
         exp = exp * 10 + (*c - '0');
         ++c;
      }
      if (exp > 9) [[unlikely]] {
         return false;
      }

      i *= powers_of_ten_int[exp];
      v = T((uint32_t(i) ^ -sign) + sign);
      return (i - sign) <= (std::numeric_limits<T>::max)();
   }
   
   template <std::integral T, class Char>
      requires(std::is_signed_v<T> && sizeof(T) == 8)
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& c) noexcept
   {
      const uint8_t sign = *c == '-';
      c += sign;
      
      if (digit_table[uint8_t(*c)]) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (c[-2] == '0') [[unlikely]] {
         return false;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (not exp_dec_table[uint8_t(*c)]) {
            v = sign ? -v : v;
            return true;
         }
         goto finish;
      }

      if (digit_table[uint8_t(*c)]) {
         if (sign) {
            if (v > peak_negative<T>[uint8_t(*c)]) [[unlikely]] {
               return false;
            }
            v = (-v) * 10 - (*c - '0');
         }
         else {
            if (v > peak_positive<T>[uint8_t(*c)]) [[unlikely]] {
               return false;
            }
            v = v * 10 + (*c - '0');
         }
         ++c;
         if (digit_table[uint8_t(*c)]) [[unlikely]] {
            return false;
         }
      }
      else {
         v = sign ? -v : v;
      }

   finish:
      if (*c == 'e' || *c == 'E') {
         ++c;
      }
      else {
         if (*c == '.') [[unlikely]] {
            return false;
         }
         return true;
      }

      c += (*c == '+');

      if (not digit_table[uint8_t(*c)]) [[unlikely]] {
         return false;
      }
      ++c;
      uint8_t exp = c[-1] - '0';
      if (digit_table[uint8_t(*c)]) {
         exp = exp * 10 + (*c - '0');
         ++c;
      }
      if (exp > 19) [[unlikely]] {
         return false;
      }

#if defined(__SIZEOF_INT128__)
      const __uint128_t res = __uint128_t(v) * powers_of_ten_int[exp];
      v = sign ? -T(res) : T(res);
      return (res - sign) <= (std::numeric_limits<T>::max)();
#else
      const auto res = full_multiplication(uint64_t(v), powers_of_ten_int[exp]);
      v = T((uint64_t(res) ^ -sign) + sign);
      return res.high == 0 && ((res.low - sign) <= (std::numeric_limits<T>::max)());
#endif
   }

   static constexpr std::array<size_t, 4> int_buffer_lengths{8, 8, 16, 24};

   template <std::integral T, class Char>
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& it, const Char* end) noexcept
   {
      // The number of characters needed at most for each type, rounded to nearest 8 bytes
      constexpr auto buffer_length = int_buffer_lengths[std::bit_width(sizeof(T)) - 1];
      // We copy the rest of the buffer or 64 bytes into a null terminated buffer
      std::array<char, buffer_length> data{};
      const auto n = size_t(end - it);
      if (n > 0) [[likely]] {
         if (n < buffer_length) {
            std::memcpy(data.data(), it, n);
         }
         else {
            std::memcpy(data.data(), it, buffer_length);
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

namespace glz::detail
{
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
      if (!digit_table[uint8_t(*c)]) [[unlikely]] {
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

      while (digit_table[uint8_t(*c)]) {
         consume_digit();
      }
      auto n = int64_t(std::distance(digits.begin(), next_digit));

      if (*c == '.') {
         ++c;
         while (digit_table[uint8_t(*c)]) {
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
         while (digit_table[uint8_t(*c)] && exp < 128) {
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
}
