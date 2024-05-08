// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"
#include "glaze/simd/neon.hpp"

namespace glz
{
#if GLZ_CHECK_INSTRUCTION(GLZ_NEON)

   template <simd128 T0>
   GLZ_ALWAYS_INLINE uint32_t toBitMask(T0&& value) noexcept
   {
      static constexpr uint8x16_t bit_mask{0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
                                           0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
      auto minput = value & bit_mask;
      uint8x16_t tmp = vpaddq_u8(minput, minput);
      tmp = vpaddq_u8(tmp, tmp);
      tmp = vpaddq_u8(tmp, tmp);
      return vgetq_lane_u16(vreinterpretq_u16_u8(tmp), 0);
   }

   template <simd128 T0, simd128 T1>
   GLZ_ALWAYS_INLINE uint32_t opCmpEq(T0&& value, T1&& other) noexcept
   {
      return toBitMask(vceqq_u8(value, other));
   }

#elif GLZ_CHECK_FOR_AVX(GLZ_AVX)

   template <simd128 T0, simd128 T1>
   GLZ_ALWAYS_INLINE auto opCmpEq(T0&& a, T1&& b) noexcept
   {
      return uint32_t(_mm_movemask_epi8(_mm_cmpeq_epi8(a, b)));
   }

#if GLZ_CHECK_FOR_AVX(GLZ_AVX2)

   template <simd256 T0, simd256 T1>
   GLZ_ALWAYS_INLINE auto opCmpEq(T0&& a, T1&& b) noexcept
   {
      return static_cast<uint32_t>(
         _mm256_movemask_epi8(_mm256_cmpeq_epi8(a, b)));
   }

#if GLZ_CHECK_FOR_AVX(GLZ_AVX512)

   template <simd512 T0, simd512 T1>
   GLZ_ALWAYS_INLINE auto opCmpEq(T0&& a, T1&& b) noexcept
   {
      return uint64_t(_mm512_cmpeq_epi8_mask(a, b));
   }

#endif

#endif

#else

   template <simd128 T0, simd128 T1>
   GLZ_ALWAYS_INLINE uint32_t opCmpEq(T0&& a, T1&& b) noexcept
   {
      return uint32_t(_mm128_movemask_epi8(
         _mm128_cmpeq_epi8(a, b, std::make_index_sequence<16>{}),
         std::make_index_sequence<16>{}));
   }

#endif
}
