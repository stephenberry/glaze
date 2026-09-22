// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstdint>

#include "glaze/util/inline.hpp"

namespace glz
{
   // std::countr_zero uses another branch check whether the input is zero,
   // we use this function when we know that x > 0
   GLZ_ALWAYS_INLINE auto countr_zero(const uint32_t x) noexcept
   {
#ifdef _MSC_VER
      return std::countr_zero(x);
#else
#if __has_builtin(__builtin_ctzl)
      return __builtin_ctzl(x);
#else
      return std::countr_zero(x);
#endif
#endif
   }

   GLZ_ALWAYS_INLINE auto countr_zero(const uint64_t x) noexcept
   {
#ifdef _MSC_VER
      return std::countr_zero(x);
#else
#if __has_builtin(__builtin_ctzll)
      return __builtin_ctzll(x);
#else
      return std::countr_zero(x);
#endif
#endif
   }

#if defined(__SIZEOF_INT128__)
   GLZ_ALWAYS_INLINE auto countr_zero(__uint128_t x) noexcept
   {
      uint64_t low = uint64_t(x);
      if (low != 0) {
         return countr_zero(low);
      }
      else {
         uint64_t high = uint64_t(x >> 64);
         return countr_zero(high) + 64;
      }
   }
#endif

   // std::countl_zero uses another branch check whether the input is zero,
   // we use this function when we know that x > 0
   GLZ_ALWAYS_INLINE constexpr auto countl_zero(const uint32_t x) noexcept
   {
#ifdef _MSC_VER
      return std::countl_zero(x);
#else
#if __has_builtin(__builtin_clz)
      return __builtin_clz(x);
#else
      return std::countl_zero(x);
#endif
#endif
   }

   constexpr int int_log2(uint32_t x) noexcept { return 31 - glz::countl_zero(x | 1); }

   consteval uint32_t repeat_byte4(const auto repeat) { return uint32_t(0x01010101u) * uint8_t(repeat); }

   consteval uint64_t repeat_byte8(const uint8_t repeat) { return 0x0101010101010101ull * repeat; }

   // Byte-granular tests over a word, the SWAR technique from Mycroft's bit twiddling notes: a
   // byte that matches becomes zero after the xor, and a zero byte is the only one that borrows
   // into its own high bit. The result marks the low bit group of every match, so the first match
   // is the lowest set bit however many bytes matched.
   GLZ_ALWAYS_INLINE constexpr uint64_t has_zero(const uint64_t chunk) noexcept
   {
      return (((chunk - 0x0101010101010101ull) & ~chunk) & 0x8080808080808080ull);
   }

   GLZ_ALWAYS_INLINE constexpr uint64_t has_quote(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8('"'));
   }

   GLZ_ALWAYS_INLINE constexpr uint64_t has_escape(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8('\\'));
   }

   GLZ_ALWAYS_INLINE constexpr uint64_t has_space(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8(' '));
   }

   template <char Char>
   GLZ_ALWAYS_INLINE constexpr uint64_t has_char(const uint64_t chunk) noexcept
   {
      return has_zero(chunk ^ repeat_byte8(Char));
   }
}
