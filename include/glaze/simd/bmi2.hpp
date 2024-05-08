// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

namespace glz
{
#if GLZ_CHECK_INSTRUCTION(GLZ_BMI2) || GLZ_CHECK_INSTRUCTION(GLZ_ANY_AVX)
   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T pdep(T&& a, T&& b) noexcept
   {
      return _pdep_u32(a, b);
   }

   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T pdep(T&& a, T&& b) noexcept
   {
      return _pdep_u64(a, b);
   }

#else

   template <simd_unsigned T>
   GLZ_ALWAYS_INLINE T pdep(T src, T mask) noexcept
   {
      T result = 0;
      T src_bit = 1;

      for (int32_t x = 0; x < 64; ++x) {
         if (mask & 1) {
            result |= (src & src_bit);
            src_bit <<= 1;
         }
         mask >>= 1;
      }

      return result;
   }
#endif
}
