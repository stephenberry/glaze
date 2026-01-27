// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"
#include "glaze/util/inline.hpp"

#if defined(GLZ_USE_NEON)

namespace glz::detail
{
   template <class Data, class WriteEscape>
   GLZ_ALWAYS_INLINE void neon_string_escape(const char*& c, const char* e, Data*& data, size_t n,
                                              WriteEscape&& write_escape)
   {
      if (n > 15) {
         const uint8x16_t quote_vec = vdupq_n_u8('"');
         const uint8x16_t bs_vec = vdupq_n_u8('\\');
         const uint8x16_t ctrl_threshold = vdupq_n_u8(0x20);

         auto check = [&](uint8x16_t v) {
            return vorrq_u8(
               vorrq_u8(
                  vceqq_u8(v, quote_vec),
                  vceqq_u8(v, bs_vec)),
               vcltq_u8(v, ctrl_threshold));
         };

         // Wide path: 64 bytes (4×16) per iteration to reduce loop overhead.
         // When an escape is found anywhere in the 64-byte region, fall through
         // to the 16-byte loop which locates the exact position.
         for (const char* end_m63 = e - 63; c < end_m63;) {
            const uint8x16_t v0 = vld1q_u8(reinterpret_cast<const uint8_t*>(c));
            const uint8x16_t v1 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 16));
            const uint8x16_t v2 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 32));
            const uint8x16_t v3 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 48));

            vst1q_u8(reinterpret_cast<uint8_t*>(data), v0);
            vst1q_u8(reinterpret_cast<uint8_t*>(data + 16), v1);
            vst1q_u8(reinterpret_cast<uint8_t*>(data + 32), v2);
            vst1q_u8(reinterpret_cast<uint8_t*>(data + 48), v3);

            const uint8x16_t any = vorrq_u8(
               vorrq_u8(check(v0), check(v1)),
               vorrq_u8(check(v2), check(v3)));

            if (vmaxvq_u8(any) == 0) {
               data += 64;
               c += 64;
               continue;
            }

            // Escape found somewhere in the 64-byte region.
            // Fall through to the 16-byte loop to locate and handle it.
            break;
         }

         // 16-byte loop: handles the tail and any escape found in the wide path.
         for (const char* end_m15 = e - 15; c < end_m15;) {
            const uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(c));
            vst1q_u8(reinterpret_cast<uint8_t*>(data), v); // speculative store

            if (vmaxvq_u8(check(v)) == 0) {
               data += 16;
               c += 16;
               continue;
            }

            // Scalar scan to find first escapable byte — guaranteed to find one
            // within the 16-byte chunk that vmaxvq_u8 flagged.
            // The speculative store already wrote clean bytes ahead of this position.
            while (uint8_t(*c) >= 0x20 && uint8_t(*c) != '"' && uint8_t(*c) != '\\') {
               ++data;
               ++c;
            }
            write_escape();
         }
      }
   }
}

#endif
