// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <concepts>
#include <cstdint>

#include "glaze/util/inline.hpp"
#include "glaze/util/type_traits.hpp"

#if defined(__clang__)
#define GLZ_CLANG 1
#elif defined(__GNUC__) && defined(__llvm__)
#define GLZ_CLANG 1
#elif defined(__APPLE__) && defined(__clang__)
#define GLZ_CLANG 1
#elif defined(_MSC_VER)
#define GLZ_MSVC 1
#elif defined(__GNUC__) && !defined(__clang__)
#define GLZ_GNUCXX 1
#endif

#if defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))
#define GLZ_MAC 1
#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
#define GLZ_LINUX 1
#elif defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#define GLZ_WIN 1
#endif

#if !defined(GLZ_CPU_INSTRUCTIONS)
#define GLZ_CPU_INSTRUCTIONS 0
#endif

#if !defined GLZ_ALIGN
#define GLZ_ALIGN alignas(bytes_per_step)
#endif

#if !defined(GLZ_CHECK_INSTRUCTION)
#define GLZ_CHECK_INSTRUCTION(x) (GLZ_CPU_INSTRUCTIONS & x)
#endif

#if !defined(GLZ_CHECK_FOR_AVX)
#define GLZ_CHECK_FOR_AVX(x) (GLZ_CPU_INSTRUCTIONS >= x)
#endif

#if !defined(GLZ_POPCNT)
#define GLZ_POPCNT (1 << 0)
#endif
#if !defined(GLZ_LZCNT)
#define GLZ_LZCNT (1 << 1)
#endif
#if !defined(GLZ_BMI)
#define GLZ_BMI (1 << 2)
#endif
#if !defined(GLZ_BMI2)
#define GLZ_BMI2 (1 << 3)
#endif
#if !defined(GLZ_NEON)
#define GLZ_NEON (1 << 4)
#endif
#if !defined(GLZ_AVX)
#define GLZ_AVX (1 << 5)
#endif
#if !defined(GLZ_AVX2)
#define GLZ_AVX2 (1 << 6)
#endif
#if !defined(GLZ_AVX512)
#define GLZ_AVX512 (1 << 7)
#endif

#if !defined(GLZ_ANY)
#define GLZ_ANY (GLZ_AVX | GLZ_AVX2 | GLZ_AVX512 | GLZ_POPCNT | GLZ_BMI | GLZ_BMI2 | GLZ_LZCNT)
#endif

#if !defined(GLZ_ANY_AVX)
#define GLZ_ANY_AVX (GLZ_AVX | GLZ_AVX2 | GLZ_AVX512)
#endif

#if defined(_MSC_VER)
#define GLZ_VISUAL_STUDIO 1
#if defined(__clang__)
#define GLZ_CLANG_VISUAL_STUDIO 1
#else
#define GLZ_REGULAR_VISUAL_STUDIO 1
#endif
#endif

#if GLZ_CHECK_INSTRUCTION(GLZ_ANY)

#include <immintrin.h>

#endif

namespace glz
{
   enum struct simd_arch : uint8_t
   {
      swar, // uses SIMD within a register
      avx,
      avx2,
      avx512,
      neon
   };
   
   template <class T, class... T>
   struct first_t
   {
      using type = value_type;
   };

   template <class... T>
   using unwrap_t = std::remove_cvref_t<typename first_t<T...>::type>;

   union __m128x;

#if GLZ_CHECK_INSTRUCTION(GLZ_ANY_AVX)

   using simd128_t = __m128i;
   using simd256_t = __m256i;
   using simd512_t = __m512i;

#if GLZ_CHECK_INSTRUCTION(GLZ_AVX512)
   using simd_t = __m512i;
   constexpr uint64_t bits_per_step{512};
   using string_parsing_type = uint64_t;
#elif GLZ_CHECK_INSTRUCTION(GLZ_AVX2)
   using simd_t = __m256i;
   constexpr uint64_t bits_per_step{256};
   using string_parsing_type = uint32_t;
#elif GLZ_CHECK_INSTRUCTION(GLZ_AVX)
   using simd_t = __m128i;
   constexpr uint64_t bits_per_step{128};
   using string_parsing_type = uint16_t;
#endif
#elif GLZ_CHECK_INSTRUCTION(GLZ_NEON)

#include <arm_neon.h>

   using simd128_t = uint8x16_t;
   using simd256_t = uint32_t;
   using simd512_t = uint64_t;

