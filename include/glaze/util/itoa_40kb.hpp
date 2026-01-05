// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Maximum speed integer to ascii conversion using 40KB lookup table
// Use to_chars from itoa.hpp for a smaller binary with 400B tables
// Based on techniques from:
// - https://github.com/ibireme/c_numconv_benchmark/blob/master/src/itoa/itoa_yy.c
// - https://github.com/RealTimeChris/Jsonifier (IToStr.hpp)

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>

#include "glaze/util/inline.hpp"

namespace glz
{
   namespace itoa_40kb_impl
   {
      // 2-digit character pairs table (200 bytes) - cache-line aligned
      alignas(64) inline constexpr char char_table[200] = {
         '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
         '1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
         '2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
         '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
         '4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
         '5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
         '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
         '7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
         '8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
         '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'};

      // 2-digit uint16_t table for direct 2-byte memcpy (100 × 2 = 200 bytes)
      alignas(64) inline constexpr uint16_t digit_pairs[100] = {
         0x3030, 0x3130, 0x3230, 0x3330, 0x3430, 0x3530, 0x3630, 0x3730, 0x3830, 0x3930,
         0x3031, 0x3131, 0x3231, 0x3331, 0x3431, 0x3531, 0x3631, 0x3731, 0x3831, 0x3931,
         0x3032, 0x3132, 0x3232, 0x3332, 0x3432, 0x3532, 0x3632, 0x3732, 0x3832, 0x3932,
         0x3033, 0x3133, 0x3233, 0x3333, 0x3433, 0x3533, 0x3633, 0x3733, 0x3833, 0x3933,
         0x3034, 0x3134, 0x3234, 0x3334, 0x3434, 0x3534, 0x3634, 0x3734, 0x3834, 0x3934,
         0x3035, 0x3135, 0x3235, 0x3335, 0x3435, 0x3535, 0x3635, 0x3735, 0x3835, 0x3935,
         0x3036, 0x3136, 0x3236, 0x3336, 0x3436, 0x3536, 0x3636, 0x3736, 0x3836, 0x3936,
         0x3037, 0x3137, 0x3237, 0x3337, 0x3437, 0x3537, 0x3637, 0x3737, 0x3837, 0x3937,
         0x3038, 0x3138, 0x3238, 0x3338, 0x3438, 0x3538, 0x3638, 0x3738, 0x3838, 0x3938,
         0x3039, 0x3139, 0x3239, 0x3339, 0x3439, 0x3539, 0x3639, 0x3739, 0x3839, 0x3939};

      // 4-digit uint32_t table for direct 4-byte memcpy (10000 × 4 = 40KB)
      inline constexpr auto digit_quads = []() consteval {
         std::array<uint32_t, 10000> table{};
         for (uint32_t i = 0; i < 10000; ++i) {
            table[i] = static_cast<uint32_t>(0x30 + (i / 1000)) |
                       (static_cast<uint32_t>(0x30 + ((i / 100) % 10)) << 8) |
                       (static_cast<uint32_t>(0x30 + ((i / 10) % 10)) << 16) |
                       (static_cast<uint32_t>(0x30 + (i % 10)) << 24);
         }
         return table;
      }();

      // 128-bit multiplication for efficient division by 100000000
#if defined(__SIZEOF_INT128__)
      GLZ_ALWAYS_INLINE uint64_t div_1e8(uint64_t value) noexcept
      {
         constexpr __uint128_t multiplier = 0xabcc77118461cefdULL;
         constexpr uint64_t shift = 90;
         return static_cast<uint64_t>((static_cast<__uint128_t>(value) * multiplier) >> shift);
      }
#else
      GLZ_ALWAYS_INLINE uint64_t div_1e8(uint64_t value) noexcept
      {
         return value / 100000000ULL;
      }
#endif

      // ==================== uint32_t implementations ====================

      GLZ_ALWAYS_INLINE char* u32_2(char* buf, uint32_t val) noexcept
      {
         const uint64_t lz = val < 10;
         // Use OR since val*2 is always even (bit 0 is 0)
         std::memcpy(buf, char_table + ((val * 2) | lz), 2);
         buf -= lz;
         return buf + 2;
      }

      GLZ_ALWAYS_INLINE char* u32_4(char* buf, uint32_t val) noexcept
      {
         const uint32_t aa = (val * 5243) >> 19;
         const uint64_t lz = val < 1000;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[val - aa * 100], 2);
         return buf + 4;
      }

