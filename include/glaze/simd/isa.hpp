// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Instruction Set Architecture

#include <array>
#include <cstring>

#include "glaze/simd/simd.hpp"
#include "glaze/simd/avx.hpp"
#include "glaze/simd/avx2.hpp"
#include "glaze/simd/avx512.hpp"
#include "glaze/simd/compare.hpp"
#include "glaze/simd/gather.hpp"
#include "glaze/simd/shuffle.hpp"

namespace glz
{
   using avx_list = type_list<
      type_holder<16, simd128_t, uint16_t, (std::numeric_limits<uint16_t>::max)()>,
      type_holder<32, simd256_t, uint32_t, (std::numeric_limits<uint32_t>::max)()>,
      type_holder<64, simd512_t, uint64_t, (std::numeric_limits<uint64_t>::max)()>>;

   using avx_integer_list =
   type_list<type_holder<8, uint64_t, uint64_t, 8>,
                                     type_holder<16, simd128_t, uint16_t, 16>,
                                     type_holder<32, simd256_t, uint32_t, 32>,
                                     type_holder<64, simd512_t, uint64_t, 64>>;

   GLZ_ALWAYS_INLINE uint64_t prefixXor(uint64_t prevInString) noexcept
   {
      prevInString ^= prevInString << 1;
      prevInString ^= prevInString << 2;
      prevInString ^= prevInString << 4;
      prevInString ^= prevInString << 8;
      prevInString ^= prevInString << 16;
      prevInString ^= prevInString << 32;
      return prevInString;
   }

   template <class T0>
   GLZ_ALWAYS_INLINE simd_t opClMul(T0&& value, int64_t& prevInString) noexcept
   {
      GLZ_ALIGN uint64_t vs[bits_per_step64];
      store(value, vs);
      vs[0] = prefixXor(vs[0]) ^ prevInString;
      prevInString = static_cast<int64_t>(vs[0]) >> 63;
      vs[1] = prefixXor(vs[1]) ^ prevInString;
      prevInString = static_cast<int64_t>(vs[1]) >> 63;
      if constexpr (bits_per_step64 > 2) {
         vs[2] = prefixXor(vs[2]) ^ prevInString;
         prevInString = static_cast<int64_t>(vs[2]) >> 63;
         vs[3] = prefixXor(vs[3]) ^ prevInString;
         prevInString = static_cast<int64_t>(vs[3]) >> 63;
      }
      if constexpr (bits_per_step64 > 4) {
         vs[4] = prefixXor(vs[4]) ^ prevInString;
         prevInString = static_cast<int64_t>(vs[4]) >> 63;
         vs[5] = prefixXor(vs[5]) ^ prevInString;
         prevInString = static_cast<int64_t>(vs[5]) >> 63;
         vs[6] = prefixXor(vs[6]) ^ prevInString;
         prevInString = static_cast<int64_t>(vs[6]) >> 63;
         vs[7] = prefixXor(vs[7]) ^ prevInString;
         prevInString = static_cast<int64_t>(vs[7]) >> 63;
      }
      return vgather<simd_t>(vs);
   }

   template <class T0>
   GLZ_ALWAYS_INLINE simd_t opSub(T0&& value, T0&& other) noexcept
   {
      GLZ_ALIGN uint64_t vs[bits_per_step64 * 2];
      store(value, vs);
      store(other, vs + bits_per_step64);
      bool carryInNew{};
      vs[bits_per_step64] = vs[0] - vs[bits_per_step64] - uint64_t(carryInNew);
      carryInNew = vs[bits_per_step64] > vs[0];
      vs[1 + bits_per_step64] = vs[1] - vs[1 + bits_per_step64] - uint64_t(carryInNew);
      carryInNew = vs[1 + bits_per_step64] > vs[1];
      if constexpr (bits_per_step64 > 2) {
         vs[2 + bits_per_step64] = vs[2] - vs[2 + bits_per_step64] - uint64_t(carryInNew);
         carryInNew = vs[2 + bits_per_step64] > vs[2];
         vs[3 + bits_per_step64] = vs[3] - vs[3 + bits_per_step64] - uint64_t(carryInNew);
         carryInNew = vs[3 + bits_per_step64] > vs[3];
      }
      if constexpr (bits_per_step64 > 4) {
         vs[4 + bits_per_step64] = vs[4] - vs[4 + bits_per_step64] - uint64_t(carryInNew);
         carryInNew = vs[4 + bits_per_step64] > vs[4];
         vs[5 + bits_per_step64] = vs[5] - vs[5 + bits_per_step64] - uint64_t(carryInNew);
         carryInNew = vs[5 + bits_per_step64] > vs[5];
         vs[6 + bits_per_step64] = vs[6] - vs[6 + bits_per_step64] - uint64_t(carryInNew);
         carryInNew = vs[6 + bits_per_step64] > vs[6];
         vs[7 + bits_per_step64] = vs[7] - vs[7 + bits_per_step64] - uint64_t(carryInNew);
         carryInNew = vs[7 + bits_per_step64] > vs[7];
      }
      return vgather<simd_t>(vs + bits_per_step64);
   }

   template <uint64_t amount, class T0>
   GLZ_ALWAYS_INLINE simd_t opShl(T0&& value) noexcept
   {
      GLZ_ALIGN uint64_t vs[bits_per_step64 * 2];
      store(value, vs);
      static constexpr uint64_t shift{64 - amount};
      vs[bits_per_step64] = vs[0] << amount;
      vs[1 + bits_per_step64] = vs[1] << amount | vs[1 - 1] >> (shift);
      if constexpr (bits_per_step64 > 2) {
         vs[2 + bits_per_step64] = vs[2] << amount | vs[2 - 1] >> (shift);
         vs[3 + bits_per_step64] = vs[3] << amount | vs[3 - 1] >> (shift);
      }
      if constexpr (bits_per_step64 > 4) {
         vs[4 + bits_per_step64] = vs[4] << amount | vs[4 - 1] >> (shift);
         vs[5 + bits_per_step64] = vs[5] << amount | vs[5 - 1] >> (shift);
         vs[6 + bits_per_step64] = vs[6] << amount | vs[6 - 1] >> (shift);
         vs[7 + bits_per_step64] = vs[7] << amount | vs[7 - 1] >> (shift);
      }
      return vgather<simd_t>(vs + bits_per_step64);
   }

   template <class T0>
   GLZ_ALWAYS_INLINE simd_t opFollows(T0&& value, bool& overflow) noexcept
   {
      bool oldOverflow = overflow;
      overflow = opGetMSB(value);
      return opSetLSB(opShl<1>(value), oldOverflow);
   }
}
