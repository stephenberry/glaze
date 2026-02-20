// Glaze Library
// For the license information refer to glaze.hpp

// sweethash - High performance string hashing
// sweet32 and sweet64 hash algorithms using SWAR (8 bytes at a time)
// Developed by Stephen Berry

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace glz::sweethash
{

template <class T>
concept contiguous_range = requires(const T& s) {
   s.data();
   s.size();
};

// Prime constants for mixing
inline constexpr uint64_t prime1 = 0x9E3779B97F4A7C15ull; // Golden ratio derived
inline constexpr uint64_t prime2 = 0xC6A4A7935BD1E995ull; // Used in MurmurHash64
inline constexpr uint64_t prime3 = 0x94D049BB133111EBull;
inline constexpr uint64_t prime4 = 0xBF58476D1CE4E5B9ull;
inline constexpr uint64_t prime5 = 0x165667B19E3779F9ull;

// Mix function for 64-bit values
[[nodiscard]] inline constexpr uint64_t mix64(uint64_t v) noexcept
{
   v = v ^ (v >> 33);
   v = v * prime2;
   v = v ^ (v >> 29);
   v = v * prime3;
   return v;
}

// sweet64: 64-bit hash function
[[nodiscard]] inline uint64_t sweet64(const void* data, size_t len, uint64_t seed = prime1) noexcept
{
   const uint8_t* p = static_cast<const uint8_t*>(data);

   // For inputs <= 8 bytes, we have at most 64 bits of input
   // Return bytes directly with seed - minimal perfect hash
   if (len <= 8) {
      uint64_t v = 0;
      switch (len) {
      case 8:
         std::memcpy(&v, p, 8);
         break;
      case 7:
         std::memcpy(&v, p, 7);
         break;
      case 6:
         std::memcpy(&v, p, 6);
         break;
      case 5:
         std::memcpy(&v, p, 5);
         break;
      case 4:
         std::memcpy(&v, p, 4);
         break;
      case 3:
         std::memcpy(&v, p, 3);
         break;
      case 2:
         std::memcpy(&v, p, 2);
         break;
      case 1:
         v = p[0];
         break;
      default:
         break;
      }
      // XOR with (len+1) to differentiate empty from "\0", etc.
      return v ^ seed ^ (len + 1);
   }

   uint64_t h = (seed ^ prime4) + (len * prime1);
   const uint8_t* const end = p + len;

   // For large inputs (>= 32 bytes), use 4-lane parallel processing
   if (len >= 32) {
      uint64_t h1 = h;
      uint64_t h2 = (seed * prime2) ^ prime3;
      uint64_t h3 = (seed * prime3) ^ prime4;
      uint64_t h4 = (seed * prime4) ^ prime5;

      do {
         uint64_t k1, k2, k3, k4;
         std::memcpy(&k1, p, 8);
         std::memcpy(&k2, p + 8, 8);
         std::memcpy(&k3, p + 16, 8);
         std::memcpy(&k4, p + 24, 8);

         k1 = k1 * prime2;
         k1 = k1 ^ (k1 >> 47);
         h1 = h1 ^ k1;
         h1 = h1 * prime1;

         k2 = k2 * prime3;
         k2 = k2 ^ (k2 >> 47);
         h2 = h2 ^ k2;
         h2 = h2 * prime2;

         k3 = k3 * prime4;
         k3 = k3 ^ (k3 >> 47);
         h3 = h3 ^ k3;
         h3 = h3 * prime3;

         k4 = k4 * prime5;
         k4 = k4 ^ (k4 >> 47);
         h4 = h4 ^ k4;
         h4 = h4 * prime4;

         p = p + 32;
         len = len - 32;
      } while (len >= 32);

      h = h1 + h2 + h3 + h4;
   }

   // Process remaining 8-byte chunks
   while (len >= 8) {
      uint64_t k;
      std::memcpy(&k, p, 8);
      k = k * prime2;
      k = k ^ (k >> 47);
      h = h ^ k;
      h = h * prime1;
      p = p + 8;
      len = len - 8;
   }

   // Process remaining 1-7 bytes by reading last 8 bytes (overlapping)
   if (len > 0) {
      uint64_t k;
      std::memcpy(&k, end - 8, 8);
      k = k * prime2;
      k = k ^ (k >> 47);
      k = k * prime3;
      h = h ^ k;
   }

   return mix64(h);
}

// sweet32: 32-bit hash function
[[nodiscard]] inline uint32_t sweet32(const void* data, size_t len,
                                      uint32_t seed = static_cast<uint32_t>(prime1)) noexcept
{
   const uint8_t* p = static_cast<const uint8_t*>(data);

   // For inputs <= 8 bytes, use simple hash (tail handler requires len >= 9)
   if (len <= 8) {
      uint64_t v = 0;
      switch (len) {
      case 8:
         std::memcpy(&v, p, 8);
         break;
      case 7:
         std::memcpy(&v, p, 7);
         break;
      case 6:
         std::memcpy(&v, p, 6);
         break;
      case 5:
         std::memcpy(&v, p, 5);
         break;
      case 4:
         std::memcpy(&v, p, 4);
         break;
      case 3:
         std::memcpy(&v, p, 3);
         break;
      case 2:
         std::memcpy(&v, p, 2);
         break;
      case 1:
         v = p[0];
         break;
      default:
         break;
      }
      v = v ^ seed ^ (len + 1);
      return static_cast<uint32_t>(v) ^ static_cast<uint32_t>(v >> 32);
   }

   // Full hash for inputs > 8 bytes
   uint64_t seed64 = (static_cast<uint64_t>(seed) << 32) | static_cast<uint64_t>(seed);
   uint64_t h = (seed64 ^ prime4) + (len * prime1);
   const uint8_t* const end = p + len;

   if (len >= 32) {
      uint64_t h1 = h;
      uint64_t h2 = (seed64 * prime2) ^ prime3;
      uint64_t h3 = (seed64 * prime3) ^ prime4;
      uint64_t h4 = (seed64 * prime4) ^ prime5;

      do {
         uint64_t k1, k2, k3, k4;
         std::memcpy(&k1, p, 8);
         std::memcpy(&k2, p + 8, 8);
         std::memcpy(&k3, p + 16, 8);
         std::memcpy(&k4, p + 24, 8);

         k1 = k1 * prime2;
         k1 = k1 ^ (k1 >> 47);
         h1 = h1 ^ k1;
         h1 = h1 * prime1;

         k2 = k2 * prime3;
         k2 = k2 ^ (k2 >> 47);
         h2 = h2 ^ k2;
         h2 = h2 * prime2;

         k3 = k3 * prime4;
         k3 = k3 ^ (k3 >> 47);
         h3 = h3 ^ k3;
         h3 = h3 * prime3;

         k4 = k4 * prime5;
         k4 = k4 ^ (k4 >> 47);
         h4 = h4 ^ k4;
         h4 = h4 * prime4;

         p = p + 32;
         len = len - 32;
      } while (len >= 32);

      h = h1 + h2 + h3 + h4;
   }

   while (len >= 8) {
      uint64_t k;
      std::memcpy(&k, p, 8);
      k = k * prime2;
      k = k ^ (k >> 47);
      h = h ^ k;
      h = h * prime1;
      p = p + 8;
      len = len - 8;
   }

   if (len > 0) {
      uint64_t k;
      std::memcpy(&k, end - 8, 8);
      k = k * prime2;
      k = k ^ (k >> 47);
      k = k * prime3;
      h = h ^ k;
   }

   h = mix64(h);
   return static_cast<uint32_t>(h) ^ static_cast<uint32_t>(h >> 32);
}

// Convenience overloads for string-like types
template <contiguous_range T>
[[nodiscard]] inline uint64_t sweet64(const T& s, uint64_t seed = prime1) noexcept
{
   return sweet64(static_cast<const void*>(s.data()), s.size(), seed);
}

template <contiguous_range T>
[[nodiscard]] inline uint32_t sweet32(const T& s, uint32_t seed = static_cast<uint32_t>(prime1)) noexcept
{
   return sweet32(static_cast<const void*>(s.data()), s.size(), seed);
}

} // namespace glz::sweethash
