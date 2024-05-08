// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp""

#if GLZ_CHECK_INSTRUCTION(GLZ_AVX)
namespace glz
{
   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAndNot(T0&& a, T1&& b) noexcept
   {
      return _mm_andnot_si128(b, a);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAnd(T0&& a, T1&& b) noexcept
   {
      return _mm_and_si128(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opXor(T0&& a, T1&& b) noexcept
   {
      return _mm_xor_si128(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opOr(T0&& a, T1&& b) noexcept
   {
      return _mm_or_si128(a, b);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE simd_t opSetLSB(T0&& a, bool b) noexcept
   {
      unwrap_t<simd_t> mask = _mm_set_epi64x(0x00ll, 0x01ll);
      return b ? _mm_or_si128(a, mask) : _mm_andnot_si128(mask, a);
   }

   template <simd T>
   GLZ_ALWAYS_INLINE simd_t opNot(T&& a) noexcept
   {
      return _mm_xor_si128(a, _mm_set1_epi64x(0xFFFFFFFFFFFFFFFFll));
   }

   template <simd T>
   GLZ_ALWAYS_INLINE bool opGetMSB(T&& a) noexcept
   {
      simd_t res = _mm_and_si128(a, _mm_set_epi64x(0x8000000000000000ll, 0x00ll));
      return !_mm_testz_si128(res, res);
   }

   template <simd T>
   GLZ_ALWAYS_INLINE bool opBool(T&& a) noexcept
   {
      return !_mm_testz_si128(a, a);
   }

   GLZ_ALWAYS_INLINE simd_t reset() noexcept { return _mm_setzero_si128(); }
}
#endif
