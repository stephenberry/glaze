// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"

#if !GLZ_CHECK_INSTRUCTION(GLZ_AVX) && !GLZ_CHECK_INSTRUCTION(GLZ_AVX2) && !GLZ_CHECK_INSTRUCTION(GLZ_AVX512) && \
   !GLZ_CHECK_INSTRUCTION(GLZ_NEON)
namespace glz
{
   template <class T0, size_t... Is>
   GLZ_ALWAYS_INLINE string_parsing_type _mm128_movemask_epi8(T0&& a, std::index_sequence<Is...>&&) noexcept
   {
      string_parsing_type mask{0};
      ((mask |= (a.m128x_int8[Is] & 0x80) ? (1 << Is) : 0), ...);
      return mask;
   }

   template <class T0, class T1>
   GLZ_ALWAYS_INLINE simd_t _mm128_or_si128(T0&& a, T1&& b) noexcept
   {
      simd_t value{};
      memcpy(value.m128x_uint64, a.m128x_uint64, sizeof(value));
      value.m128x_uint64[0] |= b.m128x_uint64[0];
      value.m128x_uint64[1] |= b.m128x_uint64[1];
      return value;
   }

   template <class T0, class T1>
   GLZ_ALWAYS_INLINE simd_t _mm128_and_si128(T0&& a, T1&& b) noexcept
   {
      simd_t value{};
      memcpy(value.m128x_uint64, a.m128x_uint64, sizeof(value));
      value.m128x_uint64[0] &= b.m128x_uint64[0];
      value.m128x_uint64[1] &= b.m128x_uint64[1];
      return value;
   }

   template <class T0, class T1>
   GLZ_ALWAYS_INLINE simd_t _mm128_andnot_si128(T0&& a, T1&& b) noexcept
   {
      simd_t value{};
      memcpy(value.m128x_uint64, b.m128x_uint64, sizeof(value));
      value.m128x_uint64[0] &= ~a.m128x_uint64[0];
      value.m128x_uint64[1] &= ~a.m128x_uint64[1];
      return value;
   }

   template <class T0, class T1>
   GLZ_ALWAYS_INLINE simd_t _mm128_xor_si128(T0&& a, T1&& b) noexcept
   {
      simd_t value{};
      memcpy(value.m128x_uint64, a.m128x_uint64, sizeof(value));
      value.m128x_uint64[0] ^= b.m128x_uint64[0];
      value.m128x_uint64[1] ^= b.m128x_uint64[1];
      return value;
   }

   template <class T0, class T1, size_t... Is>
   GLZ_ALWAYS_INLINE simd_t _mm128_cmpeq_epi8(T0&& a, T1&& b, std::index_sequence<Is...>&&) noexcept
   {
      simd_t result{};
      ((result.m128x_int8[Is] = (a.m128x_int8[Is] == b.m128x_int8[Is]) ? 0xFF : 0), ...);
      return result;
   }

   GLZ_ALWAYS_INLINE bool _mm128_testz_si128(simd_t& a, simd_t& b) noexcept
   {
      a.m128x_uint64[0] &= b.m128x_uint64[0];
      a.m128x_uint64[1] &= b.m128x_uint64[1];
      return a.m128x_uint64[0] == 0 && a.m128x_uint64[1] == 0;
   }

   GLZ_ALWAYS_INLINE simd_t _mm128_set_epi64x(uint64_t a, uint64_t b) noexcept
   {
      simd_t returnValue{};
      std::memcpy(&returnValue.m128x_uint64[0], &b, sizeof(uint64_t));
      std::memcpy(&returnValue.m128x_uint64[1], &a, sizeof(uint64_t));
      return returnValue;
   }

   GLZ_ALWAYS_INLINE simd_t _mm128_set1_epi64x(uint64_t a) noexcept
   {
      simd_t returnValue{};
      std::memcpy(&returnValue.m128x_uint64[0], &a, sizeof(uint64_t));
      std::memcpy(&returnValue.m128x_uint64[1], &a, sizeof(uint64_t));
      return returnValue;
   }

   template <class T0, class T1, size_t... Is>
   GLZ_ALWAYS_INLINE simd_t _mm128_shuffle_epi8(T0&& a, T1&& b, std::index_sequence<Is...>) noexcept
   {
      simd_t result{};
      size_t index{};
      (((index = b.m128x_uint8[Is] & 0x0F), (result.m128x_uint8[Is] = a.m128x_uint8[index])), ...);
      return result;
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAndNot(T0&& value, T1&& other) noexcept
   {
      return _mm128_andnot_si128(std::forward<T1>(other), std::forward<T0>(value));
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAnd(T0&& value, T1&& other) noexcept
   {
      return _mm128_and_si128(std::forward<T0>(value), std::forward<T1>(other));
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opXor(T0&& value, T1&& other) noexcept
   {
      return _mm128_xor_si128(std::forward<T0>(value), std::forward<T1>(other));
   }

   template <simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opOr(T0&& value, T1&& other) noexcept
   {
      return _mm128_or_si128(std::forward<T0>(value), std::forward<T1>(other));
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE simd_t opSetLSB(T0&& value, bool valueNew) noexcept
   {
      unwrap_t<simd_t> mask = _mm128_set_epi64x(0x00ll, 0x01ll);
      return valueNew ? _mm128_or_si128(value, mask) : _mm128_andnot_si128(mask, value);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE simd_t opNot(T0&& value) noexcept
   {
      return _mm128_xor_si128(std::forward<T0>(value), _mm128_set1_epi64x(0xFFFFFFFFFFFFFFFFll));
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE bool opGetMSB(T0&& value) noexcept
   {
      simd_t result = _mm128_and_si128(std::forward<T0>(value), _mm128_set_epi64x(0x8000000000000000ll, 0x00ll));
      return !_mm128_testz_si128(result, result);
   }

   template <simd T0>
   GLZ_ALWAYS_INLINE bool opBool(T0&& value) noexcept
   {
      return !_mm128_testz_si128(value, value);
   }

   GLZ_ALWAYS_INLINE simd_t reset() noexcept { return simd_t{}; }
}
#endif
