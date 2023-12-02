#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <iterator>

#include "glaze/util/inline.hpp"
#include "glaze/util/strod.hpp"

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
   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, const char*& c) noexcept
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
   GLZ_ALWAYS_INLINE constexpr bool stoui64(uint64_t& res, auto& it) noexcept
   {
      static_assert(sizeof(*it) == sizeof(char));
      const char* cur = reinterpret_cast<const char*>(&*it);
      const char* beg = cur;
      if (stoui64(res, cur)) {
         it += (cur - beg);
         return true;
      }
      return false;
   }

   template <class T, bool force_conformance, class CharType>
      requires(std::is_unsigned_v<T>)
   inline bool parse_int(T& val, const CharType*& cur) noexcept
   {
      const CharType* sig_cut{}; // significant part cutting position for long number
      [[maybe_unused]] const CharType* sig_end{}; // significant part ending position
      const CharType* dot_pos{}; // decimal point position
      uint32_t frac_zeros = 0;
      uint64_t sig = uint64_t(*cur - '0'); // significant part of the number
      int32_t exp = 0; // exponent part of the number
      bool exp_sign; // temporary exponent sign from literal part
      int32_t exp_sig = 0; // temporary exponent number from significant part
      int32_t exp_lit = 0; // temporary exponent number from exponent literal part
      uint64_t num_tmp; // temporary number for reading
      const CharType* tmp; // temporary cursor for reading

      /* begin with non-zero digit */
      if (sig > 9) [[unlikely]] {
         return false;
      }
      constexpr auto zero = uint8_t('0');
#define expr_intg(i)                              \
   if ((num_tmp = cur[i] - zero) <= 9) [[likely]] \
      sig = num_tmp + sig * 10;                   \
   else {                                         \
      if constexpr (force_conformance && i > 1) { \
         if (*cur == zero) return false;          \
      }                                           \
      goto digi_sepr_##i;                         \
   }
      repeat_in_1_18(expr_intg);
#undef expr_intg
      if constexpr (force_conformance) {
         if (*cur == zero) return false;
      }
      cur += 19; /* skip continuous 19 digits */
      if (!digi_is_digit_or_fp(*cur)) {
         val = static_cast<T>(sig);
         return true;
      }
      goto digi_intg_more; /* read more digits in integral part */
      /* process first non-digit character */
#define expr_sepr(i)                                              \
   digi_sepr_##i : if ((!digi_is_fp(uint8_t(cur[i])))) [[likely]] \
   {                                                              \
      cur += i;                                                   \
      val = sig;                                                  \
      return true;                                                \
   }                                                              \
   dot_pos = cur + i;                                             \
   if ((cur[i] == '.')) [[likely]] {                              \
      if (sig == 0)                                               \
         while (cur[frac_zeros + i + 1] == zero) ++frac_zeros;    \
      goto digi_frac_##i;                                         \
   }                                                              \
   cur += i;                                                      \
   sig_end = cur;                                                 \
   goto digi_exp_more;
      repeat_in_1_18(expr_sepr)
#undef expr_sepr
      /* read fraction part */
#define expr_frac(i)                                                                                 \
   digi_frac_##i : if (((num_tmp = uint64_t(cur[i + 1 + frac_zeros] - zero)) <= 9)) [[likely]] sig = \
                      num_tmp + sig * 10;                                                            \
   else                                                                                              \
   {                                                                                                 \
      goto digi_stop_##i;                                                                            \
   }
         repeat_in_1_18(expr_frac)
#undef expr_frac
            cur += 20 + frac_zeros; /* skip 19 digits and 1 decimal point */
      if (uint8_t(*cur - zero) > 9) goto digi_frac_end; /* fraction part end */
      goto digi_frac_more; /* read more digits in fraction part */
      /* significant part end */
#define expr_stop(i)                          \
   digi_stop_##i : cur += i + 1 + frac_zeros; \
   goto digi_frac_end;
      repeat_in_1_18(expr_stop)
#undef expr_stop
         /* read more digits in integral part */
         digi_intg_more : static constexpr uint64_t U64_MAX = (std::numeric_limits<uint64_t>::max)(); // todo
      if ((num_tmp = *cur - zero) < 10) {
         if (!digi_is_digit_or_fp(cur[1])) {
            /* this number is an integer consisting of 20 digits */
            if ((sig < (U64_MAX / 10)) || (sig == (U64_MAX / 10) && num_tmp <= (U64_MAX % 10))) {
               sig = num_tmp + sig * 10;
               cur++;
               val = static_cast<T>(sig);
               return true;
            }
         }
      }
      if ((e_bit | *cur) == 'e') {
         dot_pos = cur;
         goto digi_exp_more;
      }
      if (*cur == '.') {
         dot_pos = cur++;
         if (uint8_t(*cur - zero) > 9) [[unlikely]] {
            return false;
         }
      }
      /* read more digits in fraction part */
   digi_frac_more:
      sig_cut = cur; /* too large to fit in u64, excess digits need to be cut */
      sig += (*cur >= '5'); /* round */
      while (uint8_t(*++cur - zero) < 10) {
      }
      if (!dot_pos) {
         dot_pos = cur;
         if (*cur == '.') {
            if (uint8_t(*++cur - zero) > 9) [[unlikely]] {
               return false;
            }
            while (uint8_t(*++cur - zero) < 10) {
            }
         }
      }
      exp_sig = int32_t(dot_pos - sig_cut);
      exp_sig += (dot_pos < sig_cut);
      /* ignore trailing zeros */
      tmp = cur - 1;
      while (*tmp == '0' || *tmp == '.') tmp--;
      if (tmp < sig_cut) {
         sig_cut = nullptr;
      }
      else {
         sig_end = cur;
      }
      if ((e_bit | *cur) == 'e') goto digi_exp_more;
      goto digi_exp_finish;
      /* fraction part end */
   digi_frac_end:
      sig_end = cur;
      exp_sig = -int32_t((cur - dot_pos) - 1);
      if constexpr (force_conformance) {
         if (exp_sig == 0) [[unlikely]]
            return false;
      }
      if ((e_bit | *cur) != 'e') [[likely]] {
         if ((exp_sig < F64_MIN_DEC_EXP - 19)) [[unlikely]] {
            val = 0;
            return true;
         }
         exp = exp_sig;
         goto digi_finish;
      }
      else {
         goto digi_exp_more;
      }
      /* read exponent part */
   digi_exp_more:
      exp_sign = (*++cur == '-');
      cur += (*cur == '+' || *cur == '-');
      if (uint8_t(*cur - zero) > 9) {
         if constexpr (force_conformance) {
            return false;
         }
         else {
            goto digi_finish;
         }
      }
      while (*cur == '0') ++cur;
      /* read exponent literal */
      tmp = cur;
      uint8_t c;
      while (uint8_t(c = *cur - zero) < 10) {
         ++cur;
         exp_lit = c + uint32_t(exp_lit) * 10;
      }
      // large exponent case
      if ((cur - tmp >= 6)) [[unlikely]] {
         if (sig == 0 || exp_sign) {
            val = 0;
            return true;
         }
         else {
            return false;
         }
      }
      exp_sig += exp_sign ? -exp_lit : exp_lit;
      // validate exponent value
   digi_exp_finish:
      if (sig == 0) {
         val = 0;
         return true;
      }
      if (exp_sig == 19) {
         val *= T(powers_of_ten_int[exp_sig - 1]);
         if (is_safe_multiplication10(val)) [[likely]] {
            return val *= 10;
         }
         else [[unlikely]] {
            return false;
         }
      }
      else if (exp_sig >= 20) [[unlikely]] {
         return false;
      }
      exp = exp_sig;
      /* all digit read finished */
   digi_finish:

      if (exp <= -20) [[unlikely]] {
         val = T(0);
         return true;
      }

      val = static_cast<T>(sig);
      if (exp >= 0) {
         val *= T(powers_of_ten_int[exp]);
      }
      else {
         val /= T(powers_of_ten_int[-exp]);
      }
      return true;
   }
}
