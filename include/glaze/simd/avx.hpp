// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"
#include "glaze/util/bit.hpp"
#include "glaze/util/inline.hpp"
#include <cstring>

#if defined(GLZ_USE_AVX2)

namespace glz::detail
{
   template <class Data, class WriteEscape>
   GLZ_ALWAYS_INLINE void avx2_string_escape(const char*& c, const char* e, Data*& data, size_t n,
                                             WriteEscape&& write_escape)
   {
      // -----------------------------------------------------------------------
      // AVX2 SIMD PROCESSING
      // Only enter if string is long enough for at least one vector (32 bytes)
      // -----------------------------------------------------------------------
      if (n >= 32) {
         const __m256i quote_vec = _mm256_set1_epi8('"');
         const __m256i bs_vec = _mm256_set1_epi8('\\');
         const __m256i ctrl_mask = _mm256_set1_epi8(static_cast<int8_t>(0xE0));
         const __m256i zero = _mm256_setzero_si256();

         // Process 32 bytes at a time
         while (c <= e - 32) {
            __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(c));

            // Check for ", \, or control characters
            __m256i m = _mm256_or_si256(
               _mm256_or_si256(_mm256_cmpeq_epi8(v, quote_vec), _mm256_cmpeq_epi8(v, bs_vec)),
               _mm256_cmpeq_epi8(_mm256_and_si256(v, ctrl_mask), zero)
            );

            uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(m));

            // FAST PATH: No escaping needed
            if (mask == 0) {
               _mm256_storeu_si256(reinterpret_cast<__m256i*>(data), v);
               data += 32;
               c += 32;
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
            
            // Jump to scalar tail
            goto scalar_tail;
         }
      }

   scalar_tail:
      // -----------------------------------------------------------------------
      // SCALAR TAIL
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