   using simd_t = uint8x16_t;
   constexpr uint64_t bits_per_step{128};
   using string_parsing_type = uint16_t;
#else
   union __m128x {
#if GLZ_WIN
      int8_t m128x_int8[16]{};
      int16_t m128x_int16[8];
      int32_t m128x_int32[4];
      int64_t m128x_int64[2];
      uint8_t m128x_uint8[16];
      int16_t m128x_uint16[8];
      int32_t m128x_uint32[4];
      uint64_t m128x_uint64[2];
#else
      int64_t m128x_int64[2];
      int8_t m128x_int8[16]{};
      int16_t m128x_int16[8];
      int32_t m128x_int32[4];
      uint8_t m128x_uint8[16];
      int16_t m128x_uint16[8];
      int32_t m128x_uint32[4];
      uint64_t m128x_uint64[2];
#endif
   };
   using simd128_t = __m128x;
   using simd256_t = uint32_t;
   using simd512_t = uint64_t;

   using simd_t = __m128x;
   constexpr uint64_t bits_per_step{128};
   using string_parsing_type = uint16_t;
#endif

   constexpr uint64_t bytes_per_step{bits_per_step / 8};
   constexpr uint64_t bits_per_step64{bits_per_step / 64};
   constexpr uint64_t strides_per_step{bits_per_step / bytes_per_step};

   using string_view_ptr = const uint8_t*;
   using structural_index = const uint8_t*;
   using string_buffer_ptr = uint8_t*;

   template <class T>
   concept simd512 = std::same_as<simd512_t, std::decay_t<T>>;
   template <class T>
   concept simd256 = std::same_as<simd256_t, std::decay_t<T>>;
   template <class T>
   concept simd128 = std::same_as<simd128_t, std::decay_t<T>>;
   template <class T>
   concept simd = std::same_as<simd_t, std::decay_t<T>>;
   
   template <class T>
   concept simd_bool = std::same_as<unwrap_t<T>, bool>;
   
   template <class T>
   concept simd_char = std::same_as<unwrap_t<T>, char>;
   
   template <class T>
   concept simd_uchar = std::same_as<unwrap_t<T>, unsigned char>;

   template <class T>
   concept simd_uint8 = std::same_as<uint8_t, unwrap_t<T>>;

   template <class T>
   concept simd_uint16 = std::same_as<uint16_t, unwrap_t<T>>;

   template <class T>
   concept simd_uint32 = std::same_as<uint32_t, unwrap_t<T>>;

   template <class T>
   concept simd_uint64 = std::same_as<uint64_t, unwrap_t<T>>;
   
   /*template <class T>
   concept int8_type = std::same_as<int8_t, unwrap_t<T>>;

   template <class T>
   concept int16_type = std::same_as<int16_t, unwrap_t<T>>;

   template <class T>
   concept int32_type = std::same_as<int32_t, unwrap_t<T>>;

   template <class T>
   concept int64_type = std::same_as<int64_t, unwrap_t<T>>;*/

   template <class T>
	concept simd_unsigned = std::unsigned_integral<unwrap_t<T>> && !simd_bool<T>;
   
   template <class T>
   concept simd_integer = std::integral<unwrap_t<T>> && !simd_bool<T>;
   
   template <class... Ts>
   struct type_list;
   
   template <class T, class... Rest>
   struct type_list<T, Rest...> {
      using current_type = T;
      using remaining_types = type_list<Rest...>;
      static constexpr uint64_t size = 1 + sizeof...(Rest);
   };

   template <uint64_t BytesProcessed, class T, class Integer, Integer Mask>
   struct type_holder {
      static constexpr uint64_t bytesProcessed{ BytesProcessed };
      static constexpr integer_type_new mask{ Mask };
      using type         = T;
      using integer_type = Integer;
   };
   
   template <class T>
   struct type_list<T> {
      using current_type = T;
      static constexpr uint64_t size = 1;
   };
   
   template <class type_list, uint64_t Index>
   struct get_type_at_index;

   template <class T, class... Rest> struct get_type_at_index<type_list<T, Rest...>, 0> {
      using type = T;
   };

   template <class T, class... Rest, uint64_t Index>
   struct get_type_at_index<type_list<T, Rest...>, Index> {
      using type = typename get_type_at_index<type_list<Rest...>, Index - 1>::type;
   };
}

