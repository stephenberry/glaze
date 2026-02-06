// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"
#include "glaze/util/bit.hpp"
#include "glaze/util/inline.hpp"
#include <cstring>

#if defined(GLZ_USE_SSE2)

namespace glz::detail
{
   template <class Data, class WriteEscape>
   GLZ_ALWAYS_INLINE void sse2_string_escape(const char*& c, const char* e, Data*& data, size_t n,
                                             WriteEscape&& write_escape)
   {
      // -----------------------------------------------------------------------
      // SSE2 SIMD PROCESSING
      // Only enter if string is long enough for at least one vector (16 bytes)
      // -----------------------------------------------------------------------
      if (n >= 16) {
         const __m128i quote_vec = _mm_set1_epi8('"');
         const __m128i bs_vec = _mm_set1_epi8('\\');
         // Control char mask: values 0x00-0x1F
         const __m128i ctrl_mask = _mm_set1_epi8(static_cast<int8_t>(0xE0));
         const __m128i zero = _mm_setzero_si128();

         // Process 16 bytes at a time
         while (c <= e - 16) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c));

            // Check for ", \, or control characters
            __m128i m = _mm_or_si128(
               _mm_or_si128(_mm_cmpeq_epi8(v, quote_vec), _mm_cmpeq_epi8(v, bs_vec)),
               _mm_cmpeq_epi8(_mm_and_si128(v, ctrl_mask), zero)
            );

            uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(m));

            // FAST PATH: No escaping needed
            if (mask == 0) {
               _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v);
               data += 16;
               c += 16;
               continue;
            }

            // SLOW PATH: Escape found
            uint32_t len = countr_zero(mask);
            
            if (len > 0) {
               std::memcpy(data, c, len);
               data += len;
               c += len;
            }

            write_escape();
            ++c; // Skip the escaped character
            
            // Jump to scalar tail to handle the rest safely
            // This is valid: jumping OUT of the scope destroys SIMD vars
            goto scalar_tail;
         }
      }

   scalar_tail:
      // -----------------------------------------------------------------------
      // SCALAR TAIL
      // Process remaining bytes (0-15) and handle complex sequences
      // -----------------------------------------------------------------------
      while (c < e) {
         const uint8_t val = static_cast<uint8_t>(*c);
         
         if (val < 0x20 || val == '"' || val == '\\') [[unlikely]] {
            write_escape();
            ++c;
            continue;
         }
         
         *data++ = static_cast<Data>(*c++);
      }
   }
} // namespace glz::detail

#endif