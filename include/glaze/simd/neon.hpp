// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/simd/simd.hpp"
#include "glaze/util/inline.hpp"

#if defined(GLZ_USE_NEON)

namespace glz::detail
{
   GLZ_ALWAYS_INLINE uint8_t neon_hmax_u8(const uint8x16_t v) noexcept
   {
#if defined(__aarch64__) || defined(_M_ARM64)
      return vmaxvq_u8(v);
#else
      uint8x8_t max8 = vmax_u8(vget_low_u8(v), vget_high_u8(v));
      max8 = vpmax_u8(max8, max8);
      max8 = vpmax_u8(max8, max8);
      max8 = vpmax_u8(max8, max8);
      return vget_lane_u8(max8, 0);
#endif
   }

   namespace original
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
               return vorrq_u8(vorrq_u8(vceqq_u8(v, quote_vec), vceqq_u8(v, bs_vec)), vcltq_u8(v, ctrl_threshold));
            };

            for (const char* end_m63 = e - 63; c < end_m63;) {
               const uint8x16_t v0 = vld1q_u8(reinterpret_cast<const uint8_t*>(c));
               const uint8x16_t v1 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 16));
               const uint8x16_t v2 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 32));
               const uint8x16_t v3 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 48));

               vst1q_u8(reinterpret_cast<uint8_t*>(data), v0);
               vst1q_u8(reinterpret_cast<uint8_t*>(data + 16), v1);
               vst1q_u8(reinterpret_cast<uint8_t*>(data + 32), v2);
               vst1q_u8(reinterpret_cast<uint8_t*>(data + 48), v3);

               const uint8x16_t any = vorrq_u8(vorrq_u8(check(v0), check(v1)), vorrq_u8(check(v2), check(v3)));

               if (neon_hmax_u8(any) == 0) {
                  data += 64;
                  c += 64;
                  continue;
               }
               break;
            }

            for (const char* end_m15 = e - 15; c < end_m15;) {
               const uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(c));
               vst1q_u8(reinterpret_cast<uint8_t*>(data), v);

               if (neon_hmax_u8(check(v)) == 0) {
                  data += 16;
                  c += 16;
                  continue;
               }

               while (uint8_t(*c) >= 0x20 && uint8_t(*c) != '"' && uint8_t(*c) != '\\') {
                  ++data;
                  ++c;
               }
               write_escape();
            }
         }
      }
   } // namespace original

   namespace optimized
   {
      GLZ_ALWAYS_INLINE uint16_t neon_movemask_u8(const uint8x16_t input) noexcept
      {
         const uint8x16_t bitmask_elements = {
            0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
            0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
         };
         uint8x16_t masked = vandq_u8(input, bitmask_elements);
         uint8x16_t temp = vpaddq_u8(masked, masked);
         temp = vpaddq_u8(temp, temp);
         temp = vpaddq_u8(temp, temp);
         uint64_t low = vgetq_lane_u64(vreinterpretq_u64_u8(temp), 0);
         uint64_t high = vgetq_lane_u64(vreinterpretq_u64_u8(temp), 1);
         return static_cast<uint16_t>((low & 0xFF) | ((high & 0xFF) << 8));
      }

      template <class Data, class WriteEscape>
      GLZ_ALWAYS_INLINE void neon_string_escape(const char*& c, const char* e, Data*& data, size_t n,
                                                WriteEscape&& write_escape)
      {
         if (n > 15) {
            const uint8x16_t quote_vec = vdupq_n_u8('"');
            const uint8x16_t bs_vec = vdupq_n_u8('\\');
            const uint8x16_t ctrl_threshold = vdupq_n_u8(0x20);

            auto check = [&](uint8x16_t v) {
               return vorrq_u8(vorrq_u8(vceqq_u8(v, quote_vec), vceqq_u8(v, bs_vec)), vcltq_u8(v, ctrl_threshold));
            };

            for (const char* end_m63 = e - 63; c < end_m63;) {
               const uint8x16_t v0 = vld1q_u8(reinterpret_cast<const uint8_t*>(c));
               const uint8x16_t v1 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 16));
               const uint8x16_t v2 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 32));
               const uint8x16_t v3 = vld1q_u8(reinterpret_cast<const uint8_t*>(c + 48));

               vst1q_u8(reinterpret_cast<uint8_t*>(data), v0);
               vst1q_u8(reinterpret_cast<uint8_t*>(data + 16), v1);
               vst1q_u8(reinterpret_cast<uint8_t*>(data + 32), v2);
               vst1q_u8(reinterpret_cast<uint8_t*>(data + 48), v3);

               const uint8x16_t any = vorrq_u8(vorrq_u8(check(v0), check(v1)), vorrq_u8(check(v2), check(v3)));

               if (neon_hmax_u8(any) == 0) {
                  data += 64;
                  c += 64;
                  continue;
               }
               break;
            }

            for (const char* end_m15 = e - 15; c < end_m15;) {
               const uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(c));
               vst1q_u8(reinterpret_cast<uint8_t*>(data), v);

               const uint16_t mask = neon_movemask_u8(check(v));
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
   } // namespace optimized

   template <class Data, class WriteEscape>
   GLZ_ALWAYS_INLINE void neon_string_escape(const char*& c, const char* e, Data*& data, size_t n,
                                             WriteEscape&& write_escape)
   {
      optimized::neon_string_escape(c, e, data, n, std::forward<WriteEscape>(write_escape));
   }
} // namespace glz::detail

#endif
