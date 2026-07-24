#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "glaze/util/for_each.hpp"
#include "glaze/util/inline.hpp"
#include "glaze/util/type_traits.hpp"

// Characters to integer parsing

// - We don't allow decimals in integer parsing
// - We don't allow negative exponents
// These cases can produce fractions which slow performance and add confusion
// as to how the integer ought to be parsed (truncation, rounding, etc.)
// This integer parsing is designed to be straightforward and fast
// Values like 1e6 are allowed because it enables less typing from the user
// and has a clear integer value

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
// much clearer errors, especially when we don't perform trailing validation.

#if defined(_MSC_VER) && !defined(__clang__)
// Turn off MSVC warning for possible loss of data: we are intentionally allowing well defined unsigned integer
// overflows
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

namespace glz
{
   inline constexpr std::array<uint64_t, 20> powers_of_ten_int{1ull,
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

   GLZ_ALWAYS_INLINE constexpr bool is_digit(const uint8_t c) noexcept { return c <= '9' && c >= '0'; }

   // Exponents at or beyond this magnitude are out of range for every integer width, so the exponent
   // accumulators clamp here rather than growing without bound. Sits far above the largest accepted
   // exponent (19, for uint64_t) and low enough that a clamped value cannot overflow uint32_t.
   inline constexpr uint32_t exponent_clamp = 1000;

   // Consumes the run of exponent digits starting at `c` and returns its value, clamped at
   // exponent_clamp. Clamping is what keeps the result meaningful: a narrow accumulator wraps mod its
   // width, which aliases an out-of-range exponent onto an accepted one. Every digit is consumed so
   // that leading zeros reach their true value -- JSON forbids a leading zero in the integer part but
   // permits any run of digits in the exponent, making "1e007" a valid spelling of 10^7 -- and so the
   // caller resumes past the whole exponent rather than mid-number.
   //
   // Requires *c to be a digit and the buffer to be terminated by a non-digit (the null terminator
   // counts); this scan is bounded by the input, not by a digit count.
   template <class Char>
   GLZ_ALWAYS_INLINE constexpr uint32_t parse_exponent(Char*& c) noexcept
   {
      uint32_t exp = uint32_t(*c - '0');
      ++c;
      while (is_digit(*c)) {
         // Written as a select rather than a branch: the clamp only ever engages on absurdly long
         // exponents, so a branch here would be a mispredict risk on the path that matters.
         exp = exp < exponent_clamp ? exp * 10 + uint32_t(*c - '0') : exp;
         ++c;
      }
      return exp;
   }

   // Computed overflow checks - used instead of lookup tables to save 4KB+ of binary size
   // Uses adjusted threshold for branch-free single comparison (7-12% faster than bitwise approach)
   template <class T>
   GLZ_ALWAYS_INLINE constexpr bool would_overflow_positive(std::remove_volatile_t<T> v, uint8_t next_digit) noexcept
   {
      using U = std::remove_volatile_t<T>;
      constexpr auto max_val = static_cast<uint64_t>((std::numeric_limits<U>::max)());
      constexpr auto threshold = max_val / 10;
      constexpr auto last_digit = max_val % 10;
      const auto uv = static_cast<uint64_t>(v);
      const auto digit = static_cast<uint64_t>(next_digit - '0');
      // When digit > last_digit, effective threshold is one less
      // This is branch-free and faster than bitwise OR/AND approach
      return uv > (threshold - uint64_t(digit > last_digit));
   }

   template <class T>
   GLZ_ALWAYS_INLINE constexpr bool would_overflow_negative(std::remove_volatile_t<T> v, uint8_t next_digit) noexcept
   {
      // For negative: max magnitude is max + 1 (e.g., -2147483648 for int32)
      using U = std::remove_volatile_t<T>;
      constexpr auto max_val = static_cast<uint64_t>((std::numeric_limits<U>::max)()) + 1;
      constexpr auto threshold = max_val / 10;
      constexpr auto last_digit = max_val % 10;
      const auto uv = static_cast<uint64_t>(v);
      const auto digit = static_cast<uint64_t>(next_digit - '0');
      // When digit > last_digit, effective threshold is one less
      return uv > (threshold - uint64_t(digit > last_digit));
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
      if consteval {
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
#elif defined(GLZ_FASTFLOAT_32BIT) || (defined(_WIN64) && !defined(__clang__) && !defined(__MINGW32__))
      answer.low = _umul128(a, b, &answer.high); // _umul128 not available on ARM64
#elif defined(GLZ_FASTFLOAT_64BIT) && defined(__SIZEOF_INT128__)
      __uint128_t r = ((__uint128_t)a) * b;
      answer.low = uint64_t(r);
      answer.high = uint64_t(r >> 64);
#else
      answer.low = umul128_generic(a, b, &answer.high);
#endif
      return answer;
   }

   template <std::integral T>
      requires(std::is_unsigned_v<T> && (sizeof(T) <= 8))
   GLZ_ALWAYS_INLINE constexpr const uint8_t* parse_int(T& v, const uint8_t* c) noexcept
   {
      if (is_digit(*c)) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return {};
      }

      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         return c;
      }

      if (c[-2] == '0') [[unlikely]] {
         return {};
      }

      if constexpr (sizeof(T) > 1) {
         if (is_digit(*c)) {
            v = v * 10 + (*c - '0');
            ++c;
         }
         else {
            return c;
         }

         if (is_digit(*c)) {
            v = v * 10 + (*c - '0');
            ++c;
         }
         else {
            return c;
         }

         if constexpr (sizeof(T) > 2) {
            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               return c;
            }

            if constexpr (sizeof(T) > 4) {
               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  return c;
               }
            }
         }
      }

      if (is_digit(*c)) {
         // Test before multiplying. The previous guard multiplied first and then compared the
         // already-wrapped value against max/10 - 10, which only caught wraps that happened to land
         // below that threshold: "300" wraps to 44 for uint8_t and sailed through, as did most
         // out-of-range values. would_overflow_positive is the same branch-free check the signed
         // path above already uses, so the two paths now reject on the same rule.
         if (would_overflow_positive<T>(v, *c)) [[unlikely]] {
            return {};
         }
         v = v * 10 + (*c - '0');
         ++c;
         if (is_digit(*c)) [[unlikely]] {
            return {};
         }
      }

      return c;
   }

   template <std::integral T, class Char>
      requires(std::is_unsigned_v<T>)
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, Char*& c) noexcept
   {
      if (auto ptr = parse_int(v, reinterpret_cast<const uint8_t*>(c))) [[likely]] {
         c = reinterpret_cast<const Char*>(ptr);
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

         if (not is_digit(*c)) [[unlikely]] {
            return false;
         }
         const uint32_t exp = parse_exponent(c);
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
         else if constexpr (sizeof(T) == 4) {
            if (exp > 9) [[unlikely]] {
               return false;
            }
         }
         else {
            if (exp > 19) [[unlikely]] {
               return false;
            }
         }

         if constexpr (sizeof(T) == 1) {
            static constexpr std::array<uint8_t, 3> powers_of_ten{1, 10, 100};
            const uint64_t i = v * powers_of_ten[exp];
            v = T(i);
            return i <= (std::numeric_limits<T>::max)();
         }
         else if constexpr (sizeof(T) == 2) {
            static constexpr std::array<uint16_t, 5> powers_of_ten{1, 10, 100, 1000, 10000};
            const uint64_t i = v * powers_of_ten[exp];
            v = T(i);
            return i <= (std::numeric_limits<T>::max)();
         }
         else if constexpr (sizeof(T) < 8) {
            const uint64_t i = v * powers_of_ten_int[exp];
            v = T(i);
            return i <= (std::numeric_limits<T>::max)();
         }
         else {
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
      }
      return false;
   }

   template <std::integral T>
      requires(std::is_signed_v<T> && (sizeof(T) <= 8))
   GLZ_ALWAYS_INLINE constexpr const uint8_t* parse_int(T& v, const uint8_t* c) noexcept
   {
      const uint8_t sign = *c == '-';
      c += sign;

      if (is_digit(*c)) [[likely]] {
         v = *c - '0';
         ++c;
      }
      else [[unlikely]] {
         return {};
      }

      if (is_digit(*c)) {
         v = v * 10 + (*c - '0');
         ++c;
      }
      else {
         if (sign) {
            v = -v;
         }
         return c;
      }

      if (c[-2] == '0') [[unlikely]] {
         return {};
      }

      if constexpr (sizeof(T) > 1) {
         if (is_digit(*c)) {
            v = v * 10 + (*c - '0');
            ++c;
         }
         else {
            if (sign) {
               v = -v;
            }
            return c;
         }

         if (is_digit(*c)) {
            v = v * 10 + (*c - '0');
            ++c;
         }
         else {
            if (sign) {
               v = -v;
            }
            return c;
         }

         if constexpr (sizeof(T) > 2) {
            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               if (sign) {
                  v = -v;
               }
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               if (sign) {
                  v = -v;
               }
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               if (sign) {
                  v = -v;
               }
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               if (sign) {
                  v = -v;
               }
               return c;
            }

            if (is_digit(*c)) {
               v = v * 10 + (*c - '0');
               ++c;
            }
            else {
               if (sign) {
                  v = -v;
               }
               return c;
            }

            if constexpr (sizeof(T) > 4) {
               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }

               if (is_digit(*c)) {
                  v = v * 10 + (*c - '0');
                  ++c;
               }
               else {
                  if (sign) {
                     v = -v;
                  }
                  return c;
               }
            }
         }
      }

      if (is_digit(*c)) {
         if (sign) {
            if (would_overflow_negative<T>(v, *c)) [[unlikely]] {
               return {};
            }
            v = -1 * v;
            v = v * 10 - (*c - '0');
         }
         else {
            if (would_overflow_positive<T>(v, *c)) [[unlikely]] {
               return {};
            }
            v = v * 10 + (*c - '0');
         }
         ++c;
         if (is_digit(*c)) [[unlikely]] {
            return {};
         }
         return c;
      }

      if (sign) {
         v = -v;
      }
      return c;
   }

   template <std::integral T, class Char>
      requires(std::is_signed_v<T>)
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, Char*& c) noexcept
   {
      using X = std::decay_t<T>;
      using utype = std::make_unsigned_t<X>;

      const uint8_t sign = *c == '-';
      if (auto ptr = parse_int(v, reinterpret_cast<const uint8_t*>(c))) [[likely]] {
         c = reinterpret_cast<const Char*>(ptr);
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

         if (not is_digit(*c)) [[unlikely]] {
            return false;
         }
         const uint32_t exp = parse_exponent(c);
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
         else if constexpr (sizeof(T) == 4) {
            if (exp > 9) [[unlikely]] {
               return false;
            }
         }
         else {
            if (exp > 18) [[unlikely]] {
               return false;
            }
         }

         utype i = utype((utype(v) ^ -sign) + sign);
         if constexpr (sizeof(T) < 8) {
            // Scale in a width the product cannot wrap, then range check before narrowing. Scaling
            // inside `utype` truncated first and checked afterwards, so an out-of-range magnitude
            // aliased onto an accepted one: "13e2" read as 20 for int8_t and "5e9" as 705032704 for
            // int32_t. The widest case here is a 4-byte magnitude scaled by 10^9, which stays well
            // inside uint64_t. The unsigned path already widens the same way.
            const uint64_t scaled = uint64_t(i) * powers_of_ten_int[exp];
            v = T((utype(scaled) ^ -sign) + sign);
            // Bound the magnitude directly rather than subtracting the sign from it: a negative
            // zero makes `scaled - sign` underflow, and the old narrow expression only survived
            // that because it promoted to int. A negative value may reach one past the positive
            // limit, which is exactly INT_MIN's magnitude.
            return scaled <= uint64_t((std::numeric_limits<T>::max)()) + sign;
         }
         else {
            // Scale the sign-stripped magnitude `i`, not the two's-complement bit pattern of `v`:
            // for a negative value that pattern is a huge unsigned number, so every negative
            // 64-bit integer written with an exponent ("-1e2") overflowed and was rejected. The
            // narrower branches above already scale `i`.
#if defined(__SIZEOF_INT128__)
            const __uint128_t res = __uint128_t(i) * powers_of_ten_int[exp];
            v = T((uint64_t(res) ^ -sign) + sign);
            // Compare the full 128-bit product. Narrowing it to 64 bits first would let an
            // out-of-range magnitude alias onto an accepted one, e.g. 9e36 truncating into range.
            return res <= __uint128_t(9223372036854775807ull + sign);
#else
            const auto res = full_multiplication(i, powers_of_ten_int[exp]);
            v = T((uint64_t(res.low) ^ -sign) + sign);
            return res.high == 0 && (uint64_t(res.low) <= (9223372036854775807ull + sign));
#endif
         }
      }
      return false;
   }

   // Increase by 8 to support exponentials
   inline constexpr std::array<size_t, 4> int_buffer_lengths{16, 16, 24, 32};

   template <std::integral T, class Char>
   GLZ_ALWAYS_INLINE constexpr bool atoi(T& v, const Char*& it, const Char* end) noexcept
   {
      // The number of characters needed at most for each type, rounded to nearest 8 bytes
      constexpr auto buffer_length = int_buffer_lengths[std::bit_width(sizeof(T)) - 1];
      // We copy the rest of the buffer, or buffer_length bytes, into a null terminated buffer. The
      // trailing byte is never written by the copy, so the null-terminated atoi below always halts
      // inside the array even when the input fills it: the exponent scan stops at a non-digit rather
      // than after a fixed digit count, and a value-initialized array alone would not terminate a
      // copy that covers every byte.
      std::array<char, buffer_length + 1> data{};
      const auto n = size_t(end - it);
      if (n > 0) [[likely]] {
         std::memcpy(data.data(), it, n < buffer_length ? n : buffer_length);

         const auto start = data.data();
         const auto* c = start;
         const auto valid = glz::atoi(v, c);
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
         // Clamp instead of wrapping: a uint8_t accumulator turns "1e256" into exponent 0, which
         // aliases an out-of-range magnitude onto an accepted one ("1e256" decoding as 1). The old
         // `exp < 128` guard could not catch that, since the wrap happened before the test, and it
         // also left `c` parked mid-number once it did trip.
         int32_t exp = 0;
         while (digit_table[uint8_t(*c)]) {
            if (exp < int32_t(exponent_clamp)) {
               exp = 10 * exp + (*c - '0');
            }
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

#if defined(_MSC_VER) && !defined(__clang__)
// restore disabled warnings
#pragma warning(pop)
#endif