namespace glz
{
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAndNot(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return _mm_andnot_si128(b, a);
      }
      else if constexpr (Arch == avx2) {
         return _mm256_andnot_si256(b, a);
      }
      else if constexpr (Arch == avx512) {
         return _mm512_andnot_si512(b, a);
      }
      else if constexpr (Arch == neon) {
         return vbicq_u8(a, b);
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
   
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opAnd(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return _mm_and_si128(a, b);
      }
      else if constexpr (Arch == avx2) {
         return _mm256_and_si256(a, b);
      }
      else if constexpr (Arch == avx512) {
         return _mm512_and_si512(a, b);
      }
      else if constexpr (Arch == neon) {
         return vandq_u8(a, b);
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
   
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opXor(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return _mm_xor_si128(a, b);
      }
      else if constexpr (Arch == avx2) {
         return _mm256_xor_si256(a, b);
      }
      else if constexpr (Arch == avx512) {
         return _mm512_xor_si512(a, b);
      }
      else if constexpr (Arch == neon) {
         return veorq_u8(a, b);
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
      
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opOr(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return _mm_or_si128(a, b);
      }
      else if constexpr (Arch == avx2) {
         return _mm256_or_si256(a, b);
      }
      else if constexpr (Arch == avx512) {
         return _mm512_or_si512(a, b);
      }
      else if constexpr (Arch == neon) {
         return vorrq_u8(a, b);
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
      
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opSetLSB(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         unwrap_t<simd_t> mask = _mm_set_epi64x(0x00ll, 0x01ll);
         return b ? _mm_or_si128(a, mask) : _mm_andnot_si128(mask, a);
      }
      else if constexpr (Arch == avx2) {
         unwrap_t<simd_t> mask = _mm256_set_epi64x(0x00ll, 0x00ll, 0x00ll, 0x01ll);
         return b ? _mm256_or_si256(a, mask) : _mm256_andnot_si256(mask, a);
      }
      else if constexpr (Arch == avx512) {
         unwrap_t<simd_t> mask =
            _mm512_set_epi64(0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x01ll);
         return valueNew ? _mm512_or_si512(a, mask) : _mm512_andnot_si512(mask, a);
      }
      else if constexpr (Arch == neon) {
         constexpr uint8x16_t mask{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
         return b ? vorrq_u8(a, mask) : vbicq_u8(a, mask);
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
   
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opNot(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return _mm_xor_si128(a, _mm_set1_epi64x(0xFFFFFFFFFFFFFFFFll));
      }
      else if constexpr (Arch == avx2) {
         return _mm256_xor_si256(a, _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFll));
      }
      else if constexpr (Arch == avx512) {
         return _mm512_xor_si512(a, _mm512_set1_epi64(0xFFFFFFFFFFFFFFFFll));
      }
      else if constexpr (Arch == neon) {
         return vmvnq_u8(a);
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
   
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opGetMSB(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         simd_t res = _mm_and_si128(a, _mm_set_epi64x(0x8000000000000000ll, 0x00ll));
         return !_mm_testz_si128(res, res);
      }
      else if constexpr (Arch == avx2) {
         simd_t result =
            _mm256_and_si256(a, _mm256_set_epi64x(0x8000000000000000ll, 0x00ll, 0x00ll, 0x00ll));
         return !_mm256_testz_si256(result, result);
      }
      else if constexpr (Arch == avx512) {
         simd_t result = _mm512_and_si512(
            a, _mm512_set_epi64(0x8000000000000000ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll));
         return _mm512_test_epi64_mask(result, result);
      }
      else if constexpr (Arch == neon) {
         return (vgetq_lane_u8(a, 15) & 0x80) != 0;
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
   
   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t opBool(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return !_mm_testz_si128(a, a);
      }
      else if constexpr (Arch == avx2) {
         return !_mm256_testz_si256(a, a);
      }
      else if constexpr (Arch == avx512) {
         return _mm512_test_epi64_mask(a, a);
      }
      else if constexpr (Arch == neon) {
         return vmaxvq_u8(a) != 0;
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }

   template <simd_arch Arch, simd T0, simd T1>
   GLZ_ALWAYS_INLINE simd_t reset(T0&& a, T1&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return _mm_setzero_si128();
      }
      else if constexpr (Arch == avx2) {
         return _mm256_setzero_si256();
      }
      else if constexpr (Arch == avx512) {
         return _mm512_setzero_si512();
      }
      else if constexpr (Arch == neon) {
         return {};
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
   
#if GLZ_CHECK_INSTRUCTION(GLZ_NEON)
   template <simd128 T0>
   GLZ_ALWAYS_INLINE uint32_t compare_bit_mask(T0&& value) noexcept
   {
      static constexpr uint8x16_t bit_mask{0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
                                           0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
      auto minput = value & bit_mask;
      uint8x16_t tmp = vpaddq_u8(minput, minput);
      tmp = vpaddq_u8(tmp, tmp);
      tmp = vpaddq_u8(tmp, tmp);
      return vgetq_lane_u16(vreinterpretq_u16_u8(tmp), 0);
   }
#endif
   
   template <simd_arch Arch, class T>
   GLZ_ALWAYS_INLINE auto opCmpEq(T&& a, T&& b) noexcept
   {
      using enum simd_arch;
      if constexpr (Arch == avx) {
         return uint32_t(_mm_movemask_epi8(_mm_cmpeq_epi8(a, b)));
      }
      else if constexpr (Arch == avx2) {
         return uint32_t(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, b)));
      }
      else if constexpr (Arch == avx512) {
         return uint64_t(_mm512_cmpeq_epi8_mask(a, b));
      }
      else if constexpr (Arch == neon) {
         return compare_bit_mask(vceqq_u8(value, other));
      }
      else {
         static_assert(false_v<Arch>, "SIMD Architecture not supported");
      }
   }
}
