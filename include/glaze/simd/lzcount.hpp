// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

namespace glz
{
#if GLZ_CHECK_INSTRUCTION(GLZ_LZCNT) || GLZ_CHECK_INSTRUCTION(GLZ_ANY_AVX)

   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T lzcnt(T&& a) noexcept
   {
      return _lzcnt_u32(a);
   }

   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T lzcnt(T&& a) noexcept
   {
      return _lzcnt_u64(a);
   }

#elif GLZ_CHECK_INSTRUCTION(GLZ_NEON)

   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T lzcnt(T&& a) noexcept
   {
#if defined(GLZ_REGULAR_VISUAL_STUDIO)
      unsigned long leading_zero = 0;
      if (_BitScanReverse32(&leading_zero, a)) {
         return 32 - leading_zero;
      }
      else {
         return 32;
      }
#else
      return __builtin_clz(a);
#endif
   }

   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T lzcnt(T&& a) noexcept
   {
#if defined(GLZ_REGULAR_VISUAL_STUDIO)
      unsigned long leading_zero = 0;
      if (_BitScanReverse64(&leading_zero, a)) {
         return 63 - leading_zero;
      }
      else {
         return 64;
      }
#else
      return __builtin_clzll(a);
#endif
   }

#else

   template <simd_unsigned T>
   constexpr T lzcnt(T&& a) noexcept
   {
      if (a == 0) {
         return sizeof(T) * 8;
      }

      T count{};
      T mask{static_cast<T>(1) << (std::numeric_limits<T>::digits - 1)};

      while ((a & mask) == 0) {
         ++count;
         mask >>= 1;
      }

      return count;
   }
#endif
}
