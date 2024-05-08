// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/fallback.hpp"
#include "glaze/simd/neon.hpp"

namespace glz
{
#if GLZ_CHECK_INSTRUCTION(GLZ_NEON)

   template <simd128 T0, simd128 T1>
   GLZ_ALWAYS_INLINE simd128_t opShuffle(T0&& a, T1&& b) noexcept
   {
      constexpr uint8x16_t mask{0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                                       0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F};
      return vqtbl1q_u8(a, vandq_u8(b, mask));
   }

#elif GLZ_CHECK_FOR_AVX(GLZ_AVX)

   template <simd128 T0, simd128 T1>
   GLZ_ALWAYS_INLINE simd128_t opShuffle(T0&& a, T1&& b) noexcept
   {
      return _mm_shuffle_epi8(a, b);
   }

#if GLZ_CHECK_FOR_AVX(GLZ_AVX2)

   template <simd256 T0, simd256 T1>
   GLZ_ALWAYS_INLINE simd256_t opShuffle(T0&& a, T1&& b) noexcept
   {
      return _mm256_shuffle_epi8(a, b);
   }

#if GLZ_CHECK_FOR_AVX(GLZ_AVX512)

   template <simd512 T0, simd512 T1>
   GLZ_ALWAYS_INLINE simd512_t opShuffle(T0&& a, T1&& b) noexcept
   {
      return _mm512_shuffle_epi8(a, b);
   }

#endif

#endif

#else

   template <simd128 T0, simd128 T1>
   GLZ_ALWAYS_INLINE simd128_t opShuffle(T0&& a, T1&& b) noexcept
   {
      return _mm128_shuffle_epi8(a, b, std::make_index_sequence<16>{});
   }

#endif
}
