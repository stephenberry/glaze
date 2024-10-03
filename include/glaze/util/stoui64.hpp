#pragma once

#include <array>
#include <bit>
#include <cmath>
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

   //============================================================================
   // Digit Character Matcher
   //============================================================================
   // Digit: '0'.
   constexpr uint8_t digi_type_zero = 1 << 0;
   // Digit: [1-9].
   constexpr uint8_t digi_type_nonzero = 1 << 1;
   // Plus sign (positive): '+'.
   constexpr uint8_t digi_type_pos = 1 << 2;
   // Minus sign (negative): '-'.
   constexpr uint8_t digi_type_neg = 1 << 3;
   // Decimal point: '.'
   constexpr uint8_t digi_type_dot = 1 << 4;
   // Exponent sign: 'e, 'E'.
   constexpr uint8_t digi_type_exp = 1 << 5;
   // Digit type table
   constexpr std::array<uint8_t, 256> digi_table = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x08, 0x10, 0x00, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
      0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
   // Match a character with specified type.
   inline constexpr bool digi_is_type(uint8_t d, uint8_t type) noexcept { return (digi_table[d] & type) != 0; }
   // Match a floating point indicator: '.', 'e', 'E'.
   inline constexpr bool digi_is_fp(uint8_t d) noexcept
   {
      return (digi_table[d] & (digi_type_dot | digi_type_exp)) != 0;
   }
   // Match a digit or floating point indicator: [0-9], '.', 'e', 'E'.
   inline constexpr bool digi_is_digit_or_fp(uint8_t d) noexcept
   {
      return digi_is_type(d, uint8_t(digi_type_zero | digi_type_nonzero | digi_type_dot | digi_type_exp));
   }
