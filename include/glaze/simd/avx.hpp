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
   if (n > 31) {
      const __m256i quote_vec = _mm256_set1_epi8('"');
      const __m256i bs_vec = _mm256_set1_epi8('\\');
      const __m256i ctrl_mask = _mm256_set1_epi8(static_cast<int8_t>(0xE0));
      const __m256i zero = _mm256_setzero_si256();

      auto compute_mask = [&](__m256i v) -> __m256i {
         return _mm256_or_si256(
            _mm256_or_si256(
               _mm256_cmpeq_epi8(v, quote_vec),
               _mm256_cmpeq_epi8(v, bs_vec)
            ),
            _mm256_cmpeq_epi8(_mm256_and_si256(v, ctrl_mask), zero)
         );
      };

      if (n > 63) {
         const char* end_m63 = e - 63;
         while (c < end_m63) {
            __m256i v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(c));
            __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(c + 32));

            __m256i m0 = compute_mask(v0);
            __m256i m1 = compute_mask(v1);
            __m256i any = _mm256_or_si256(m0, m1);

            if (_mm256_testz_si256(any, any)) {
               _mm256_storeu_si256(reinterpret_cast<__m256i*>(data), v0);
               _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + 32), v1);
               c += 64;
               data += 64;
               continue;
            }

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(data), v0);
            uint32_t mask0 = static_cast<uint32_t>(_mm256_movemask_epi8(m0));
            if (mask0 != 0) {
               uint32_t len = countr_zero(mask0);
               c += len;
               data += len;
               write_escape();
               goto avx_tail_start; // Skok do pętli 32-bajtowej po ucieczce
            }
            c += 32;
            data += 32;

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(data), v1);
            uint32_t mask1 = static_cast<uint32_t>(_mm256_movemask_epi8(m1));
            uint32_t len = countr_zero(mask1);
            c += len;
            data += len;
            write_escape();
            goto avx_tail_start;
         }
      }

   avx_tail_start:
      for (const char* end_m31 = e - 31; c < end_m31;) {
         __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(c));
         _mm256_storeu_si256(reinterpret_cast<__m256i*>(data), v);
         uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(compute_mask(v)));

         if (mask == 0) {
            data += 32;
            c += 32;
            continue;
         }

         uint32_t len = countr_zero(mask);
         c += len;
         data += len;
         write_escape();
      }
   }
}
} // namespace glz::detail
#endif