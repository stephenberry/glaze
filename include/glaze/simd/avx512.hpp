// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

#if GLZ_CHECK_INSTRUCTION(GLZ_AVX512)
namespace glz
{
   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAndNot(T0&& a, T1&& b) noexcept
   {
      return _mm512_andnot_si512(b, a);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAnd(T0&& a, T1&& b) noexcept
   {
      return _mm512_and_si512(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opXor(T0&& a, T1&& b) noexcept
   {
      return _mm512_xor_si512(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opOr(T0&& a, T1&& b) noexcept
   {
      return _mm512_or_si512(a, b);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE simd_t opSetLSB(T0&& a, bool valueNew) noexcept
   {
      unwrap_t<simd_t> mask =
         _mm512_set_epi64(0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x01ll);
      return valueNew ? _mm512_or_si512(a, mask) : _mm512_andnot_si512(mask, a);
   }

   template <simd T>
   GLZ_ALWAYS_INLINE simd_t opNot(T&& a) noexcept
   {
      return _mm512_xor_si512(a, _mm512_set1_epi64(0xFFFFFFFFFFFFFFFFll));
   }

   template <simd T>
   GLZ_ALWAYS_INLINE bool opGetMSB(T&& a) noexcept
   {
      simd_t result = _mm512_and_si512(
         a, _mm512_set_epi64(0x8000000000000000ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll));
      return _mm512_test_epi64_mask(result, result);
   }

   template <simd T>
   GLZ_ALWAYS_INLINE bool opBool(T&& a) noexcept
   {
      return _mm512_test_epi64_mask(a, a);
   }

   GLZ_ALWAYS_INLINE simd_t reset() noexcept { return _mm512_setzero_si512(); }
}
#endif
