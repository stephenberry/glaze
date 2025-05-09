// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>
#include <span>
#include <string_view>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/util/compare.hpp"

#ifdef _MSC_VER
// Turn off broken warning from MSVC for << operator precedence
#pragma warning(push)
#pragma warning(disable : 4554)
#endif

// Notes on padding:
// We only need to do buffer extensions on very short keys (n < 8)
// Our static thread_local string_buffer always has enough padding for short strings (n < 8)
// Only short string_view keys from the primary buffer are potentially an issue
// Typically short keys are not going to be at the end of the buffer
// For valid keys we always have a quote and a null character '\0'
// Our key can be empty, which means we need 6 bytes of additional padding

// To provide a mechanism to short circuit hashing when we know an unknown key is provided
// we allow hashing algorithms to return the seed when a hash does not need to be performed.
// To improve performance, we can ensure that the seed never hashes with any of the buckets as well.

namespace glz
{
   template <class T1, class T2>
   struct pair
   {
      using first_type = T1;
      using second_type = T2;
      T1 first{};
      T2 second{};
   };

   template <class T1, class T2>
   pair(T1, T2) -> pair<T1, T2>;

   template <size_t I, pair_t T>
   constexpr decltype(auto) get(T&& p) noexcept
   {
      if constexpr (I == 0) {
         return p.first;
      }
      else if constexpr (I == 1) {
         return p.second;
      }
      else {
         static_assert(false_v<decltype(p)>, "Invalid get for pair");
      }
   }
}

namespace glz
{
   inline constexpr uint64_t to_uint64_n_below_8(const char* bytes, const size_t N) noexcept
   {
      static_assert(std::endian::native == std::endian::little);
      uint64_t res{};
      if (std::is_constant_evaluated()) {
         for (size_t i = 0; i < N; ++i) {
            res |= (uint64_t(uint8_t(bytes[i])) << (i << 3));
         }
      }
      else {
         switch (N) {
         case 1: {
            std::memcpy(&res, bytes, 1);
            break;
         }
         case 2: {
            std::memcpy(&res, bytes, 2);
            break;
         }
         case 3: {
            std::memcpy(&res, bytes, 3);
            break;
         }
         case 4: {
            std::memcpy(&res, bytes, 4);
            break;
         }
         case 5: {
            std::memcpy(&res, bytes, 5);
            break;
         }
         case 6: {
            std::memcpy(&res, bytes, 6);
            break;
         }
         case 7: {
            std::memcpy(&res, bytes, 7);
            break;
         }
         default: {
            // zero size case
            break;
         }
         }
      }
      return res;
   }

   template <size_t N = 8>
   constexpr uint64_t to_uint64(const char* bytes) noexcept
   {
      static_assert(N <= sizeof(uint64_t));
      static_assert(std::endian::native == std::endian::little);
      if (std::is_constant_evaluated()) {
         uint64_t res{};
         for (size_t i = 0; i < N; ++i) {
            res |= (uint64_t(uint8_t(bytes[i])) << (i << 3));
         }
         return res;
      }
      else if constexpr (N == 8) {
         uint64_t res;
         std::memcpy(&res, bytes, N);
         return res;
      }
      else {
         uint64_t res{};
         std::memcpy(&res, bytes, N);
         constexpr auto num_bytes = sizeof(uint64_t);
         constexpr auto shift = (uint64_t(num_bytes - N) << 3);
         if constexpr (shift == 0) {
            return res;
         }
         else {
            return (res << shift) >> shift;
         }
      }
   }

   // Check if a size_t value exists inside of a container like std::array<size_t, N>
   // Using a pointer and size rather than std::span for faster compile times
   constexpr bool contains(const size_t* data, const size_t size, const size_t val) noexcept
   {
      for (size_t i = 0; i < size; ++i) {
         if (data[i] == val) {
            return true;
         }
      }
      return false;
   }
}

#ifdef _MSC_VER
// restore disabled warning
#pragma warning(pop)
#endif
