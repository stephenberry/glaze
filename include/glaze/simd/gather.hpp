// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

namespace glz
{
   template <simd_unsigned T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      T ret{};
      std::memcpy(&ret, str, sizeof(T));
      return ret;
   }

#if GLZ_CHECK_INSTRUCTION(GLZ_NEON)

   template <simd128 T, simd_uint16 Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      return vreinterpretq_u8_u16(vld1q_u16(str));
   }

   template <simd128 T, simd_uint64 Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      return vreinterpretq_u8_u64(vld1q_u64(str));
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      return vld1q_u8(str);
   }

   template <simd128 T, simd_char Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      GLZ_ALIGN unsigned char newArray[16];
      for (uint64_t x = 0; x < 16; ++x) {
         newArray[x] = static_cast<unsigned char>(str[x]);
      }
      return vld1q_u8(newArray);
   }

   template <simd128 T, simd_uchar Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      return vld1q_u8(str);
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      return vld1q_u8(str);
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T gather(Char str) noexcept
   {
      return vdupq_n_u8(str);
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE void store(const T& value, Char* storage) noexcept
   {
      vst1q_u64(storage, vreinterpretq_u64_u8(value));
   }

   template <simd128 T, simd_uint8 Char>
   GLZ_ALWAYS_INLINE void store(const T& value, Char* storage) noexcept
   {
      vst1q_u8(storage, value);
   }

#elif GLZ_CHECK_FOR_AVX(GLZ_AVX)

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      return _mm_load_si128(reinterpret_cast<const __m128i*>(str));
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      return _mm_loadu_si128(reinterpret_cast<const __m128i*>(str));
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T gather(Char str) noexcept
   {
      return _mm_set1_epi8(str);
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE void store(const T& value, Char* storage) noexcept
   {
      _mm_store_si128(reinterpret_cast<__m128i*>(storage), value);
   }

#if GLZ_CHECK_FOR_AVX(GLZ_AVX2)

   template <simd256 T, class Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      return _mm256_load_si256(reinterpret_cast<const __m256i*>(str));
   }

   template <simd256 T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(str));
   }

   template <simd256 T, class Char>
   GLZ_ALWAYS_INLINE T gather(Char str) noexcept
   {
      return _mm256_set1_epi8(str);
   }

   template <simd256 T, class Char>
   GLZ_ALWAYS_INLINE void store(const T& value, Char* storage) noexcept
   {
      _mm256_store_si256(reinterpret_cast<__m256i*>(storage), value);
   }

#if GLZ_CHECK_FOR_AVX(GLZ_AVX512)

   template <simd512 T, class Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      return _mm512_load_si512(str);
   }

   template <simd512 T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      return _mm512_loadu_si512(reinterpret_cast<const __m512i*>(str));
   }

   template <simd512 T, class Char>
   GLZ_ALWAYS_INLINE T gather(Char str) noexcept
   {
      return _mm512_set1_epi8(str);
   }

   template <simd512 T, class Char>
   GLZ_ALWAYS_INLINE void store(const T& value, Char* storage) noexcept
   {
      _mm512_store_si512(storage, value);
   }

#endif

#endif

#else

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      simd_t ret{};
      std::memcpy(&ret, str, sizeof(simd_t));
      return ret;
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      simd_t ret{};
      std::memcpy(&ret, str, sizeof(simd_t));
      return ret;
   }

   template <simd128 T, class Char>
   GLZ_ALWAYS_INLINE T gather(Char str) noexcept
   {
      simd_t ret{};
      std::memset(&ret, str, sizeof(simd_t));
      return ret;
   }

   template <class Char>
   GLZ_ALWAYS_INLINE void store(const simd_t& value, Char* storage) noexcept
   {
      std::memcpy(storage, &value, sizeof(simd_t));
   }

#endif
}
