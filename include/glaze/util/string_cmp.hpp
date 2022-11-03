// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "fmt/format.h"

namespace glz
{
   // An optimized string comparison algorithm that is typically faster than memcmp
   inline bool string_cmp(auto&& s0, auto&& s1) noexcept
   {
       const auto n = s0.size();
       if (s1.size() != n) {
           return false;
       }

       if (n < 8) {
         const auto* d0 = reinterpret_cast<const uint64_t*>(s0.data());
          const auto* d1 = reinterpret_cast<const uint64_t*>(s1.data());
          const auto shift = 64 - 8 * n;
          return ((*d0) << shift) == ((*d1) << shift);
         //return std::memcmp(s0.data(), s1.data(), n);
       }

       const char* d0 = s0.data();
       const char* d1 = s1.data();
       const char* end7 = s0.data() + n - 7;

       for (; d0 < end7; d0 += 8, d1 += 8) {
         if (*reinterpret_cast<const uint64_t*>(d0) != *reinterpret_cast<const uint64_t*>(d1)) {
               return false;
           }
       }

       const uint64_t nm8 = n - 8;
       return (*reinterpret_cast<const uint64_t*>(s0.data() + nm8) == *reinterpret_cast<const uint64_t*>(s1.data() + nm8));
   }
}
