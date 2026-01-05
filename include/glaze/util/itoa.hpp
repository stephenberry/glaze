// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Default integer to ascii conversion
// Uses 400 bytes of lookup tables (char_table + digit_pairs)
// For maximum speed with 40KB tables, use itoa_40kb.hpp instead

#include <concepts>
#include <cstdint>
#include <cstring>

#include "glaze/util/inline.hpp"

namespace glz
{
   namespace itoa_impl
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

      // 128-bit multiplication for efficient division by 100000000
      // Modern compilers optimize regular division the same way, but this ensures it
#if defined(__SIZEOF_INT128__)
      GLZ_ALWAYS_INLINE uint64_t div_1e8(uint64_t value) noexcept
      {
         constexpr __uint128_t multiplier = 0xabcc77118461cefdULL;
         constexpr uint64_t shift = 90;
         return static_cast<uint64_t>((static_cast<__uint128_t>(value) * multiplier) >> shift);
      }
#else
      GLZ_ALWAYS_INLINE uint64_t div_1e8(uint64_t value) noexcept { return value / 100000000ULL; }
#endif

      // ==================== uint32_t implementations ====================

      GLZ_ALWAYS_INLINE char* u32_2(char* buf, uint32_t val) noexcept
      {
         const uint32_t lz = val < 10;
         std::memcpy(buf, char_table + ((val * 2) | lz), 2);
         return buf + 2 - lz;
      }

