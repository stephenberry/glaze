// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "glaze/util/inline.hpp"

namespace glz::tag
{
   inline constexpr uint8_t null = 0;
   inline constexpr uint8_t boolean = 1;
   inline constexpr uint8_t number = 2;
   inline constexpr uint8_t string = 3;
   inline constexpr uint8_t object = 4;
   inline constexpr uint8_t typed_array = 5;
   inline constexpr uint8_t untyped_array = 6;
   inline constexpr uint8_t type = 7;
}

namespace glz::detail
{
   template <uint32_t N, std::unsigned_integral T>
   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr auto set_bits(T y)
   {
      static_assert(N > 0 && N <= sizeof(T) * 8, "Invalid number of bits to write specified");
      // create a mask with N 1s
      constexpr auto mask = (uint64_t(1) << N) - 1;

      T x{};
      // clear the bits in x that will be set by y
      x &= ~mask;

      // set the bits in x with y
      x |= y & mask;
      return x;
   }

   template <uint32_t N, std::unsigned_integral T>
   GLZ_ALWAYS_INLINE constexpr void set_bits(T& x, T y)
   {
      static_assert(N > 0 && N <= sizeof(T) * 8, "Invalid number of bits to write specified");
      // create a mask with N 1s
      constexpr auto mask = (uint64_t(1) << N) - 1;

      // clear the bits in x that will be set by y
      x &= ~mask;

      // set the bits in x with y
      x |= y & mask;
   }

   template <uint32_t K, uint32_t N, std::unsigned_integral T>
   GLZ_ALWAYS_INLINE constexpr void set_bits(T& x, T y)
   {
      static_assert(K >= 0 && K <= sizeof(T) * 8, "Invalid number of bits to discard specified");
      static_assert(N > 0 && N <= sizeof(T) * 8 - K, "Invalid number of bits to write specified");
      // create a mask with N 1s starting from the K-th bit
      constexpr auto mask = ((uint64_t(1) << N) - 1) << K;

      // clear the bits in x that will be set by y
      x &= ~mask;

      // set the bits in x with y
      x |= (y << K) & mask;
   }

   template <uint32_t K, uint32_t N, std::unsigned_integral T>
   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr auto set_bits(T&& x, T y)
   {
      static_assert(K >= 0 && K <= sizeof(T) * 8, "Invalid number of bits to discard specified");
      static_assert(N > 0 && N <= sizeof(T) * 8 - K, "Invalid number of bits to write specified");
      // create a mask with N 1s starting from the K-th bit
      constexpr auto mask = ((uint64_t(1) << N) - 1) << K;

      // clear the bits in x that will be set by y
      x &= ~mask;

      // set the bits in x with y
      x |= (y << K) & mask;
      return x;
   }

   template <uint32_t N, std::unsigned_integral T>
   GLZ_ALWAYS_INLINE constexpr auto get_bits(T x)
   {
      static_assert(N > 0 && N <= sizeof(T) * 8, "Invalid number of bits to read specified");
      // Create a bit mask with N bits set
      constexpr auto mask = (uint64_t(1) << N) - 1;

      // Extract the bits from x using the mask
      return x & mask;
   }

   template <uint32_t K, uint32_t N, std::unsigned_integral T>
   GLZ_ALWAYS_INLINE constexpr auto get_bits(T x)
   {
      static_assert(K >= 0 && K <= sizeof(T) * 8, "Invalid number of bits to discard specified");
      static_assert(N > 0 && N <= sizeof(T) * 8 - K, "Invalid number of bits to read specified");
      // Create a bit mask with N bits set starting at bit K
      constexpr auto mask = ((uint64_t(1) << N) - 1) << K;

      // Extract the bits from x using the mask
      return (x & mask) >> K;
   }

   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr size_t int_from_compressed(auto&& it, auto&&) noexcept
   {
      uint8_t header;
      std::memcpy(&header, &(*it), 1);
      const uint8_t config = uint8_t(get_bits<2>(header));

      switch (config) {
      case 0:
         ++it;
         return get_bits<2, 6>(header);
      case 1: {
         uint16_t h;
         std::memcpy(&h, &(*it), 2);
         std::advance(it, 2);
         return get_bits<2, 14>(h);
      }
      case 2: {
         uint32_t h;
         std::memcpy(&h, &(*it), 4);
         std::advance(it, 4);
         return get_bits<2, 30>(h);
      }
      case 3: {
         uint64_t h;
         std::memcpy(&h, &(*it), 8);
         std::advance(it, 8);
         return get_bits<2, 62>(h);
      }
      default:
         return 0;
      }
   }

   GLZ_ALWAYS_INLINE constexpr void skip_compressed_int(auto&& it, auto&&) noexcept
   {
      uint8_t header;
      std::memcpy(&header, &(*it), 1);
      const uint8_t config = uint8_t(get_bits<2>(header));

      switch (config) {
      case 0:
         ++it;
         return;
      case 1: {
         std::advance(it, 2);
         return;
      }
      case 2: {
         std::advance(it, 4);
         return;
      }
      case 3: {
         std::advance(it, 8);
         return;
      }
      default:
         return;
      }
   }

   template <class T>
   GLZ_ALWAYS_INLINE constexpr size_t to_byte_count() noexcept
   {
      return std::bit_width(sizeof(T)) - 1;
   }
}
