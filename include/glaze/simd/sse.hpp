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
      if (n < 16) return; // let SWAR → scalar handle it

      const __m128i quote_vec = _mm_set1_epi8('"');
      const __m128i bs_vec   = _mm_set1_epi8('\\');
      const __m128i ctrl_mask = _mm_set1_epi8(static_cast<int8_t>(0xE0));
      const __m128i zero     = _mm_setzero_si128();

      while (c <= e - 16) {
         const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c));

         // Speculative store
         _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v);

         const __m128i is_ctrl  = _mm_cmpeq_epi8(_mm_and_si128(v, ctrl_mask), zero);
         const __m128i is_quote = _mm_cmpeq_epi8(v, quote_vec);
         const __m128i is_bs    = _mm_cmpeq_epi8(v, bs_vec);
         const __m128i m = _mm_or_si128(_mm_or_si128(is_quote, is_bs), is_ctrl);

         const uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(m));

         if (mask == 0) {
            data += 16;
            c += 16;
         }
         else {
            const uint32_t len = countr_zero(mask);
            data += len;
            c += len;
            write_escape();
         }
      }
      // Remaining < 16 bytes fall through to SWAR → scalar
   }
}

#endif
