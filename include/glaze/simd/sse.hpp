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
   if (n < 16) {
   scalar_tail:
      while (c < e) {
         const uint8_t val = static_cast<uint8_t>(*c);
         if (val < 0x20 || val == '"' || val == '\\') [[unlikely]] {
            write_escape();
            continue;
         }
         *data++ = static_cast<Data>(*c++); // FIX: static_cast dla std::byte
      }
      return;
   }

   const __m128i quote_vec = _mm_set1_epi8('"');
   const __m128i bs_vec = _mm_set1_epi8('\\');
   const __m128i ctrl_mask = _mm_set1_epi8(static_cast<int8_t>(0xE0));
   const __m128i zero = _mm_setzero_si128();

   if (n >= 64) {
      const char* end_m63 = e - 63;
      while (c < end_m63) {
         _mm_prefetch(c + 128, _MM_HINT_T0);
         
         __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c));
         __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c + 16));
         __m128i v2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c + 32));
         __m128i v3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c + 48));
         
         auto get_mask = [&](__m128i v) -> uint32_t {
            __m128i m = _mm_or_si128(_mm_or_si128(_mm_cmpeq_epi8(v, quote_vec), _mm_cmpeq_epi8(v, bs_vec)),
                                     _mm_cmpeq_epi8(_mm_and_si128(v, ctrl_mask), zero));
            return static_cast<uint32_t>(_mm_movemask_epi8(m));
         };

         uint32_t m0 = get_mask(v0);
         uint32_t m1 = get_mask(v1);
         uint32_t m2 = get_mask(v2);
         uint32_t m3 = get_mask(v3);

         if ((m0 | m1 | m2 | m3) == 0) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data + 16), v1);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data + 32), v2);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data + 48), v3);
            c += 64; data += 64;
            continue;
         }

         // FIX: Zawsze wykonuj storeu przed sprawdzeniem maski, aby zachować ciągłość danych
         _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v0);
         if (m0) { uint32_t len = countr_zero(m0); c += len; data += len; write_escape(); break; }
         c += 16; data += 16;
         
         _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v1);
         if (m1) { uint32_t len = countr_zero(m1); c += len; data += len; write_escape(); break; }
         c += 16; data += 16;
         
         _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v2);
         if (m2) { uint32_t len = countr_zero(m2); c += len; data += len; write_escape(); break; }
         c += 16; data += 16;
         
         _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v3); // FIX: Brakujący store dla v3
         uint32_t len = countr_zero(m3); c += len; data += len; write_escape(); break;
      }
   }

   while (c <= e - 16) {
      __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(c));
      _mm_storeu_si128(reinterpret_cast<__m128i*>(data), v);
      uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(_mm_or_si128(_mm_or_si128(_mm_cmpeq_epi8(v, quote_vec), _mm_cmpeq_epi8(v, bs_vec)),
                                     _mm_cmpeq_epi8(_mm_and_si128(v, ctrl_mask), zero))));
      if (mask == 0) { data += 16; c += 16; continue; }
      uint32_t len = countr_zero(mask); c += len; data += len; write_escape();
   }

   goto scalar_tail;
}
}
#endif