// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "dragonbox/dragonbox_to_chars.h"
#include "fmt/format.h"

#include <concepts>

namespace glz::dragonbox
{
   template <class T>
   concept Uint = std::same_as<T, uint32_t> || std::same_as<T, uint64_t>;
   
   template <Uint T>
   static constexpr std::uint32_t decimal_length(const T v) noexcept
   {
      if constexpr (std::same_as<T, std::uint32_t>) {
         // Function precondition: v is not a 10-digit number.
         // (f2s: 9 digits are sufficient for round-tripping.)
         // (d2fixed: We print 9-digit blocks.)
         assert(v < 1000000000);
         if (v >= 100000000) {
            return 9;
         }
         if (v >= 10000000) {
            return 8;
         }
         if (v >= 1000000) {
            return 7;
         }
         if (v >= 100000) {
            return 6;
         }
         if (v >= 10000) {
            return 5;
         }
         if (v >= 1000) {
            return 4;
         }
         if (v >= 100) {
            return 3;
         }
         if (v >= 10) {
            return 2;
         }
         return 1;
      }
      else {
         // This is slightly faster than a loop.
         // The average output length is 16.38 digits, so we check high-to-low.
         // Function precondition: v is not an 18, 19, or 20-digit number.
         // (17 digits are sufficient for round-tripping.)
         assert(v < 100000000000000000L);
         if (v >= 10000000000000000L) {
            return 17;
         }
         if (v >= 1000000000000000L) {
            return 16;
         }
         if (v >= 100000000000000L) {
            return 15;
         }
         if (v >= 10000000000000L) {
            return 14;
         }
         if (v >= 1000000000000L) {
            return 13;
         }
         if (v >= 100000000000L) {
            return 12;
         }
         if (v >= 10000000000L) {
            return 11;
         }
         if (v >= 1000000000L) {
            return 10;
         }
         if (v >= 100000000L) {
            return 9;
         }
         if (v >= 10000000L) {
            return 8;
         }
         if (v >= 1000000L) {
            return 7;
         }
         if (v >= 100000L) {
            return 6;
         }
         if (v >= 10000L) {
            return 5;
         }
         if (v >= 1000L) {
            return 4;
         }
         if (v >= 100L) {
            return 3;
         }
         if (v >= 10L) {
            return 2;
         }
         return 1;
      }
   }

   char *to_chars(auto &&val, char *buffer) noexcept
   {
      using V = std::decay_t<decltype(val)>;

      auto br = jkj::dragonbox::float_bits<V, jkj::dragonbox::default_float_traits<V>>(val);
      auto const exponent_bits = br.extract_exponent_bits();
      auto const s = br.remove_exponent_bits(exponent_bits);

      if (br.is_finite(exponent_bits)) {
         if (s.is_negative()) {
            *buffer = '-';
            ++buffer;
         }
         if (br.is_nonzero()) {
            auto decimal = to_decimal<V, jkj::dragonbox::default_float_traits<V>>(s, exponent_bits,
                                                                                  jkj::dragonbox::policy::sign::ignore);
            const auto significand = decimal.significand;
            const auto exponent = decimal.exponent;

            if (exponent == 0) {
               return fmt::detail::format_decimal(buffer, significand, decimal_length(significand)).end;
            }

            const int s_digits = decimal_length(significand);
            int output_exp = exponent + s_digits - 1;
            if (output_exp == 0) {
               return fmt::detail::write_significand(buffer, significand, s_digits, s_digits + exponent, '.');
            }
            else if (output_exp < 0 && output_exp > -4) {
               const auto fill_to = buffer - output_exp + 1;
               std::fill(buffer, fill_to, '0');
               *(buffer + 1) = '.';
               buffer = fill_to;
               return fmt::detail::format_decimal(buffer, significand, s_digits).end;
            }
            else if (output_exp > 0 && (output_exp - s_digits) < 3) {
               if (exponent >= 0) {
                  buffer = fmt::detail::format_decimal(buffer, significand, s_digits).end;
                  const auto fill_to = buffer + exponent;
                  std::fill(buffer, fill_to, '0');
                  return fill_to;
               }
               return fmt::detail::write_significand(buffer, significand, s_digits, s_digits + exponent, '.');
            }
            using V = std::decay_t<decltype(val)>;
            return jkj::dragonbox::to_chars_detail::to_chars<V, jkj::dragonbox::default_float_traits<V>>(
               significand, exponent, buffer);
         }
         else {
            *buffer = '0';
            return buffer + 1;
         }
      }
      else {
         if (s.has_all_zero_significand_bits()) {
            if (s.is_negative()) {
               *buffer = '-';
               ++buffer;
            }
            std::memcpy(buffer, "Infinity", 8);
            return buffer + 8;
         }
         else {
            std::memcpy(buffer, "nan", 3);
            return buffer + 3;
         }
      }
   }
}
