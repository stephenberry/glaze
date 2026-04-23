// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include "glaze/core/buffer_traits.hpp"
#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"

namespace glz
{
   inline constexpr std::string_view base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

   inline std::string read_base64(const std::string_view input)
   {
      std::string decoded_data;
      static constexpr std::array<int, 256> decode_table = [] {
         std::array<int, 256> t;
         t.fill(-1);
         for (int i = 0; i < 64; ++i) {
            t[base64_chars[i]] = i;
         }
         return t;
      }();

      int val = 0, valb = -8;
      for (unsigned char c : input) {
         if (decode_table[c] == -1) break; // Stop decoding at padding '=' or invalid characters
         val = (val << 6) + decode_table[c];
         valb += 6;
         if (valb >= 0) {
            decoded_data.push_back((val >> valb) & 0xFF);
            valb -= 8;
         }
      }

      return decoded_data;
   }

   // 12-bit → two-character table. 4096 × 2 B = 8 KB, fits in L1D.
   // Halves the per-triple lookup count from 4 to 2 vs. the obvious
   // byte-at-a-time version.
   inline constexpr auto base64_pair_table = [] {
      std::array<char[2], 4096> t{};
      for (size_t i = 0; i < 4096; ++i) {
         t[i][0] = base64_chars[(i >> 6) & 0x3F];
         t[i][1] = base64_chars[i & 0x3F];
      }
      return t;
   }();

   // RFC 4648 base64, no line breaks. `=` padding only.
   // Writes directly into a caller-supplied output buffer at offset `ix`.
   // The buffer's value_type must be byte-sized.
   template <class B>
   inline void write_base64_to(is_context auto& ctx, const uint8_t* bytes, size_t n, B& out, size_t& ix) noexcept
   {
      using V = typename std::decay_t<B>::value_type;
      static_assert(sizeof(V) == 1, "write_base64_to requires a byte-sized output buffer");
      const size_t full_triples = n / 3;
      const size_t rem = n % 3;
      const size_t out_len = 4 * (full_triples + (rem ? 1 : 0));
      if (!ensure_space(ctx, out, ix + out_len + write_padding_bytes)) return;

      if (full_triples) {
         char* dst = reinterpret_cast<char*>(&out[ix]);
         for (size_t i = 0; i < full_triples; ++i) {
            const uint8_t b0 = bytes[3 * i];
            const uint8_t b1 = bytes[3 * i + 1];
            const uint8_t b2 = bytes[3 * i + 2];
            std::memcpy(dst, base64_pair_table[(uint32_t(b0) << 4) | (b1 >> 4)], 2);
            std::memcpy(dst + 2, base64_pair_table[(uint32_t(b1 & 0x0F) << 8) | b2], 2);
            dst += 4;
         }
         ix += 4 * full_triples;
      }

      if (rem == 1) {
         const uint8_t b0 = bytes[3 * full_triples];
         out[ix++] = static_cast<V>(base64_chars[b0 >> 2]);
         out[ix++] = static_cast<V>(base64_chars[(b0 & 0x03) << 4]);
         out[ix++] = static_cast<V>('=');
         out[ix++] = static_cast<V>('=');
      }
      else if (rem == 2) {
         const uint8_t b0 = bytes[3 * full_triples];
         const uint8_t b1 = bytes[3 * full_triples + 1];
         std::memcpy(&out[ix], base64_pair_table[(uint32_t(b0) << 4) | (b1 >> 4)], 2);
         ix += 2;
         out[ix++] = static_cast<V>(base64_chars[(b1 & 0x0F) << 2]);
         out[ix++] = static_cast<V>('=');
      }
   }

   inline std::string write_base64(const std::string_view input)
   {
      std::string out;
      size_t ix{};
      context ctx{};
      write_base64_to(ctx, reinterpret_cast<const uint8_t*>(input.data()), input.size(), out, ix);
      out.resize(ix);
      return out;
   }
}
