// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// This is an adapter to speed up the parsing portion of the fast_float conversion

#include "glaze/util/fast_float.hpp"
#include "glaze/util/inline.hpp"

namespace glz
{
   // Assuming that you use no more than 19 digits, this will
   // parse an ASCII string.
   template <class UC>
   GLZ_ALWAYS_INLINE constexpr fast_float::parsed_number_string_t<UC> parse_number_string(
      UC const* p, UC const* pend, fast_float::parse_options_t<UC> options) noexcept
   {
      using namespace fast_float;
      const chars_format fmt = options.format;
      static constexpr UC decimal_point = '.';

      parsed_number_string_t<UC> answer;
      answer.valid = false;
      answer.too_many_digits = false;
      answer.negative = (*p == UC('-'));
      if (*p == UC('-')) { // C++17 20.19.3.(7.1) explicitly forbids '+' sign here
         ++p;
         if (!is_integer(*p)) { // a sign must be followed by an integer
            return answer;
         }
      }
      UC const* const start_digits = p;

      uint64_t i = 0; // an unsigned int avoids signed overflows (which are bad)

      while (is_integer(*p)) {
         // a multiplication by 10 is cheaper than an arbitrary integer
         // multiplication
         i = 10 * i + uint64_t(*p - UC('0')); // might overflow, we will handle the overflow later
         ++p;
      }
      UC const* const end_of_integer_part = p;
      int64_t digit_count = int64_t(end_of_integer_part - start_digits);
      answer.integer = fast_float::span<const UC>(start_digits, size_t(digit_count));
      
      // at least 1 digit in integer part, without leading zeros
      if (digit_count == 0 || (start_digits[0] == UC('0') && digit_count > 1)) {
         return answer;
      }

      int64_t exponent = 0;
      const bool has_decimal_point = (*p == decimal_point);
      if (has_decimal_point) {
         ++p;
         UC const* before = p;
         // can occur at most twice without overflowing, but let it occur more, since
         // for integers with many digits, digit parsing is the primary bottleneck.
         loop_parse_if_eight_digits(p, pend, i);

         while ((p != pend) && is_integer(*p)) {
            uint8_t digit = uint8_t(*p - UC('0'));
            ++p;
            i = i * 10 + digit; // in rare cases, this will overflow, but that's ok
         }
         exponent = before - p;
         answer.fraction = fast_float::span<const UC>(before, size_t(p - before));
         digit_count -= exponent;
      }
      // at least 1 digit in fractional part
      if (has_decimal_point && exponent == 0) {
         return answer;
      }
      
      int64_t exp_number = 0; // explicit exponential part
      if ((UC('e') == *p) || (UC('E') == *p)) {
         UC const* location_of_e = p;
         if ((UC('e') == *p) || (UC('E') == *p) || (UC('d') == *p) || (UC('D') == *p)) {
            ++p;
         }
         bool neg_exp = false;
         if ((p != pend) && (UC('-') == *p)) {
            neg_exp = true;
            ++p;
         }
         else if ((p != pend) && (UC('+') == *p)) { // '+' on exponent is allowed by C++17 20.19.3.(7.1)
            ++p;
         }
         if ((p == pend) || !is_integer(*p)) {
            if (!(fmt & chars_format::fixed)) {
               // We are in error.
               return answer;
            }
            // Otherwise, we will be ignoring the 'e'.
            p = location_of_e;
         }
         else {
            while ((p != pend) && is_integer(*p)) {
               uint8_t digit = uint8_t(*p - UC('0'));
               if (exp_number < 0x10000000) {
                  exp_number = 10 * exp_number + digit;
               }
               ++p;
            }
            if (neg_exp) {
               exp_number = -exp_number;
            }
            exponent += exp_number;
         }
      }
      answer.lastmatch = p;
      answer.valid = true;

      // If we frequently had to deal with long strings of digits,
      // we could extend our code by using a 128-bit integer instead
      // of a 64-bit integer. However, this is uncommon.
      //
      // We can deal with up to 19 digits.
      if (digit_count > 19) { // this is uncommon
         // It is possible that the integer had an overflow.
         // We have to handle the case where we have 0.0000somenumber.
         // We need to be mindful of the case where we only have zeroes...
         // E.g., 0.000000000...000.
         UC const* start = start_digits;
         while ((start != pend) && (*start == UC('0') || *start == decimal_point)) {
            if (*start == UC('0')) {
               digit_count--;
            }
            start++;
         }

         if (digit_count > 19) {
            answer.too_many_digits = true;
            // Let us start again, this time, avoiding overflows.
            // We don't need to check if is_integer, since we use the
            // pre-tokenized spans from above.
            i = 0;
            p = answer.integer.ptr;
            UC const* int_end = p + answer.integer.len();
            const uint64_t minimal_nineteen_digit_integer{1000000000000000000};
            while ((i < minimal_nineteen_digit_integer) && (p != int_end)) {
               i = i * 10 + uint64_t(*p - UC('0'));
               ++p;
            }
            if (i >= minimal_nineteen_digit_integer) { // We have a big integers
               exponent = end_of_integer_part - p + exp_number;
            }
            else { // We have a value with a fractional component.
               p = answer.fraction.ptr;
               UC const* frac_end = p + answer.fraction.len();
               while ((i < minimal_nineteen_digit_integer) && (p != frac_end)) {
                  i = i * 10 + uint64_t(*p - UC('0'));
                  ++p;
               }
               exponent = answer.fraction.ptr - p + exp_number;
            }
            // We have now corrected both exponent and i, to a truncated value
         }
      }
      answer.exponent = exponent;
      answer.mantissa = i;
      return answer;
   }

   template <class T, class UC>
   constexpr fast_float::from_chars_result_t<UC> from_chars_advanced(UC const* first, UC const* last, T& value,
                                                                     fast_float::parse_options_t<UC> options) noexcept
   {
      using namespace fast_float;
      static_assert(is_supported_float_type<T>(), "only some floating-point types are supported");
      static_assert(is_supported_char_type<UC>(), "only char, wchar_t, char16_t and char32_t are supported");

      parsed_number_string_t<UC> pns = glz::parse_number_string<UC>(first, last, options);
      if (!pns.valid) [[unlikely]] {
         from_chars_result_t<UC> answer;
         answer.ec = std::errc::invalid_argument;
         answer.ptr = first;
         return answer;
      }

      return from_chars_advanced(pns, value);
   }
}
