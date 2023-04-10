// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <concepts>

#include "glaze/util/inline.hpp"

namespace glz::detail
{
   template <uint32_t N, std::unsigned_integral T>
   [[nodiscard]] inline constexpr auto set_bits(T y) {
       static_assert(N > 0 && N <= sizeof(T) * 8, "Invalid number of bits to write specified");
       // create a mask with N 1s
       constexpr auto mask = (1ul << N) - 1;
       
      T x{};
       // clear the bits in x that will be set by y
       x &= ~mask;
       
       // set the bits in x with y
       x |= y & mask;
      return x;
   }
   
   template <uint32_t N, std::unsigned_integral T>
   inline constexpr void set_bits(T& x, T y) {
       static_assert(N > 0 && N <= sizeof(T) * 8, "Invalid number of bits to write specified");
       // create a mask with N 1s
       constexpr auto mask = (1ul << N) - 1;
       
       // clear the bits in x that will be set by y
       x &= ~mask;
       
       // set the bits in x with y
       x |= y & mask;
   }

   template <uint32_t K, uint32_t N, std::unsigned_integral T>
   inline constexpr void set_bits(T& x, T y) {
       static_assert(K >= 0 && K <= sizeof(T) * 8, "Invalid number of bits to discard specified");
       static_assert(N > 0 && N <= sizeof(T) * 8 - K, "Invalid number of bits to write specified");
       // create a mask with N 1s starting from the K-th bit
       constexpr auto mask = ((1ul << N) - 1) << K;
       
       // clear the bits in x that will be set by y
       x &= ~mask;
       
       // set the bits in x with y
       x |= (y << K) & mask;
   }

   template <uint32_t N, std::unsigned_integral T>
   inline constexpr auto get_bits(T x) {
       static_assert(N > 0 && N <= sizeof(T) * 8, "Invalid number of bits to read specified");
       // Create a bit mask with N bits set
       constexpr auto mask = (1ul << N) - 1;

       // Extract the bits from x using the mask
       return x & mask;
   }

   template <uint32_t K, uint32_t N, std::unsigned_integral T>
   inline constexpr auto get_bits(T x) {
       static_assert(K >= 0 && K <= sizeof(T) * 8, "Invalid number of bits to discard specified");
       static_assert(N > 0 && N <= sizeof(T) * 8 - K, "Invalid number of bits to read specified");
       // Create a bit mask with N bits set starting at bit K
       constexpr auto mask = ((1ul << N) - 1) << K;

       // Extract the bits from x using the mask
       return (x & mask) >> K;
   }
   
   GLZ_ALWAYS_INLINE constexpr size_t int_from_compressed(auto&& it, auto&&) noexcept
   {
      uint8_t header;
      std::memcpy(&header, &(*it), 1);
      const uint8_t config = get_bits<2>(header);
      
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
   
   GLZ_ALWAYS_INLINE constexpr size_t byte_count(std::integral auto i) noexcept
   {
      switch (i) {
         case 0:
            return 1;
         case 1:
            return 2;
         case 2:
            return 4;
         case 3:
            return 8;
         case 4:
            return 16;
         case 5:
            return 32;
         case 6:
            return 64;
         case 7:
            return 128;
         default:
            return 0;
      }
   }
}
