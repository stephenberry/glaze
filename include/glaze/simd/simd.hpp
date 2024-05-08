// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>

#include "glaze/util/inline.hpp"
#include "glaze/util/type_traits.hpp"

#if defined(__arm64__) && __has_include(<arm_neon.h>)
#define GLZ_NEON
#endif

#ifdef GLZ_NEON
#include <arm_neon.h>
#else
#include <immintrin.h>
#endif

namespace glz
{
   struct unsupported {};
}

#if !defined(__arm64__) && __has_include(<emmintrin.h>)
#include <emmintrin.h>
#else
namespace glz
{
   using __m128i = unsupported;
   using __m256i = unsupported;
   using __m512i = unsupported;
   
   constexpr auto _mm_set_epi64x(auto, auto) { return 0; }
   constexpr auto _mm256_set_epi64x(auto, auto, auto, auto) { return 0; }
   constexpr auto _mm512_set_epi64(auto, auto, auto, auto, auto, auto, auto, auto) {
      return 0;
   }
   
   constexpr auto _mm_set1_epi64x(auto) { return 0; }
   constexpr auto _mm256_set1_epi64x(auto) { return 0; }
   constexpr auto _mm512_set1_epi64(auto) { return 0; }
   
   constexpr auto _mm_setzero_si128() { return 0; }
   constexpr auto _mm256_setzero_si256() { return 0; }
   constexpr auto _mm512_setzero_si512() { return 0; }
   
   constexpr auto _mm_load_si128(auto) { return 0; }
   constexpr auto _mm256_load_si256(auto) { return 0; }
   constexpr auto _mm512_load_si512(auto) { return 0; }
   
   constexpr auto _mm_loadu_si128(auto) { return 0; }
   constexpr auto _mm256_loadu_si256(auto) { return 0; }
   constexpr auto _mm512_loadu_si512(auto) { return 0; }
}
#endif

namespace glz
{
   enum struct simd : uint8_t
   {
      swar, // uses class  within a register
      avx,
      avx2,
      avx512,
      neon
   };
   
   template <class T, class...>
   struct first_t
   {
      using type = T;
   };

   template <class... T>
   using unwrap_t = std::remove_cvref_t<typename first_t<T...>::type>;
   
   template <simd Arch>
   struct arch;
   
   template <>
   struct arch<simd::swar>
   {
      static constexpr uint64_t bits_per_step{64};
      using simd_t = uint64_t;
      using string_parsing_type = uint16_t;
   };
   
   template <>
   struct arch<simd::avx>
   {
      static constexpr uint64_t bits_per_step{128};
      using simd_t = __m128i;
      using string_parsing_type = uint16_t;
   };
   
   template <>
   struct arch<simd::avx2>
   {
      static constexpr uint64_t bits_per_step{256};
      using simd_t = __m256i;
      using string_parsing_type = uint32_t;
   };
   
   template <>
   struct arch<simd::avx512>
   {
      static constexpr uint64_t bits_per_step{512};
      using simd_t = __m512i;
      using string_parsing_type = uint64_t;
   };
   
   template <>
   struct arch<simd::neon>
   {
      static constexpr uint64_t bits_per_step{128};
      using simd_t = uint8x16_t;
      using string_parsing_type = uint16_t;
   };
   
   template <simd Arch>
   constexpr uint64_t bytes_per_step() noexcept
   {
      return arch<Arch>::bits_per_step / 8;
   }
   
   template <simd Arch>
   constexpr uint64_t bits_per_step64() noexcept
   {
      return arch<Arch>::bits_per_step / 64;
   }
   
