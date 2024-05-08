// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

#if GLZ_CHECK_INSTRUCTION(GLZ_AVX2)
namespace glz
{
   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAndNot(T0&& a, T1&& b) noexcept
   {
      return _mm256_andnot_si256(b, a);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAnd(T0&& a, T1&& b) noexcept
   {
      return _mm256_and_si256(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opXor(T0&& a, T1&& b) noexcept
   {
      return _mm256_xor_si256(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opOr(T0&& a, T1&& b) noexcept
   {
      return _mm256_or_si256(a, b);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE simd_t opSetLSB(T0&& a, bool b) noexcept
   {
      unwrap_t<simd_t> mask = _mm256_set_epi64x(0x00ll, 0x00ll, 0x00ll, 0x01ll);
      return b ? _mm256_or_si256(a, mask) : _mm256_andnot_si256(mask, a);
   }

   template <simd T>
   GLZ_ALWAYS_INLINE simd_t opNot(T&& a) noexcept
   {
      return _mm256_xor_si256(a, _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFll));
   }

   template <simd T>
   GLZ_ALWAYS_INLINE bool opGetMSB(T&& a) noexcept
   {
      simd_t result =
         _mm256_and_si256(a, _mm256_set_epi64x(0x8000000000000000ll, 0x00ll, 0x00ll, 0x00ll));
      return !_mm256_testz_si256(result, result);
   }

   template <simd T>
   GLZ_ALWAYS_INLINE bool opBool(T&& a) noexcept
   {
      return !_mm256_testz_si256(a, a);
   }

   GLZ_ALWAYS_INLINE simd_t reset() noexcept { return _mm256_setzero_si256(); }
}
#endif
