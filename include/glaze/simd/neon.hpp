// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

namespace glz
{
#if GLZ_CHECK_INSTRUCTION(GLZ_NEON)

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAndNot(T0&& a, T1&& b) noexcept
   {
      return vbicq_u8(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAnd(T0&& a, T1&& b) noexcept
   {
      return vandq_u8(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opXor(T0&& a, T1&& b) noexcept
   {
      return veorq_u8(a, b);
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opOr(T0&& a, T1&& b) noexcept
   {
      return vorrq_u8(a, b);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE simd_t opSetLSB(T0&& a, bool b) noexcept
   {
      constexpr uint8x16_t mask{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      return b ? vorrq_u8(a, mask) : vbicq_u8(a, mask);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE simd_t opNot(T0&& a) noexcept
   {
      return vmvnq_u8(a);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE bool opGetMSB(T0&& a) noexcept
   {
      return (vgetq_lane_u8(a, 15) & 0x80) != 0;
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE bool opBool(T0&& a) noexcept
   {
      return vmaxvq_u8(a) != 0;
   }

   GLZ_ALWAYS_INLINE simd_t reset() noexcept { return {}; }

#endif
}