// Macros used for loop unrolling
#define repeat_in_1_18(x)                                                                                \
   {                                                                                                     \
      x(1) x(2) x(3) x(4) x(5) x(6) x(7) x(8) x(9) x(10) x(11) x(12) x(13) x(14) x(15) x(16) x(17) x(18) \
   }
   constexpr auto e_bit = static_cast<uint8_t>('E' ^ 'e');
   //============================================================================
   // IEEE-754 Double Number Constants
   //============================================================================
   // maximum decimal power of double number (1.7976931348623157e308)
   constexpr auto f64_max_dec_exp = 308;
   // minimum decimal power of double number (4.9406564584124654e-324)
   constexpr auto f64_min_dec_exp = (-324);

   constexpr char zero = '0';

   consteval uint32_t ceillog2(uint32_t x) { return x < 2 ? x : 1 + ceillog2(x >> 1); }

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

   template <class T, bool IsVolatile>
   GLZ_ALWAYS_INLINE bool digi_finish(int32_t& exp, T& val, uint64_t& sig)
   {
      if (exp <= -20) [[unlikely]] {
         val = T(0);
         return true;
      }

      val = static_cast<T>(sig);
      if constexpr (IsVolatile) {
         if (exp >= 0) {
            val = val * static_cast<T>(powers_of_ten_int[exp]);
         }
         else {
            val = val / static_cast<T>(powers_of_ten_int[-exp]);
         }
      }
      else {
         if (exp >= 0) {
            val *= static_cast<T>(powers_of_ten_int[exp]);
         }
         else {
            val /= static_cast<T>(powers_of_ten_int[-exp]);
         }
      }
      return true;
   }

   template <class T, bool IsVolatile>
   GLZ_ALWAYS_INLINE bool digi_exp_finish(uint64_t& sig, T& val, int32_t& exp_sig, int32_t& exp)
   {
      if (sig == 0) {
         val = 0;
         return true;
      }
      if (exp_sig == 19) {
         if constexpr (IsVolatile) {
            val = val * static_cast<T>(powers_of_ten_int[exp_sig - 1]);
            if (is_safe_multiplication10(val)) [[likely]] {
               val = val * 10;
               return val;
            }
            else [[unlikely]] {
               return false;
            }
         }
         else {
            val *= static_cast<T>(powers_of_ten_int[exp_sig - 1]);
            if (is_safe_multiplication10(val)) [[likely]] {
               return bool(val *= 10);
            }
            else [[unlikely]] {
               return false;
            }
         }
      }
      else if (exp_sig >= 20) [[unlikely]] {
         return false;
      }
      exp = exp_sig;
      // all digit read finished:
      return digi_finish<T, IsVolatile>(exp, val, sig);
   };

   template <class T, class CharType, bool IsVolatile>
   inline bool digi_exp_more(bool& exp_sign, const CharType*& cur, const CharType*& tmp, T& val, int32_t& exp_sig,
                             int32_t& exp_lit, uint64_t& sig, int32_t& exp)
   {
      exp_sign = (*++cur == '-');
      cur += (*cur == '+' || *cur == '-');
      if (uint8_t(*cur - zero) > 9) {
         return false;
      }
      while (*cur == '0') ++cur;
      // read exponent literal
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
      // validate exponent value:
      return digi_exp_finish<T, IsVolatile>(sig, val, exp_sig, exp);
   };

   template <class T, class CharType, bool IsVolatile>
   GLZ_ALWAYS_INLINE bool digi_frac_end(bool& exp_sign, const CharType*& cur, const CharType*& tmp, T& val,
                                        int32_t& exp_sig, int32_t& exp_lit, uint64_t& sig, int32_t& exp,
                                        const CharType* dotPos)
   {
      exp_sig = -int32_t((cur - dotPos) - 1);
      if (exp_sig == 0) [[unlikely]]
         return false;
      if ((e_bit | *cur) != 'e') [[likely]] {
         if ((exp_sig < f64_min_dec_exp - 19)) [[unlikely]] {
            val = 0;
            return true;
         }
         exp = exp_sig;
         return digi_finish<T, IsVolatile>(exp, val, sig);
      }
      else {
         return digi_exp_more<T, CharType, IsVolatile>(exp_sign, cur, tmp, val, exp_sig, exp_lit, sig, exp);
      }
   };

   enum struct int_parse_state : uint32_t { error, ok, more_frac_digits };

   template <class T, class CharType, bool IsVolatile, size_t I>
   GLZ_ALWAYS_INLINE int_parse_state digi_frac(bool& exp_sign, const CharType*& cur, const CharType*& tmp, T& val,
                                               int32_t& exp_sig, int32_t& exp_lit, uint64_t& sig, int32_t& exp,
                                               const CharType* dot_pos, uint32_t& frac_zeros, uint64_t& num_tmp)
   {
      if ((num_tmp = uint64_t(cur[I + 1 + frac_zeros] - zero)) <= 9) {
         sig = num_tmp + sig * 10;
      }
      else {
         // digi_stop
         cur += I + 1 + frac_zeros;
         const bool valid =
            digi_frac_end<T, CharType, IsVolatile>(exp_sign, cur, tmp, val, exp_sig, exp_lit, sig, exp, dot_pos);
         return int_parse_state(valid);
      }
      if constexpr (I < 18) {
         return digi_frac<T, CharType, IsVolatile, I + 1>(exp_sign, cur, tmp, val, exp_sig, exp_lit, sig, exp, dot_pos,
                                                          frac_zeros, num_tmp);
      }
      else {
         return int_parse_state::more_frac_digits;
      }
   }

   template <class T, bool json_conformance = true, class CharType>
      requires(std::is_unsigned_v<T>)
   inline bool parse_int(auto& val, const CharType*& cur) noexcept
   {
      using X = std::remove_volatile_t<T>;
      constexpr auto IsVolatile = std::is_volatile_v<std::remove_reference_t<decltype(val)>>;
      const CharType* sig_cut; // significant part cutting position for long number
      const CharType* dot_pos{}; // decimal point position
      uint32_t frac_zeros = 0;
      uint64_t sig = uint64_t(*cur - '0'); // significant part of the number
      int32_t exp = 0; // exponent part of the number
      bool exp_sign; // temporary exponent sign from literal part
      int32_t exp_sig = 0; // temporary exponent number from significant part
      int32_t exp_lit = 0; // temporary exponent number from exponent literal part
      uint64_t num_tmp; // temporary number for reading
      const CharType* tmp; // temporary cursor for reading
      int_parse_state state; // digi_frac parse state

      // begin with non-zero digit
      if (sig > 9) [[unlikely]] {
         return false;
      }
      constexpr auto zero = uint8_t('0');
#define expr_intg(i)                                                                                                 \
   if ((num_tmp = cur[i] - zero) <= 9)                                                                               \
      sig = num_tmp + sig * 10;                                                                                      \
   else {                                                                                                            \
      if constexpr (json_conformance && i > 1) {                                                                     \
         if (*cur == zero) return false;                                                                             \
      }                                                                                                              \
      if (!digi_is_fp(uint8_t(cur[i]))) [[likely]] {                                                                 \
         cur += i;                                                                                                   \
         val = sig;                                                                                                  \
         return true;                                                                                                \
      }                                                                                                              \
      dot_pos = cur + i;                                                                                             \
      if ((cur[i] == '.')) [[likely]] {                                                                              \
         if (sig == 0)                                                                                               \
            while (cur[frac_zeros + i + 1] == zero) ++frac_zeros;                                                    \
         state = digi_frac<T, CharType, IsVolatile, i>(exp_sign, cur, tmp, val, exp_sig, exp_lit, sig, exp, dot_pos, \
                                                       frac_zeros, num_tmp);                                         \
         if (state == int_parse_state::more_frac_digits) [[unlikely]] {                                              \
            cur += 20 + frac_zeros;                                                                                  \
            if (uint8_t(*cur - zero) > 9) goto digi_frac_end;                                                        \
            goto digi_frac_more;                                                                                     \
         }                                                                                                           \
         else [[likely]] {                                                                                           \
            return bool(state);                                                                                      \
         }                                                                                                           \
      }                                                                                                              \
      cur += i;                                                                                                      \
      goto digi_exp_more;                                                                                            \
   }
      repeat_in_1_18(expr_intg);
#undef expr_intg
      if constexpr (json_conformance) {
         if (*cur == zero) return false;
      }
      cur += 19; // skip continuous 19 digits
      if (!digi_is_digit_or_fp(*cur)) {
         val = static_cast<X>(sig);
         return true;
      }
      // read more digits in integral part
      static constexpr uint64_t U64_MAX = (std::numeric_limits<uint64_t>::max)(); // todo
      if ((num_tmp = *cur - zero) < 10) {
         if (!digi_is_digit_or_fp(cur[1])) {
            // this number is an integer consisting of 20 digits
            if ((sig < (U64_MAX / 10)) || (sig == (U64_MAX / 10) && num_tmp <= (U64_MAX % 10))) {
               sig = num_tmp + sig * 10;
               cur++;
               val = static_cast<X>(sig);
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
      // read more digits in fraction part
   digi_frac_more:
      sig_cut = cur; // too large to fit in u64, excess digits need to be cut
      sig += (*cur >= '5'); // round
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
      // ignore trailing zeros
      tmp = cur - 1;
      while (*tmp == '0' || *tmp == '.') tmp--;
      if ((e_bit | *cur) == 'e') goto digi_exp_more;
      return digi_exp_finish<T, IsVolatile>(sig, val, exp_sig, exp);
      // fraction part end
   digi_frac_end:
      exp_sig = -int32_t((cur - dot_pos) - 1);
      if constexpr (json_conformance) {
         if (exp_sig == 0) [[unlikely]]
            return false;
      }
      if ((e_bit | *cur) != 'e') [[likely]] {
         if ((exp_sig < f64_min_dec_exp - 19)) [[unlikely]] {
            val = 0;
            return true;
         }
         exp = exp_sig;
         return digi_finish<T, IsVolatile>(exp, val, sig);
      }
      else {
         goto digi_exp_more;
      }
      // read exponent part
   digi_exp_more:
      exp_sign = (*++cur == '-');
      cur += (*cur == '+' || *cur == '-');
      if (uint8_t(*cur - zero) > 9) {
         if constexpr (json_conformance) {
            return false;
         }
         else {
            return digi_finish<T, IsVolatile>(exp, val, sig);
         }
      }
      while (*cur == '0') ++cur;
      // read exponent literal
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
      return digi_exp_finish<T, IsVolatile>(sig, val, exp_sig, exp);
   }

   // For non-null terminated buffers
   template <class T, bool json_conformance = true, class CharType>
      requires(std::is_unsigned_v<T>)
   bool parse_int(auto& val, const CharType*& cur, const CharType* end) noexcept
   {
      // We copy the rest of the buffer or 64 bytes into a null terminated buffer
      char data[65]{};
      const auto n = size_t(end - cur);
      if (n > 0) [[likely]] {
         if (n < 64) {
            std::memcpy(data, cur, n);
         }
         else {
            std::memcpy(data, cur, 64);
         }

         const char* it = data;
         const auto valid = parse_int<T, json_conformance>(val, it);
         cur += size_t(it - data);
         return valid;
      }
      else [[unlikely]] {
         return false;
      }
   }
}
