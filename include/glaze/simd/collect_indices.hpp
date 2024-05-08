// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>

#include "glaze/simd/compare.hpp"
#include "glaze/simd/gather.hpp"
#include "glaze/simd/shuffle.hpp"

namespace glz
{
   template <class Ret, class T, size_t N, size_t... Is>
   constexpr Ret create_array(const T (&arr)[N], std::index_sequence<Is...>) noexcept
   {
      return Ret{arr[Is % N]...};
   }

   template <simd_unsigned Ret, class T, size_t N, size_t... Is>
   constexpr Ret create_array(const T (&arr)[N], std::index_sequence<Is...>) noexcept
   {
      Ret returnValues{};
      std::copy(arr, arr + 1, &returnValues);
      return returnValues;
   }

   template <class Ret>
   constexpr Ret simdFromValue(uint8_t value) noexcept
   {
#if defined(GLZ_LINUX) || (defined(GLZ_WIN) && defined(GLZ_CLANG))
      constexpr uint64_t valueSize{sizeof(uint64_t)};
      int64_t arr[16 / sizeof(int64_t)]{};
      for (uint64_t x = 0; x < 16; ++x) {
         arr[x / sizeof(int64_t)] |= static_cast<int64_t>(value) << ((x % 8) * 8);
      }
#elif defined(GLZ_MAC)
      constexpr uint64_t valueSize{sizeof(char)};
      unsigned char arr[16]{};
      std::fill(arr, arr + 16, static_cast<char>(value));
#elif defined(GLZ_WIN)
      constexpr uint64_t valueSize{sizeof(char)};
      char arr[16]{};
      std::fill(arr, arr + 16, static_cast<char>(value));
#endif
      Ret returnValue{
         create_array<Ret>(arr, std::make_index_sequence<sizeof(Ret) / valueSize>{})};
      return returnValue;
   }

   template <class Ret>
   constexpr Ret simdFromTable(const std::array<char, 16> values) noexcept
   {
#if defined(GLZ_LINUX) || (defined(GLZ_WIN) && defined(GLZ_CLANG))
      constexpr uint64_t valueSize{sizeof(uint64_t)};
      int64_t arr[16 / sizeof(int64_t)]{};
      for (uint64_t x = 0; x < 16; ++x) {
         arr[x / sizeof(int64_t)] |= static_cast<int64_t>(values[x % 16]) << ((x % 8) * 8);
      }
#elif defined(GLZ_MAC)
      constexpr uint64_t valueSize{sizeof(char)};
      unsigned char arr[16]{};
      std::copy(values.data(), values.data() + std::size(arr), arr);
#elif defined(GLZ_WIN)
      constexpr uint64_t valueSize{sizeof(char)};
      char arr[16]{};
      std::copy(values.data(), values.data() + std::size(arr), arr);
#endif
      Ret returnValue{
         create_array<Ret>(arr, std::make_index_sequence<sizeof(Ret) / valueSize>{})};
      return returnValue;
   }

   struct simd_t_holder
   {
      simd_t backslashes;
      simd_t whitespace;
      simd_t quotes;
      simd_t op;
   };

   constexpr std::array<char, 16> escapeableArray00{0x00u, 0x00u, 0x22u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                                    0x08u, 0x00u, 0x00u, 0x00u, 0x0Cu, 0x0Du, 0x00u, 0x00u};
   constexpr std::array<char, 16> escapeableArray01{0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                                    0x00u, 0x09u, 0x0Au, 0x00u, 0x5Cu, 0x00u, 0x00u, 0x00u};
   constexpr std::array<char, 16> whitespaceArray{0x20u, 0x64u, 0x64u, 0x64u, 0x11u, 0x64u, 0x71u, 0x02u,
                                                  0x64u, 0x09u, 0x0Au, 0x70u, 0x64u, 0x0Du, 0x64u, 0x64u};
   constexpr std::array<char, 16> opArray{0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                          0x00u, 0x00u, 0x3Au, 0x7Bu, 0x2Cu, 0x7Du, 0x00u, 0x00u};
   template <class T>
   constexpr T escapeableTable00{simdFromTable<T>(escapeableArray00)};
   template <class T>
   constexpr T escapeableTable01{simdFromTable<T>(escapeableArray01)};
   template <class T>
   constexpr T whitespaceTable{simdFromTable<T>(whitespaceArray)};
   template <class T>
   constexpr T backslashes{simdFromValue<T>(0x5Cu)};
   template <class T>
   constexpr T opTable{simdFromTable<T>(opArray)};
   template <class T>
   constexpr T quotes{simdFromValue<T>(0x22u)};
   template <class T>
   constexpr T chars{simdFromValue<T>(0x20u)};

