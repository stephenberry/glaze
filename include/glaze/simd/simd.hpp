// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <concepts>
#include <cstdint>

#include "glaze/util/inline.hpp"

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