      GLZ_ALWAYS_INLINE char* u32_6(char* buf, uint32_t val) noexcept
      {
         const uint32_t aa = static_cast<uint32_t>((static_cast<uint64_t>(val) * 429497ULL) >> 32);
         const uint64_t lz = val < 100000;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_quads[val - aa * 10000], 4);
         return buf + 6;
      }

      GLZ_ALWAYS_INLINE char* u32_8(char* buf, uint32_t val) noexcept
      {
         const uint64_t aabb = (static_cast<uint64_t>(val) * 109951163ULL) >> 40;
         const uint64_t aa = (aabb * 5243ULL) >> 19;
         const uint64_t lz = val < 10000000;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[aabb - aa * 100], 2);
         std::memcpy(buf + 4, &digit_quads[val - aabb * 10000], 4);
         return buf + 8;
      }

      GLZ_ALWAYS_INLINE char* u32_10(char* buf, uint32_t val) noexcept
      {
         const uint64_t high = div_1e8(val);
         const uint64_t low = val - high * 100000000;
         const uint64_t lz = high < 10;
         std::memcpy(buf, char_table + ((high * 2) | lz), 2);
         buf -= lz;
         const uint64_t aabb = (low * 109951163ULL) >> 40;
         const uint64_t ccdd = low - aabb * 10000;
         std::memcpy(buf + 2, &digit_quads[aabb], 4);
         std::memcpy(buf + 6, &digit_quads[ccdd], 4);
         return buf + 10;
      }

      // ==================== uint64_t implementations ====================

      GLZ_ALWAYS_INLINE char* u64_2(char* buf, uint64_t val) noexcept
      {
         const uint64_t lz = val < 10;
         std::memcpy(buf, char_table + ((val * 2) | lz), 2);
         buf -= lz;
         return buf + 2;
      }

      GLZ_ALWAYS_INLINE char* u64_4(char* buf, uint64_t val) noexcept
      {
         const uint64_t aa = (val * 5243ULL) >> 19;
         const uint64_t lz = val < 1000;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[val - aa * 100], 2);
         return buf + 4;
      }

      GLZ_ALWAYS_INLINE char* u64_6(char* buf, uint64_t val) noexcept
      {
         const uint64_t aa = (val * 429497ULL) >> 32;
         const uint64_t lz = val < 100000;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_quads[val - aa * 10000], 4);
         return buf + 6;
      }

      GLZ_ALWAYS_INLINE char* u64_8(char* buf, uint64_t val) noexcept
      {
         const uint64_t aabb = (val * 109951163ULL) >> 40;
         const uint64_t aa = (aabb * 5243ULL) >> 19;
         const uint64_t lz = val < 10000000;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[aabb - aa * 100], 2);
         std::memcpy(buf + 4, &digit_quads[val - aabb * 10000], 4);
         return buf + 8;
      }

      GLZ_ALWAYS_INLINE char* u64_10(char* buf, uint64_t val) noexcept
      {
         const uint64_t high = div_1e8(val);
         const uint64_t low = val - high * 100000000;
         const uint64_t lz = high < 10;
         std::memcpy(buf, char_table + ((high * 2) | lz), 2);
         buf -= lz;
         const uint64_t aabb = (low * 109951163ULL) >> 40;
         const uint64_t ccdd = low - aabb * 10000;
         std::memcpy(buf + 2, &digit_quads[aabb], 4);
         std::memcpy(buf + 6, &digit_quads[ccdd], 4);
         return buf + 10;
      }

      GLZ_ALWAYS_INLINE char* u64_12(char* buf, uint64_t val) noexcept
      {
         const uint64_t high = div_1e8(val);
         const uint64_t low = val - high * 100000000;
         const uint64_t aa = (high * 5243ULL) >> 19;
         const uint64_t lz = aa < 10;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[high - aa * 100], 2);
         const uint64_t aabb = (low * 109951163ULL) >> 40;
         const uint64_t ccdd = low - aabb * 10000;
         std::memcpy(buf + 4, &digit_quads[aabb], 4);
         std::memcpy(buf + 8, &digit_quads[ccdd], 4);
         return buf + 12;
      }

      GLZ_ALWAYS_INLINE char* u64_14(char* buf, uint64_t val) noexcept
      {
         const uint64_t high = div_1e8(val);
         const uint64_t low = val - high * 100000000;
         const uint64_t aa = (high * 429497ULL) >> 32;
         const uint64_t lz = aa < 10;
         const uint64_t bbcc = high - aa * 10000;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_quads[bbcc], 4);
         const uint64_t aabb = (low * 109951163ULL) >> 40;
         const uint64_t ccdd = low - aabb * 10000;
         std::memcpy(buf + 6, &digit_quads[aabb], 4);
         std::memcpy(buf + 10, &digit_quads[ccdd], 4);
         return buf + 14;
      }

      GLZ_ALWAYS_INLINE char* u64_16(char* buf, uint64_t val) noexcept
      {
         const uint64_t high = div_1e8(val);
         const uint64_t low = val - high * 100000000;
         uint64_t aabb = (high * 109951163ULL) >> 40;
         uint64_t ccdd = high - aabb * 10000;
         const uint64_t aa = (aabb * 5243ULL) >> 19;
         const uint64_t lz = aa < 10;
         const uint64_t bb = aabb - aa * 100;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[bb], 2);
         std::memcpy(buf + 4, &digit_quads[ccdd], 4);
         aabb = (low * 109951163ULL) >> 40;
         ccdd = low - aabb * 10000;
         std::memcpy(buf + 8, &digit_quads[aabb], 4);
         std::memcpy(buf + 12, &digit_quads[ccdd], 4);
         return buf + 16;
      }

      GLZ_ALWAYS_INLINE char* u64_18(char* buf, uint64_t val) noexcept
      {
         const uint64_t high = div_1e8(val);
         const uint64_t low = val - high * 100000000;
         const uint64_t high10 = div_1e8(high);
         const uint64_t low10 = high - high10 * 100000000;
         const uint64_t lz = high10 < 10;
         std::memcpy(buf, char_table + ((high10 * 2) | lz), 2);
         buf -= lz;
         uint64_t aabb = (low10 * 109951163ULL) >> 40;
         uint64_t ccdd = low10 - aabb * 10000;
         std::memcpy(buf + 2, &digit_quads[aabb], 4);
         std::memcpy(buf + 6, &digit_quads[ccdd], 4);
         aabb = (low * 109951163ULL) >> 40;
         ccdd = low - aabb * 10000;
         std::memcpy(buf + 10, &digit_quads[aabb], 4);
         std::memcpy(buf + 14, &digit_quads[ccdd], 4);
         return buf + 18;
      }

      GLZ_ALWAYS_INLINE char* u64_20(char* buf, uint64_t val) noexcept
      {
         const uint64_t high = div_1e8(val);
         const uint64_t low = val - high * 100000000;
         const uint64_t high12 = div_1e8(high);
         const uint64_t low12 = high - high12 * 100000000;
         const uint64_t aa = (high12 * 5243ULL) >> 19;
         const uint64_t lz = aa < 10;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[high12 - aa * 100], 2);
         uint64_t aabb = (low12 * 109951163ULL) >> 40;
         uint64_t ccdd = low12 - aabb * 10000;
         std::memcpy(buf + 4, &digit_quads[aabb], 4);
         std::memcpy(buf + 8, &digit_quads[ccdd], 4);
         aabb = (low * 109951163ULL) >> 40;
         ccdd = low - aabb * 10000;
         std::memcpy(buf + 12, &digit_quads[aabb], 4);
         std::memcpy(buf + 16, &digit_quads[ccdd], 4);
         return buf + 20;
      }
   } // namespace itoa_40kb_impl

   // ==================== Public API ====================
   // Maximum speed integer to chars (uses 40KB digit_quads table)
   // Use to_chars from itoa.hpp for smaller binary size

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, uint32_t>
   GLZ_ALWAYS_INLINE char* to_chars_40kb(char* buf, T val) noexcept
   {
      using namespace itoa_40kb_impl;
      if (val < 10000) {
         if (val < 100) return u32_2(buf, val);
         else return u32_4(buf, val);
      }
      else if (val < 100000000) {
         if (val < 1000000) return u32_6(buf, val);
         else return u32_8(buf, val);
      }
      else {
         return u32_10(buf, val);
      }
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, int32_t>
   GLZ_ALWAYS_INLINE char* to_chars_40kb(char* buf, T val) noexcept
   {
      *buf = '-';
      return to_chars_40kb(buf + (val < 0), static_cast<uint32_t>((static_cast<uint32_t>(val) ^ (val >> 31)) - (val >> 31)));
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, uint64_t>
   GLZ_ALWAYS_INLINE char* to_chars_40kb(char* buf, T val) noexcept
   {
      using namespace itoa_40kb_impl;
      if (val < 10000) {
         if (val < 100) return u64_2(buf, val);
         else return u64_4(buf, val);
      }
      else if (val < 100000000) {
         if (val < 1000000) return u64_6(buf, val);
         else return u64_8(buf, val);
      }
      else if (val < 1000000000000ULL) {
         if (val < 10000000000ULL) return u64_10(buf, val);
         else return u64_12(buf, val);
      }
      else if (val < 10000000000000000ULL) {
         if (val < 100000000000000ULL) return u64_14(buf, val);
         else return u64_16(buf, val);
      }
      else if (val < 1000000000000000000ULL) {
         return u64_18(buf, val);
      }
      else {
         return u64_20(buf, val);
      }
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, int64_t>
   GLZ_ALWAYS_INLINE char* to_chars_40kb(char* buf, T val) noexcept
   {
      *buf = '-';
      return to_chars_40kb(buf + (val < 0), static_cast<uint64_t>((static_cast<uint64_t>(val) ^ (val >> 63)) - (val >> 63)));
   }

} // namespace glz
