// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"
#include "glaze/util/bit.hpp"
#include "glaze/util/inline.hpp"

#if defined(GLZ_USE_AVX2)

namespace glz::detail
{
   template <class Data, class WriteEscape>
   GLZ_ALWAYS_INLINE void avx2_string_escape(const char*& c, const char* e, Data*& data, size_t n,
                                              WriteEscape&& write_escape)
   {
      // AVX2: 32 bytes at a time with direct comparison instructions
      if (n > 31) {
         const __m256i quote_vec = _mm256_set1_epi8('"');
         const __m256i bs_vec = _mm256_set1_epi8('\\');
         // Control char detection: (v & 0xE0) == 0 iff v is 0x00-0x1F
         const __m256i ctrl_mask = _mm256_set1_epi8(static_cast<int8_t>(0xE0));
         const __m256i zero = _mm256_setzero_si256();

         for (const char* end_m31 = e - 31; c < end_m31;) {
            __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(c));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(data), v);

            const uint32_t mask = _mm256_movemask_epi8(
               _mm256_or_si256(
                  _mm256_or_si256(
                     _mm256_cmpeq_epi8(v, quote_vec),
                     _mm256_cmpeq_epi8(v, bs_vec)),
                  _mm256_cmpeq_epi8(_mm256_and_si256(v, ctrl_mask), zero)));

            if (mask == 0) {
               data += 32;
               c += 32;
               continue;
            }

            const uint32_t length = countr_zero(mask);
            c += length;
            data += length;
            write_escape();
         }
      }
   }
}

#endif
