// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"
#include "glaze/util/bit.hpp"
#include "glaze/util/inline.hpp"

#if defined(GLZ_USE_SSE2)

namespace glz::detail
{
   template <class Data, class WriteEscape>
   GLZ_ALWAYS_INLINE void sse2_string_escape(const char*& c, const char* e, Data*& data, size_t n,
                                              WriteEscape&& write_escape)
   {
      // SSE2: 16 bytes at a time with direct comparison instructions
      if (n > 15) {
         const __m128i quote_vec = _mm_set1_epi8('"');
         const __m128i bs_vec = _mm_set1_epi8('\\');
         const __m128i ctrl_mask = _mm_set1_epi8(static_cast<int8_t>(0xE0));
         const __m128i zero = _mm_setzero_si128();

         for (const char* end_m15 = e - 15; c < end_m15;) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v);

            const uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(
               _mm_or_si128(
                  _mm_or_si128(
                     _mm_cmpeq_epi8(v, quote_vec),
                     _mm_cmpeq_epi8(v, bs_vec)),
                  _mm_cmpeq_epi8(_mm_and_si128(v, ctrl_mask), zero))));

            if (mask == 0) {
               data += 16;
               c += 16;
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
