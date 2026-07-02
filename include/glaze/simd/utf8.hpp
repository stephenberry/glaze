// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// SIMD UTF-8 validation using the range/lookup algorithm popularized by Daniel Lemire and
// implemented in simdjson ("Validating UTF-8 In Less Than One Instruction Per Byte"):
// https://arxiv.org/abs/2010.03090. One algorithm, three instruction sets (NEON, AVX2, SSSE3)
// plus a scalar fallback in parse.hpp. glz::validate_utf8 dispatches here for large buffers.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "glaze/simd/simd.hpp"
#include "glaze/util/inline.hpp"

// The NEON kernel uses AArch64-only intrinsics (vqtbl1q_u8, vmaxvq_u8); 32-bit ARM (AArch32/armv7)
// defines __ARM_NEON but lacks them, so it falls back to the scalar validator. The 128-bit x86 path
// needs SSSE3 (pshufb/palignr), which the x86_64 baseline (SSE2) lacks.
#if (defined(GLZ_USE_NEON) && (defined(__aarch64__) || defined(_M_ARM64))) || defined(GLZ_USE_AVX2) || \
   (defined(GLZ_USE_SSE2) && defined(__SSSE3__))
#define GLZ_HAS_SIMD_UTF8 1
#endif

namespace glz
{
#if defined(GLZ_HAS_SIMD_UTF8)
   namespace detail
   {
      // Nibble lookup tables shared by every instruction set. Three error classes are detected
      // by ANDing a lookup on the lead byte's high nibble, the lead byte's low nibble, and the
      // following byte's high nibble; a non-zero result marks malformed UTF-8.
      //
      //   bit0 TOO_SHORT  bit1 TOO_LONG  bit2 OVERLONG_3  bit3 TOO_LARGE
      //   bit4 SURROGATE  bit5 OVERLONG_2  bit6 TOO_LARGE_1000/OVERLONG_4  bit7 TWO_CONTS
      inline constexpr uint8_t utf8_b1_high[16] = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
                                                   0x80, 0x80, 0x80, 0x80, 0x21, 0x01, 0x15, 0x49};
      inline constexpr uint8_t utf8_b1_low[16] = {0xE7, 0xA3, 0x83, 0x83, 0x8B, 0xCB, 0xCB, 0xCB,
                                                  0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xDB, 0xCB, 0xCB};
      inline constexpr uint8_t utf8_b2_high[16] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                                   0xE6, 0xAE, 0xBA, 0xBA, 0x01, 0x01, 0x01, 0x01};
      // Largest value a byte may take in each of the last three lanes of a block without starting
      // an unfinished multibyte sequence (a 4-byte lead 3 from the end, etc.).
      inline constexpr uint8_t utf8_incomplete_max[16] = {255, 255, 255, 255, 255, 255, 255, 255,
                                                          255, 255, 255, 255, 255, 0xEF, 0xDF, 0xBF};

