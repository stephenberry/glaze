// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

namespace glz
{
#if GLZ_CHECK_INSTRUCTION(GLZ_BMI) || GLZ_CHECK_INSTRUCTION(GLZ_ANY_AVX)
   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T blsr(T a) noexcept
   {
      return _blsr_u32(a);
   }

   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T blsr(T a) noexcept
   {
      return _blsr_u64(a);
   }

   template <simd_uint16 T>
   GLZ_ALWAYS_INLINE T tzcnt(T a) noexcept
   {
#if defined(GLZ_LINUX)
      return __tzcnt_u16(a);
#else
      return _tzcnt_u16(a);
#endif
   }

   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T tzcnt(T a) noexcept
   {
      return _tzcnt_u32(a);
   }

   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T tzcnt(T a) noexcept
   {
      return _tzcnt_u64(a);
   }

#elif GLZ_CHECK_INSTRUCTION(GLZ_NEON)

   template <simd_unsigned T>
   GLZ_ALWAYS_INLINE T blsr(T a) noexcept
   {
      return a & (a - 1);
   }

   template <simd_uint16 T>
   GLZ_ALWAYS_INLINE T tzcnt(T a) noexcept
   {
#if GLZ_REGULAR_VISUAL_STUDIO
      return _tzcnt_u16(a);
#else
      return __builtin_ctz(a);
#endif
   }

   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T tzcnt(T a) noexcept
   {
#if GLZ_REGULAR_VISUAL_STUDIO
      return _tzcnt_u32(a);
#else
      return __builtin_ctz(a);
#endif
   }

   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T tzcnt(T a) noexcept
   {
#if GLZ_REGULAR_VISUAL_STUDIO
      return _tzcnt_u64(a);
#else
      return __builtin_ctzll(a);
#endif
   }
#endif
}
