// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "fmt/format.h"

namespace glz
{
   // An optimized string comparison algorithm that is typically faster than memcmp
   inline bool string_cmp(const std::string_view s0, const std::string_view s1) noexcept
   {
       const auto n = s0.size();
       if (s1.size() != n) {
           return false;
       }

       if (n < 8) {
         const auto* d0 = reinterpret_cast<const uint64_t*>(s0.data());
          const auto* d1 = reinterpret_cast<const uint64_t*>(s1.data());
          const auto shift = 64 - 8 * n;
          /*std::cout << fmt::format("{:b}\n", *d0);
          std::cout << fmt::format("{:b}\n", *d1);
          std::cout << fmt::format("{:b}\n", (*d0) << shift);
          std::cout << fmt::format("{:b}\n", (*d1) << shift);*/
          return ((*d0) << shift) == ((*d1) << shift);
         //return std::memcmp(s0.data(), s1.data(), n);
       }

       const auto* d0 = s0.data();
       const auto* d1 = s1.data();
       const auto* end7 = s0.end() - 7;

       for (; d0 < end7; d0 += 8, d1 += 8) {
         if (*reinterpret_cast<const uint64_t*>(d0) != *reinterpret_cast<const uint64_t*>(d1)) {
               return false;
           }
       }

       const uint64_t nm8 = n - 8;
       return (*reinterpret_cast<const uint64_t*>(s0.data() + nm8) == *reinterpret_cast<const uint64_t*>(s1.data() + nm8));
   }
}
