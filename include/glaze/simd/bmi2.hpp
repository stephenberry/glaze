// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

#if GLZ_CHECK_INSTRUCTION(GLZ_BMI2) || GLZ_CHECK_INSTRUCTION(GLZ_ANY_AVX)
namespace glz
{
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
}
#endif