#if defined(GLZ_USE_NEON)
      GLZ_ALWAYS_INLINE uint8x16_t utf8_special(uint8x16_t input, uint8x16_t prev1) noexcept
      {
         const uint8x16_t h1 = vqtbl1q_u8(vld1q_u8(utf8_b1_high), vshrq_n_u8(prev1, 4));
         const uint8x16_t l1 = vqtbl1q_u8(vld1q_u8(utf8_b1_low), vandq_u8(prev1, vdupq_n_u8(0x0F)));
         const uint8x16_t h2 = vqtbl1q_u8(vld1q_u8(utf8_b2_high), vshrq_n_u8(input, 4));
         return vandq_u8(vandq_u8(h1, l1), h2);
      }

      GLZ_ALWAYS_INLINE uint8x16_t utf8_multibyte_lengths(uint8x16_t input, uint8x16_t prev_input,
                                                          uint8x16_t sc) noexcept
      {
         const uint8x16_t p2 = vextq_u8(prev_input, input, 14);
         const uint8x16_t p3 = vextq_u8(prev_input, input, 13);
         const uint8x16_t is3 = vqsubq_u8(p2, vdupq_n_u8(0xE0 - 1));
         const uint8x16_t is4 = vqsubq_u8(p3, vdupq_n_u8(0xF0 - 1));
         const uint8x16_t must = vcgtq_s8(vreinterpretq_s8_u8(vorrq_u8(is3, is4)), vdupq_n_s8(0));
         return veorq_u8(vandq_u8(must, vdupq_n_u8(0x80)), sc);
      }

      GLZ_ALWAYS_INLINE void utf8_block(uint8x16_t input, uint8x16_t prev_input, uint8x16_t& error) noexcept
      {
         const uint8x16_t prev1 = vextq_u8(prev_input, input, 15);
         error = vorrq_u8(error, utf8_multibyte_lengths(input, prev_input, utf8_special(input, prev1)));
      }

      inline bool validate_utf8_simd(const uint8_t* in, size_t len) noexcept
      {
         uint8x16_t error = vdupq_n_u8(0), prev = vdupq_n_u8(0), pinc = vdupq_n_u8(0);
         size_t pos = 0;
         const auto run = [&](uint8x16_t a, uint8x16_t b, uint8x16_t c, uint8x16_t d) {
            if (vmaxvq_u8(vorrq_u8(vorrq_u8(a, b), vorrq_u8(c, d))) < 0x80) {
               error = vorrq_u8(error, pinc);
               pinc = vdupq_n_u8(0);
            }
            else {
               utf8_block(a, prev, error);
               utf8_block(b, a, error);
               utf8_block(c, b, error);
               utf8_block(d, c, error);
               pinc = vqsubq_u8(d, vld1q_u8(utf8_incomplete_max));
            }
            prev = d;
         };
         for (; len - pos >= 64; pos += 64) {
            run(vld1q_u8(in + pos), vld1q_u8(in + pos + 16), vld1q_u8(in + pos + 32), vld1q_u8(in + pos + 48));
         }
         if (pos < len) {
            uint8_t tmp[64] = {0};
            std::memcpy(tmp, in + pos, len - pos);
            run(vld1q_u8(tmp), vld1q_u8(tmp + 16), vld1q_u8(tmp + 32), vld1q_u8(tmp + 48));
         }
         return vmaxvq_u8(vorrq_u8(error, pinc)) == 0;
      }

#elif defined(GLZ_USE_AVX2)
      GLZ_ALWAYS_INLINE __m256i utf8_tbl(const uint8_t* t) noexcept
      {
         return _mm256_broadcastsi128_si256(_mm_loadu_si128(reinterpret_cast<const __m128i*>(t)));
      }

      GLZ_ALWAYS_INLINE __m256i utf8_special(__m256i input, __m256i prev1) noexcept
      {
         const __m256i m = _mm256_set1_epi8(0x0F);
         const __m256i h1 = _mm256_shuffle_epi8(utf8_tbl(utf8_b1_high), _mm256_and_si256(_mm256_srli_epi16(prev1, 4), m));
         const __m256i l1 = _mm256_shuffle_epi8(utf8_tbl(utf8_b1_low), _mm256_and_si256(prev1, m));
         const __m256i h2 = _mm256_shuffle_epi8(utf8_tbl(utf8_b2_high), _mm256_and_si256(_mm256_srli_epi16(input, 4), m));
         return _mm256_and_si256(_mm256_and_si256(h1, l1), h2);
      }

      GLZ_ALWAYS_INLINE __m256i utf8_multibyte_lengths(__m256i input, __m256i prev_input, __m256i sc) noexcept
      {
         const __m256i sh = _mm256_permute2x128_si256(prev_input, input, 0x21);
         const __m256i p2 = _mm256_alignr_epi8(input, sh, 14);
         const __m256i p3 = _mm256_alignr_epi8(input, sh, 13);
         const __m256i is3 = _mm256_subs_epu8(p2, _mm256_set1_epi8(char(0xE0 - 1)));
         const __m256i is4 = _mm256_subs_epu8(p3, _mm256_set1_epi8(char(0xF0 - 1)));
         const __m256i must = _mm256_cmpgt_epi8(_mm256_or_si256(is3, is4), _mm256_setzero_si256());
         return _mm256_xor_si256(_mm256_and_si256(must, _mm256_set1_epi8(char(0x80))), sc);
      }

      GLZ_ALWAYS_INLINE void utf8_block(__m256i input, __m256i prev_input, __m256i& error) noexcept
      {
         const __m256i sh = _mm256_permute2x128_si256(prev_input, input, 0x21);
         const __m256i prev1 = _mm256_alignr_epi8(input, sh, 15);
         error = _mm256_or_si256(error, utf8_multibyte_lengths(input, prev_input, utf8_special(input, prev1)));
      }

      inline bool validate_utf8_simd(const uint8_t* in, size_t len) noexcept
      {
         alignas(32) static constexpr uint8_t maxv[32] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                          255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                          255, 255, 255, 255, 255, 255, 255, 0xEF, 0xDF, 0xBF};
         __m256i error = _mm256_setzero_si256(), prev = _mm256_setzero_si256(), pinc = _mm256_setzero_si256();
         size_t pos = 0;
         const auto run = [&](__m256i a, __m256i b) {
            if (_mm256_movemask_epi8(_mm256_or_si256(a, b)) == 0) {
               error = _mm256_or_si256(error, pinc);
               pinc = _mm256_setzero_si256();
            }
            else {
               utf8_block(a, prev, error);
               utf8_block(b, a, error);
               pinc = _mm256_subs_epu8(b, _mm256_load_si256(reinterpret_cast<const __m256i*>(maxv)));
            }
            prev = b;
         };
         for (; len - pos >= 64; pos += 64) {
            run(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + pos)),
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + pos + 32)));
         }
         if (pos < len) {
            uint8_t tmp[64] = {0};
            std::memcpy(tmp, in + pos, len - pos);
            run(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(tmp)),
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(tmp + 32)));
         }
         error = _mm256_or_si256(error, pinc);
         return _mm256_movemask_epi8(_mm256_cmpeq_epi8(error, _mm256_setzero_si256())) == -1;
      }

