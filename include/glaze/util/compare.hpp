// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace glz
{
   template <class Char>
   inline bool compare(const Char* lhs, const Char* rhs, uint64_t count) noexcept
   {
      if (count > 7) {
         uint64_t v[2];
         while (count > 8) {
            std::memcpy(v, lhs, 8);
            std::memcpy(v + 1, rhs, 8);
            if (v[0] != v[1]) {
               return false;
            }
            count -= 8;
            lhs += 8;
            rhs += 8;
         }

         const auto shift = 8 - count;
         lhs -= shift;
         rhs -= shift;

         std::memcpy(v, lhs, 8);
         std::memcpy(v + 1, rhs, 8);
         return v[0] == v[1];
      }

      {
         constexpr uint64_t n{sizeof(uint32_t)};
         if (count >= n) {
            uint32_t v[2];
            std::memcpy(v, lhs, n);
            std::memcpy(v + 1, rhs, n);
            if (v[0] != v[1]) {
               return false;
            }
            count -= n;
            lhs += n;
            rhs += n;
         }
      }
      {
         constexpr uint64_t n{sizeof(uint16_t)};
         if (count >= n) {
            uint16_t v[2];
            std::memcpy(v, lhs, n);
            std::memcpy(v + 1, rhs, n);
            if (v[0] != v[1]) {
               return false;
            }
            count -= n;
            lhs += n;
            rhs += n;
         }
      }
      if (count && *lhs != *rhs) {
         return false;
      }
      return true;
   }

   // IMPORTANT:
   // This comparison function produces less binary than `compare` above and is very fast.
   // However, if our count is less than 8, we must be able to access the previous [8 - count] bytes.
   template <class Char>
   inline bool internal_compare(const Char* lhs, const Char* rhs, uint64_t count) noexcept
   {
      uint64_t v[2];
      while (count > 8) {
         std::memcpy(v, lhs, 8);
         std::memcpy(v + 1, rhs, 8);
         if (v[0] != v[1]) {
            return false;
         }
         count -= 8;
         lhs += 8;
         rhs += 8;
      }

      const auto shift = 8 - count;
      lhs -= shift;
      rhs -= shift;

      std::memcpy(v, lhs, 8);
      std::memcpy(v + 1, rhs, 8);
      return v[0] == v[1];
   }

   template <uint64_t Count, class Char>
   inline bool compare(const Char* lhs, const Char* rhs) noexcept
   {
      if constexpr (Count > 8) {
         return internal_compare(lhs, rhs, Count);
      }
      else if constexpr (Count == 8) {
         uint64_t v[2];
         std::memcpy(v, lhs, Count);
         std::memcpy(v + 1, rhs, Count);
         return v[0] == v[1];
      }
      else if constexpr (Count > 4) {
         uint64_t v[2]{}; // must default initialize
         std::memcpy(v, lhs, Count);
         std::memcpy(v + 1, rhs, Count);
         return v[0] == v[1];
      }
      else if constexpr (Count == 4) {
         uint32_t v[2];
         std::memcpy(v, lhs, Count);
         std::memcpy(v + 1, rhs, Count);
         return v[0] == v[1];
      }
      else if constexpr (Count == 3) {
         uint32_t v[2]{}; // must default initialize
         std::memcpy(v, lhs, Count);
         std::memcpy(v + 1, rhs, Count);
         return v[0] == v[1];
      }
      else if constexpr (Count == 2) {
         uint16_t v[2];
         std::memcpy(v, lhs, Count);
         std::memcpy(v + 1, rhs, Count);
         return v[0] == v[1];
      }
      else if constexpr (Count == 1) {
         return *lhs == *rhs;
      }
      else {
         return true;
      }
   }

   // compare_sv checks sizes
   inline constexpr bool compare_sv(const std::string_view lhs, const std::string_view rhs) noexcept
   {
      if (std::is_constant_evaluated()) {
         return lhs == rhs;
      }
      else {
         return (lhs.size() == rhs.size()) && compare(lhs.data(), rhs.data(), lhs.size());
      }
   }

   template <const std::string_view& lhs>
   inline constexpr bool compare_sv(const std::string_view rhs) noexcept
   {
      if (std::is_constant_evaluated()) {
         return lhs == rhs;
      }
      else {
         constexpr auto N = lhs.size();
         return (N == rhs.size()) && compare<N>(lhs.data(), rhs.data());
      }
   }
}
