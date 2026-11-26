// Glaze Library
// For the license information refer to glaze.hpp

// Bitmask view of a 64 byte window of JSON, for skipping over a value the program does not model.
//
// Skipping is the one place where Glaze reads bytes it will never hand to anyone, so the only
// question it has to answer is where the value ends. Answering that a byte at a time means a
// branch at every string boundary -- roughly one per ten bytes of real-world JSON. Answering it a
// window at a time turns the whole question into integer arithmetic: one bit per byte for quotes,
// backslashes, and the two bracket characters, then the standard prefix-xor derivation of which
// bytes sit inside a string, and a popcount that settles whether a window can close the value at
// all. Windows that cannot are skipped outright without looking at an individual byte.
//
// Only the mask construction is per-backend; everything built on top of it (escape carry, string
// interior, depth bookkeeping) is plain uint64_t arithmetic and lives in util/parse.hpp.
//
// A target with no byte compare and no movemask gets no masks and keeps the SWAR scan, which is
// always correct, just branchier.

#pragma once

#include <cstdint>
#include <string_view>

#include "glaze/simd/simd.hpp"
#include "glaze/util/inline.hpp"

#if defined(GLZ_USE_AVX512BW) || defined(GLZ_USE_AVX2) || defined(GLZ_USE_SSE2) || defined(GLZ_USE_NEON64) || \
   defined(GLZ_USE_WASM_SIMD128)
#define GLZ_STRUCTURAL_SIMD
#endif

#if defined(GLZ_STRUCTURAL_SIMD)

namespace glz::detail::structural
{
   // One bit per byte of a 64 byte window, lowest bit for the lowest address.
   struct window
   {
      uint64_t quote;
      uint64_t backslash;
      uint64_t open;
      uint64_t close;
      uint64_t non_ascii; // any byte with the high bit set, string or not
   };

#if defined(GLZ_USE_AVX512BW)

   inline constexpr std::string_view backend = "AVX512BW";

   GLZ_ALWAYS_INLINE window load_window(const char* p, const char open_c, const char close_c) noexcept
   {
      const __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(p));
      return {uint64_t(_mm512_cmpeq_epi8_mask(v, _mm512_set1_epi8('"'))),
              uint64_t(_mm512_cmpeq_epi8_mask(v, _mm512_set1_epi8('\\'))),
              uint64_t(_mm512_cmpeq_epi8_mask(v, _mm512_set1_epi8(open_c))),
              uint64_t(_mm512_cmpeq_epi8_mask(v, _mm512_set1_epi8(close_c))), uint64_t(_mm512_movepi8_mask(v))};
   }

#elif defined(GLZ_USE_AVX2)

   inline constexpr std::string_view backend = "AVX2";

   GLZ_ALWAYS_INLINE window load_window(const char* p, const char open_c, const char close_c) noexcept
   {
      const __m256i v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
      const __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p + 32));

      const auto mask_of = [&](const char c) {
         const __m256i cv = _mm256_set1_epi8(c);
         const uint64_t lo = uint32_t(_mm256_movemask_epi8(_mm256_cmpeq_epi8(v0, cv)));
         const uint64_t hi = uint32_t(_mm256_movemask_epi8(_mm256_cmpeq_epi8(v1, cv)));
         return lo | (hi << 32);
      };

      const uint64_t high_lo = uint32_t(_mm256_movemask_epi8(v0));
      const uint64_t high_hi = uint32_t(_mm256_movemask_epi8(v1));

      return {mask_of('"'), mask_of('\\'), mask_of(open_c), mask_of(close_c), high_lo | (high_hi << 32)};
   }

#elif defined(GLZ_USE_SSE2)

   inline constexpr std::string_view backend = "SSE2";

   GLZ_ALWAYS_INLINE window load_window(const char* p, const char open_c, const char close_c) noexcept
   {
      const __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
      const __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 16));
      const __m128i v2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 32));
      const __m128i v3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 48));

      const auto mask_of = [&](const char c) {
         const __m128i cv = _mm_set1_epi8(c);
         return uint64_t(uint16_t(_mm_movemask_epi8(_mm_cmpeq_epi8(v0, cv)))) |
                (uint64_t(uint16_t(_mm_movemask_epi8(_mm_cmpeq_epi8(v1, cv)))) << 16) |
                (uint64_t(uint16_t(_mm_movemask_epi8(_mm_cmpeq_epi8(v2, cv)))) << 32) |
                (uint64_t(uint16_t(_mm_movemask_epi8(_mm_cmpeq_epi8(v3, cv)))) << 48);
      };

      const uint64_t high =
         uint64_t(uint16_t(_mm_movemask_epi8(v0))) | (uint64_t(uint16_t(_mm_movemask_epi8(v1))) << 16) |
         (uint64_t(uint16_t(_mm_movemask_epi8(v2))) << 32) | (uint64_t(uint16_t(_mm_movemask_epi8(v3))) << 48);

      return {mask_of('"'), mask_of('\\'), mask_of(open_c), mask_of(close_c), high};
   }