#else // SSSE3
      GLZ_ALWAYS_INLINE __m128i utf8_special(__m128i input, __m128i prev1) noexcept
      {
         const __m128i m = _mm_set1_epi8(0x0F);
         const __m128i h1 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(utf8_b1_high)),
                                             _mm_and_si128(_mm_srli_epi16(prev1, 4), m));
         const __m128i l1 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(utf8_b1_low)),
                                             _mm_and_si128(prev1, m));
         const __m128i h2 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(utf8_b2_high)),
                                             _mm_and_si128(_mm_srli_epi16(input, 4), m));
         return _mm_and_si128(_mm_and_si128(h1, l1), h2);
      }

      GLZ_ALWAYS_INLINE __m128i utf8_multibyte_lengths(__m128i input, __m128i prev_input, __m128i sc) noexcept
      {
         const __m128i p2 = _mm_alignr_epi8(input, prev_input, 14);
         const __m128i p3 = _mm_alignr_epi8(input, prev_input, 13);
         const __m128i is3 = _mm_subs_epu8(p2, _mm_set1_epi8(char(0xE0 - 1)));
         const __m128i is4 = _mm_subs_epu8(p3, _mm_set1_epi8(char(0xF0 - 1)));
         const __m128i must = _mm_cmpgt_epi8(_mm_or_si128(is3, is4), _mm_setzero_si128());
         return _mm_xor_si128(_mm_and_si128(must, _mm_set1_epi8(char(0x80))), sc);
      }

      GLZ_ALWAYS_INLINE void utf8_block(__m128i input, __m128i prev_input, __m128i& error) noexcept
      {
         const __m128i prev1 = _mm_alignr_epi8(input, prev_input, 15);
         error = _mm_or_si128(error, utf8_multibyte_lengths(input, prev_input, utf8_special(input, prev1)));
      }

      inline bool validate_utf8_simd(const uint8_t* in, size_t len) noexcept
      {
         __m128i error = _mm_setzero_si128(), prev = _mm_setzero_si128(), pinc = _mm_setzero_si128();
         size_t pos = 0;
         const auto run = [&](__m128i a, __m128i b, __m128i c, __m128i d) {
            if (_mm_movemask_epi8(_mm_or_si128(_mm_or_si128(a, b), _mm_or_si128(c, d))) == 0) {
               error = _mm_or_si128(error, pinc);
               pinc = _mm_setzero_si128();
            }
            else {
               utf8_block(a, prev, error);
               utf8_block(b, a, error);
               utf8_block(c, b, error);
               utf8_block(d, c, error);
               pinc = _mm_subs_epu8(d, _mm_loadu_si128(reinterpret_cast<const __m128i*>(utf8_incomplete_max)));
            }
            prev = d;
         };
         for (; len - pos >= 64; pos += 64) {
            run(_mm_loadu_si128(reinterpret_cast<const __m128i*>(in + pos)),
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + pos + 16)),
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + pos + 32)),
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + pos + 48)));
         }
         if (pos < len) {
            uint8_t tmp[64] = {0};
            std::memcpy(tmp, in + pos, len - pos);
            run(_mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp)),
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp + 16)),
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp + 32)),
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp + 48)));
         }
         error = _mm_or_si128(error, pinc);
         return _mm_movemask_epi8(_mm_cmpeq_epi8(error, _mm_setzero_si128())) == 0xFFFF;
      }
#endif
   }
#endif
}