      GLZ_ALWAYS_INLINE char* u32_4(char* buf, uint32_t val) noexcept
      {
         const uint32_t aa = (val * 5243) >> 19; // val / 100
         const uint32_t lz = aa < 10;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[val - aa * 100], 2);
         return buf + 4;
      }

      GLZ_ALWAYS_INLINE char* u32_6(char* buf, uint32_t val) noexcept
      {
         const uint32_t aa = uint32_t((uint64_t(val) * 429497) >> 32); // val / 10000
         const uint32_t bbcc = val - aa * 10000;
         const uint32_t bb = (bbcc * 5243) >> 19; // bbcc / 100
         const uint32_t lz = aa < 10;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[bb], 2);
         std::memcpy(buf + 4, &digit_pairs[bbcc - bb * 100], 2);
         return buf + 6;
      }

      GLZ_ALWAYS_INLINE char* u32_8(char* buf, uint32_t val) noexcept
      {
         const uint32_t aabb = uint32_t((uint64_t(val) * 109951163) >> 40); // val / 10000
         const uint32_t ccdd = val - aabb * 10000;
         const uint32_t aa = (aabb * 5243) >> 19; // aabb / 100
         const uint32_t cc = (ccdd * 5243) >> 19; // ccdd / 100
         const uint32_t lz = aa < 10;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[aabb - aa * 100], 2);
         std::memcpy(buf + 4, &digit_pairs[cc], 2);
         std::memcpy(buf + 6, &digit_pairs[ccdd - cc * 100], 2);
         return buf + 8;
      }

      GLZ_ALWAYS_INLINE char* u32_10(char* buf, uint32_t val) noexcept
      {
         const uint32_t aabbcc = uint32_t((uint64_t(val) * 3518437209ul) >> 45); // val / 10000
         const uint32_t aa = uint32_t((uint64_t(aabbcc) * 429497) >> 32); // aabbcc / 10000
         const uint32_t ddee = val - aabbcc * 10000;
         const uint32_t bbcc = aabbcc - aa * 10000;
         const uint32_t bb = (bbcc * 5243) >> 19;
         const uint32_t dd = (ddee * 5243) >> 19;
         const uint32_t lz = aa < 10;
         std::memcpy(buf, char_table + ((aa * 2) | lz), 2);
         buf -= lz;
         std::memcpy(buf + 2, &digit_pairs[bb], 2);
         std::memcpy(buf + 4, &digit_pairs[bbcc - bb * 100], 2);
         std::memcpy(buf + 6, &digit_pairs[dd], 2);
         std::memcpy(buf + 8, &digit_pairs[ddee - dd * 100], 2);
         return buf + 10;
      }

      // Fixed 8-digit output (no leading zero handling)
      GLZ_ALWAYS_INLINE char* u64_len_8(char* buf, uint32_t val) noexcept
      {
         const uint32_t aabb = uint32_t((uint64_t(val) * 109951163) >> 40);
         const uint32_t ccdd = val - aabb * 10000;
         const uint32_t aa = (aabb * 5243) >> 19;
         const uint32_t cc = (ccdd * 5243) >> 19;
         std::memcpy(buf, &digit_pairs[aa], 2);
         std::memcpy(buf + 2, &digit_pairs[aabb - aa * 100], 2);
         std::memcpy(buf + 4, &digit_pairs[cc], 2);
         std::memcpy(buf + 6, &digit_pairs[ccdd - cc * 100], 2);
         return buf + 8;
      }

      // Fixed 4-digit output (no leading zero handling)
      GLZ_ALWAYS_INLINE char* u64_len_4(char* buf, uint32_t val) noexcept
      {
         const uint32_t aa = (val * 5243) >> 19;
         std::memcpy(buf, &digit_pairs[aa], 2);
         std::memcpy(buf + 2, &digit_pairs[val - aa * 100], 2);
         return buf + 4;
      }

      // 1-8 digits with leading zero handling
      GLZ_ALWAYS_INLINE char* u64_len_1_8(char* buf, uint32_t val) noexcept
      {
         if (val < 100) {
            return u32_2(buf, val);
         }
         else if (val < 10000) {
            return u32_4(buf, val);
         }
         else if (val < 1000000) {
            return u32_6(buf, val);
         }
         else {
            return u32_8(buf, val);
         }
      }

      // 5-8 digits with leading zero handling
      GLZ_ALWAYS_INLINE char* u64_len_5_8(char* buf, uint32_t val) noexcept
      {
         if (val < 1000000) {
            return u32_6(buf, val);
         }
         else {
            return u32_8(buf, val);
         }
      }
   } // namespace itoa_impl

   // ==================== Public API ====================
   // Default integer to chars (uses 400 bytes of tables)
   // For maximum speed with 40KB tables, use to_chars_40kb from itoa_40kb.hpp

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, uint32_t>
   inline char* to_chars(char* buf, T val) noexcept
   {
      using namespace itoa_impl;
      if (val < 100) {
         return u32_2(buf, val);
      }
      else if (val < 10000) {
         return u32_4(buf, val);
      }
      else if (val < 1000000) {
         return u32_6(buf, val);
      }
      else if (val < 100000000) {
         return u32_8(buf, val);
      }
      else {
         return u32_10(buf, val);
      }
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, int32_t>
   inline char* to_chars(char* buf, T val) noexcept
   {
      *buf = '-';
      return to_chars(buf + (val < 0), uint32_t(val ^ (val >> 31)) - (val >> 31));
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, uint64_t>
   inline char* to_chars(char* buf, T val) noexcept
   {
      using namespace itoa_impl;
      if (val < 100000000) {
         return u64_len_1_8(buf, uint32_t(val));
      }
      else if (val < 10000000000000000ull) {
         const uint64_t hgh = div_1e8(val);
         const uint32_t low = uint32_t(val - hgh * 100000000);
         buf = u64_len_1_8(buf, uint32_t(hgh));
         return u64_len_8(buf, low);
      }
      else {
         const uint64_t tmp = div_1e8(val);
         const uint32_t low = uint32_t(val - tmp * 100000000);
         const uint32_t hgh = uint32_t(tmp / 10000);
         const uint32_t mid = uint32_t(tmp - uint64_t(hgh) * 10000);
         buf = u64_len_5_8(buf, hgh);
         buf = u64_len_4(buf, mid);
         return u64_len_8(buf, low);
      }
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, int64_t>
   inline char* to_chars(char* buf, T val) noexcept
   {
      *buf = '-';
      return to_chars(buf + (val < 0), uint64_t(val ^ (val >> 63)) - (val >> 63));
   }

   // Keep char_table in glz namespace for dtoa.hpp compatibility
   inline constexpr auto& char_table = itoa_impl::char_table;
} // namespace glz