   template <simd Arch>
   constexpr uint64_t strides_per_step() noexcept
   {
      return arch<Arch>::bits_per_step / bytes_per_step<Arch>();
   }
   
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
      static constexpr Integer mask{ Mask };
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
   template <simd Arch, class T0, class T1>
   GLZ_ALWAYS_INLINE auto opAndNot(T0&& a, T1&& b) noexcept
   {
      using enum simd;
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
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
   template <simd Arch, class T0, class T1>
   GLZ_ALWAYS_INLINE auto opAnd(T0&& a, T1&& b) noexcept
   {
      using enum simd;
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
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
   template <simd Arch, class T0, class T1>
   GLZ_ALWAYS_INLINE auto opXor(T0&& a, T1&& b) noexcept
   {
      using enum simd;
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
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
      
   template <simd Arch, class T0, class T1>
   GLZ_ALWAYS_INLINE auto opOr(T0&& a, T1&& b) noexcept
   {
      using enum simd;
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
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
      
   template <simd Arch, class T0, class T1>
   GLZ_ALWAYS_INLINE auto opSetLSB(T0&& a, T1&& b) noexcept
   {
      using enum simd;
      using simd_t = typename arch<Arch>::simd_t;
      if constexpr (Arch == avx) {
         simd_t mask = _mm_set_epi64x(0x00ll, 0x01ll);
         return b ? _mm_or_si128(a, mask) : _mm_andnot_si128(mask, a);
      }
      else if constexpr (Arch == avx2) {
         simd_t mask = _mm256_set_epi64x(0x00ll, 0x00ll, 0x00ll, 0x01ll);
         return b ? _mm256_or_si256(a, mask) : _mm256_andnot_si256(mask, a);
      }
      else if constexpr (Arch == avx512) {
         simd_t mask =
            _mm512_set_epi64(0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x00ll, 0x01ll);
         return b ? _mm512_or_si512(a, mask) : _mm512_andnot_si512(mask, a);
      }
      else if constexpr (Arch == neon) {
         constexpr uint8x16_t mask{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
         return b ? vorrq_u8(a, mask) : vbicq_u8(a, mask);
      }
      else {
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
   template <simd Arch, class T>
   GLZ_ALWAYS_INLINE auto opNot(T&& a) noexcept
   {
      using enum simd;
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
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
   template <simd Arch, class T>
   GLZ_ALWAYS_INLINE auto opGetMSB(T&& a) noexcept
   {
      using enum simd;
      using simd_t = typename arch<Arch>::simd_t;
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
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
   template <simd Arch, class T>
   GLZ_ALWAYS_INLINE auto opBool(T&& a) noexcept
   {
      using enum simd;
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
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }

   template <simd Arch, class T>
   GLZ_ALWAYS_INLINE auto reset() noexcept
   {
      using enum simd;
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
         return std::decay_t<T>{};
      }
      else {
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
#ifdef GLZ_NEON
   template <class T>
   GLZ_ALWAYS_INLINE uint32_t compare_bit_mask(T&& value) noexcept
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
   
   template <simd Arch>
   GLZ_ALWAYS_INLINE auto opCmpEq(auto&& a, auto&& b) noexcept
   {
      using enum simd;
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
         return compare_bit_mask(vceqq_u8(a, b));
      }
      else {
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
   template <simd Arch>
   GLZ_ALWAYS_INLINE auto opShuffle(auto&& a, auto&& b) noexcept
   {
      using enum simd;
      if constexpr (Arch == avx) {
         return _mm_shuffle_epi8(a, b);
      }
      else if constexpr (Arch == avx2) {
         return _mm256_shuffle_epi8(a, b);
      }
      else if constexpr (Arch == avx512) {
         return _mm512_shuffle_epi8(a, b);
      }
      else if constexpr (Arch == neon) {
         constexpr uint8x16_t mask{0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                                          0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F};
         return vqtbl1q_u8(a, vandq_u8(b, mask));
      }
      else {
         static_assert(false_v<decltype(Arch)>, "class Architecture not supported");
      }
   }
   
   /*template <simd_unsigned T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      T ret;
      std::memcpy(&ret, str, sizeof(T));
      return ret;
   }*/
   
   template <simd Arch, class T, class Char>
   GLZ_ALWAYS_INLINE T vgather(Char* str) noexcept
   {
      using enum simd;
      if constexpr (Arch == avx) {
         return _mm_load_si128(reinterpret_cast<const __m128i*>(str));
      }
      else if constexpr (Arch == avx2) {
         return _mm256_load_si256(reinterpret_cast<const __m256i*>(str));
      }
      else if constexpr (Arch == avx512) {
         return _mm512_load_si512(str);
      }
      else if constexpr (Arch == neon) {
         if constexpr (simd_uint16<Char>) {
            return vreinterpretq_u8_u16(vld1q_u16(str));
         }
         else if constexpr (simd_uint64<Char>) {
            return vreinterpretq_u8_u64(vld1q_u64(str));
         }
         else {
            return vld1q_u8(str);
         }
      }
      else {
         using simd_t = typename arch<Arch>::simd_t;
         simd_t ret;
         std::memcpy(&ret, str, sizeof(simd_t));
         return ret;
      }
   }
   
   template <simd Arch, class T, class Char>
   GLZ_ALWAYS_INLINE T ugather(Char* str) noexcept
   {
      using enum simd;
      if constexpr (Arch == avx) {
         return _mm_loadu_si128(reinterpret_cast<const __m128i*>(str));
      }
      else if constexpr (Arch == avx2) {
         return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(str));
      }
      else if constexpr (Arch == avx512) {
         return _mm512_loadu_si512(reinterpret_cast<const __m512i*>(str));
      }
      else if constexpr (Arch == neon) {
         if constexpr (simd_char<Char>) {
            alignas(bytes_per_step<Arch>()) unsigned char arr[16];
            for (uint64_t x = 0; x < 16; ++x) {
               arr[x] = static_cast<unsigned char>(str[x]);
            }
            return vld1q_u8(arr);
         }
         else {
            return vld1q_u8(str);
         }
      }
      else {
         using simd_t = typename arch<Arch>::simd_t;
         simd_t ret;
         std::memcpy(&ret, str, sizeof(simd_t));
         return ret;
      }
   }
   
   template <simd Arch, class T, class Char>
   GLZ_ALWAYS_INLINE T gather(Char* str) noexcept
   {
      using enum simd;
      if constexpr (Arch == avx) {
         return _mm_set1_epi8(str);
      }
      else if constexpr (Arch == avx2) {
         return _mm256_set1_epi8(str);
      }
      else if constexpr (Arch == avx512) {
         return _mm512_set1_epi8(str);
      }
      else if constexpr (Arch == neon) {
         return vdupq_n_u8(str);
      }
      else {
         using simd_t = typename arch<Arch>::simd_t;
         simd_t ret{};
         std::memset(&ret, str, sizeof(simd_t));
         return ret;
      }
   }
   
   template <simd Arch, class T, class Char>
   GLZ_ALWAYS_INLINE void store(const T& value, Char* storage) noexcept
   {
      using enum simd;
      if constexpr (Arch == avx) {
         _mm_store_si128(reinterpret_cast<__m128i*>(storage), value);
      }
      else if constexpr (Arch == avx2) {
         _mm256_store_si256(reinterpret_cast<__m256i*>(storage), value);
      }
      else if constexpr (Arch == avx512) {
         _mm512_store_si512(storage, value);
      }
      else if constexpr (Arch == neon) {
         if constexpr (simd_uint8<Char>) {
            vst1q_u8(storage, value);
         }
         else {
            vst1q_u64(storage, vreinterpretq_u64_u8(value));
         }
      }
      else {
         using simd_t = typename arch<Arch>::simd_t;
         std::memcpy(storage, &value, sizeof(simd_t));
      }
   }
   
   template <std::integral T>
   GLZ_ALWAYS_INLINE T blsr(T a) noexcept
   {
#ifdef _MSC_VER
      if constexpr (simd_uint32<T>) {
         return _blsr_u32(a);
      }
      else if constexpr (simd_uint64<T>) {
         return _blsr_u64(a);
      }
#else
      return a & (a - 1);
#endif
   }
   
   // BMI2
   template <simd_uint32 T>
   GLZ_ALWAYS_INLINE T pdep(T&& a, T&& b) noexcept
   {
      return _pdep_u32(a, b);
   }

   // BMI2
   template <simd_uint64 T>
   GLZ_ALWAYS_INLINE T pdep(T&& a, T&& b) noexcept
   {
      return _pdep_u64(a, b);
   }
}

// Collect Indices
namespace glz
{
   template <class T>
   constexpr T simd_escapeable0 = []() consteval {
      constexpr T escapeable{0, 0, 0x22u, 0, 0, 0, 0, 0, 0x08u, 0, 0, 0, 0x0Cu, 0x0Du, 0, 0};
      return escapeable;
   }();
   template <class T>
   constexpr T simd_escapeable1 = []() consteval {
      constexpr T escapeable{0, 0, 0, 0, 0, 0, 0, 0, 0, 0x09u, 0x0Au, 0, 0x5Cu, 0, 0, 0};
      return escapeable;
   }();
   template <class T>
   constexpr T simd_whitespace = []() consteval{
      constexpr T whitespace{0x20u, 0x64u, 0x64u, 0x64u, 0x11u, 0x64u, 0x71u, 0x02u,
                                                0x64u, 0x09u, 0x0Au, 0x70u, 0x64u, 0x0Du, 0x64u, 0x64u};
      return whitespace;
   }();
   template <class T>
   constexpr T simd_backslashes = []() consteval{
      std::array<char, 16> t{};
      t.fill('\\');
      return std::bit_cast<T>(t);
   }();
   template <class T>
   constexpr T simd_op = []() consteval{
      constexpr std::array<char, 16> op{0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0x3Au, 0x7Bu, 0x2Cu, 0x7Du, 0, 0};
      return std::bit_cast<T>(op);
   }();
   template <class T>
   constexpr T simd_quotes = []() consteval{
      std::array<char, 16> t{};
      t.fill('"');
      return std::bit_cast<T>(t);
   }();
   template <class T>
   constexpr T simd_spaces = []() consteval{
      std::array<char, 16> t{};
      t.fill(' ');
      return std::bit_cast<T>(t);
   }();
}