#elif defined(GLZ_USE_NEON64)

   inline constexpr std::string_view backend = "NEON64";

   // NEON has no movemask. Weighting each lane by its bit position and folding with pairwise adds
   // gathers the 64 lane results into one register, four lanes per add, in four steps.
   GLZ_ALWAYS_INLINE uint64_t to_bitmask(const uint8x16_t a, const uint8x16_t b, const uint8x16_t c,
                                         const uint8x16_t d) noexcept
   {
      static constexpr uint8_t bits_array[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                                               0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
      const uint8x16_t bits = vld1q_u8(bits_array);
      uint8x16_t s0 = vpaddq_u8(vandq_u8(a, bits), vandq_u8(b, bits));
      const uint8x16_t s1 = vpaddq_u8(vandq_u8(c, bits), vandq_u8(d, bits));
      s0 = vpaddq_u8(s0, s1);
      s0 = vpaddq_u8(s0, s0);
      return vgetq_lane_u64(vreinterpretq_u64_u8(s0), 0);
   }

   GLZ_ALWAYS_INLINE window load_window(const char* p, const char open_c, const char close_c) noexcept
   {
      const auto* u = reinterpret_cast<const uint8_t*>(p);
      const uint8x16_t v0 = vld1q_u8(u);
      const uint8x16_t v1 = vld1q_u8(u + 16);
      const uint8x16_t v2 = vld1q_u8(u + 32);
      const uint8x16_t v3 = vld1q_u8(u + 48);

      const auto mask_of = [&](const char c) {
         const uint8x16_t cv = vdupq_n_u8(uint8_t(c));
         return to_bitmask(vceqq_u8(v0, cv), vceqq_u8(v1, cv), vceqq_u8(v2, cv), vceqq_u8(v3, cv));
      };

      // vcltq_s8 against zero is the sign test: a byte with the high bit set is a negative int8.
      const int8x16_t zero = vdupq_n_s8(0);
      const uint64_t high = to_bitmask(vcltq_s8(vreinterpretq_s8_u8(v0), zero), //
                                       vcltq_s8(vreinterpretq_s8_u8(v1), zero), //
                                       vcltq_s8(vreinterpretq_s8_u8(v2), zero), //
                                       vcltq_s8(vreinterpretq_s8_u8(v3), zero));

      return {mask_of('"'), mask_of('\\'), mask_of(open_c), mask_of(close_c), high};
   }

#elif defined(GLZ_USE_WASM_SIMD128)

   inline constexpr std::string_view backend = "WASM_SIMD128";

   GLZ_ALWAYS_INLINE window load_window(const char* p, const char open_c, const char close_c) noexcept
   {
      const v128_t v0 = wasm_v128_load(p);
      const v128_t v1 = wasm_v128_load(p + 16);
      const v128_t v2 = wasm_v128_load(p + 32);
      const v128_t v3 = wasm_v128_load(p + 48);

      const auto mask_of = [&](const char c) {
         const v128_t cv = wasm_i8x16_splat(c);
         return uint64_t(uint16_t(wasm_i8x16_bitmask(wasm_i8x16_eq(v0, cv)))) |
                (uint64_t(uint16_t(wasm_i8x16_bitmask(wasm_i8x16_eq(v1, cv)))) << 16) |
                (uint64_t(uint16_t(wasm_i8x16_bitmask(wasm_i8x16_eq(v2, cv)))) << 32) |
                (uint64_t(uint16_t(wasm_i8x16_bitmask(wasm_i8x16_eq(v3, cv)))) << 48);
      };

      const uint64_t high =
         uint64_t(uint16_t(wasm_i8x16_bitmask(v0))) | (uint64_t(uint16_t(wasm_i8x16_bitmask(v1))) << 16) |
         (uint64_t(uint16_t(wasm_i8x16_bitmask(v2))) << 32) | (uint64_t(uint16_t(wasm_i8x16_bitmask(v3))) << 48);

      return {mask_of('"'), mask_of('\\'), mask_of(open_c), mask_of(close_c), high};
   }

#endif
}

#endif
