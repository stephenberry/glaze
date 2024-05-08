// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

namespace glz
{
#if GLZ_CHECK_INSTRUCTION(GLZ_POPCNT) || GLZ_CHECK_INSTRUCTION(GLZ_ANY_AVX)

   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T popcnt(T a) noexcept
   {
      return static_cast<uint32_t>(_mm_popcnt_u32(a));
   }

   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T popcnt(T a) noexcept
   {
      return static_cast<uint64_t>(_mm_popcnt_u64(a));
   }

#elif GLZ_CHECK_INSTRUCTION(GLZ_NEON)

   template <simd_unsigned T>
   GLZ_ALWAYS_INLINE T popcnt(T a) noexcept
   {
      return vaddv_u8(vcnt_u8(vcreate_u8(a)));
   }

#else

   template <simd_unsigned T>
   GLZ_ALWAYS_INLINE T popcnt(T a) noexcept
   {
      T count{};

      while (a > 0) {
         count += a & 1;
         a >>= 1;
      }

      return count;
   }

#endif
}