   template <simd Arch>
   GLZ_ALWAYS_INLINE simd_t collectStructuralsAsSimdBase(const simd_t* values)
   {
      GLZ_ALIGN string_parsing_type v[strides_per_step];
      v[0] = opCmpEq(opShuffle(opTable<simd_t>, values[0]), opOr(chars<simd_t>, values[0]));
      v[1] = opCmpEq(opShuffle(opTable<simd_t>, values[1]), opOr(chars<simd_t>, values[1]));
      v[2] = opCmpEq(opShuffle(opTable<simd_t>, values[2]), opOr(chars<simd_t>, values[2]));
      v[3] = opCmpEq(opShuffle(opTable<simd_t>, values[3]), opOr(chars<simd_t>, values[3]));
      v[4] = opCmpEq(opShuffle(opTable<simd_t>, values[4]), opOr(chars<simd_t>, values[4]));
      v[5] = opCmpEq(opShuffle(opTable<simd_t>, values[5]), opOr(chars<simd_t>, values[5]));
      v[6] = opCmpEq(opShuffle(opTable<simd_t>, values[6]), opOr(chars<simd_t>, values[6]));
      v[7] = opCmpEq(opShuffle(opTable<simd_t>, values[7]), opOr(chars<simd_t>, values[7]));
      return vgather<simd_t>(v);
   }

   GLZ_ALWAYS_INLINE simd_t collectWhitespaceAsSimdBase(const simd_t* values)
   {
      GLZ_ALIGN string_parsing_type v[strides_per_step];
      v[0] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[0]), values[0]);
      v[1] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[1]), values[1]);
      v[2] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[2]), values[2]);
      v[3] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[3]), values[3]);
      v[4] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[4]), values[4]);
      v[5] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[5]), values[5]);
      v[6] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[6]), values[6]);
      v[7] = opCmpEq(opShuffle(whitespaceTable<simd_t>, values[7]), values[7]);
      return vgather<simd_t>(v);
   }

   GLZ_ALWAYS_INLINE simd_t collectBackslashesAsSimdBase(const simd_t* values)
   {
      GLZ_ALIGN string_parsing_type v[strides_per_step];
      v[0] = opCmpEq(backslashes<simd_t>, values[0]);
      v[1] = opCmpEq(backslashes<simd_t>, values[1]);
      v[2] = opCmpEq(backslashes<simd_t>, values[2]);
      v[3] = opCmpEq(backslashes<simd_t>, values[3]);
      v[4] = opCmpEq(backslashes<simd_t>, values[4]);
      v[5] = opCmpEq(backslashes<simd_t>, values[5]);
      v[6] = opCmpEq(backslashes<simd_t>, values[6]);
      v[7] = opCmpEq(backslashes<simd_t>, values[7]);
      return vgather<simd_t>(v);
   }

   GLZ_ALWAYS_INLINE simd_t collectQuotesAsSimdBase(const simd_t* values)
   {
      GLZ_ALIGN string_parsing_type v[strides_per_step];
      v[0] = opCmpEq(quotes<simd_t>, values[0]);
      v[1] = opCmpEq(quotes<simd_t>, values[1]);
      v[2] = opCmpEq(quotes<simd_t>, values[2]);
      v[3] = opCmpEq(quotes<simd_t>, values[3]);
      v[4] = opCmpEq(quotes<simd_t>, values[4]);
      v[5] = opCmpEq(quotes<simd_t>, values[5]);
      v[6] = opCmpEq(quotes<simd_t>, values[6]);
      v[7] = opCmpEq(quotes<simd_t>, values[7]);
      return vgather<simd_t>(v);
   }

   template <bool doWeCollectWhitespace>
   GLZ_ALWAYS_INLINE simd_t_holder collectIndices(const simd_t* values)
   {
      simd_t_holder returnValues;
      returnValues.op = collectStructuralsAsSimdBase(values);
      returnValues.quotes = collectQuotesAsSimdBase(values);
      if constexpr (doWeCollectWhitespace) {
         returnValues.whitespace = collectWhitespaceAsSimdBase(values);
      }
      returnValues.backslashes = collectBackslashesAsSimdBase(values);
      return returnValues;
   }
}
