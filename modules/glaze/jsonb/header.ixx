// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.jsonb.header;

import std;

import glaze.core.context;

#include "glaze/util/inline.hpp"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::size_t;

// SQLite JSONB binary format - https://sqlite.org/jsonb.html
//
// Every element begins with a 1-9 byte header:
//   low 4 bits = element type (0..12; 13..15 reserved)
//   high 4 bits = payload size encoding:
//     0..11 -> payload size is exactly that value, header is 1 byte
//     12    -> next 1 byte is uint8  BE payload size, header is 2 bytes
//     13    -> next 2 bytes are uint16 BE payload size, header is 3 bytes
//     14    -> next 4 bytes are uint32 BE payload size, header is 5 bytes
//     15    -> next 8 bytes are uint64 BE payload size, header is 9 bytes
// The header is NOT required to be in its minimal form.

export namespace glz::jsonb
{
   namespace type
   {
      inline constexpr uint8_t null_ = 0;
      inline constexpr uint8_t true_ = 1;
      inline constexpr uint8_t false_ = 2;
      inline constexpr uint8_t int_ = 3; // ASCII decimal, RFC 8259 integer
      inline constexpr uint8_t int5 = 4; // ASCII JSON5 integer (hex, leading +, etc.)
      inline constexpr uint8_t float_ = 5; // ASCII decimal float, RFC 8259
      inline constexpr uint8_t float5 = 6; // ASCII JSON5 float (NaN, Infinity, .5, etc.)
      inline constexpr uint8_t text = 7; // UTF-8, no escapes needed
      inline constexpr uint8_t textj = 8; // UTF-8, contains RFC 8259 escapes (\n, \uXXXX, ...)
      inline constexpr uint8_t text5 = 9; // UTF-8, contains JSON5 escapes (\xNN, \', ...)
      inline constexpr uint8_t textraw = 10; // Raw UTF-8 bytes, may need escaping to emit JSON
      inline constexpr uint8_t array = 11;
      inline constexpr uint8_t object = 12;
      // 13, 14, 15 reserved
   }

   namespace size_code
   {
      inline constexpr uint8_t u8_follows = 12;
      inline constexpr uint8_t u16_follows = 13;
      inline constexpr uint8_t u32_follows = 14;
      inline constexpr uint8_t u64_follows = 15;
   }

   // Maximum header length in bytes (9 = 1 header byte + 8 for uint64 size)
   inline constexpr size_t max_header_bytes = 9;

   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr uint8_t get_type(uint8_t initial) noexcept { return initial & 0x0f; }
   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr uint8_t get_size_nibble(uint8_t initial) noexcept
   {
      return (initial >> 4) & 0x0f;
   }
   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr uint8_t make_initial(uint8_t type_code, uint8_t size_nibble) noexcept
   {
      return static_cast<uint8_t>((size_nibble << 4) | (type_code & 0x0f));
   }

   // Compute the number of header bytes required to encode a payload of the given size.
   // Returns 1, 2, 3, 5, or 9.
   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr size_t header_bytes_for_payload(uint64_t payload_size) noexcept
   {
      if (payload_size <= 11) return 1;
      if (payload_size <= 0xFF) return 2;
      if (payload_size <= 0xFFFF) return 3;
      if (payload_size <= 0xFFFFFFFFull) return 5;
      return 9;
   }

   // Write a header for a scalar (non-container) element whose payload size is known.
   // Always writes in minimal form. Returns the number of bytes written (1..9).
   template <class B, class IX>
   GLZ_ALWAYS_INLINE size_t write_header(uint8_t type_code, uint64_t payload_size, B& b, IX& ix) noexcept
   {
      auto put_byte = [&](uint8_t byte) {
         using V = typename std::decay_t<B>::value_type;
         b[ix] = static_cast<V>(byte);
         ++ix;
      };
      auto put_be = [&]<class T>(T v) {
         if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1) {
            v = std::byteswap(v);
         }
         std::memcpy(&b[ix], &v, sizeof(T));
         ix += sizeof(T);
      };

      if (payload_size <= 11) {
         put_byte(make_initial(type_code, static_cast<uint8_t>(payload_size)));
         return 1;
      }
      if (payload_size <= 0xFF) {
         put_byte(make_initial(type_code, size_code::u8_follows));
         put_byte(static_cast<uint8_t>(payload_size));
         return 2;
      }
      if (payload_size <= 0xFFFF) {
         put_byte(make_initial(type_code, size_code::u16_follows));
         put_be(static_cast<uint16_t>(payload_size));
         return 3;
      }
      if (payload_size <= 0xFFFFFFFFull) {
         put_byte(make_initial(type_code, size_code::u32_follows));
         put_be(static_cast<uint32_t>(payload_size));
         return 5;
      }
      put_byte(make_initial(type_code, size_code::u64_follows));
      put_be(payload_size);
      return 9;
   }

   // Patch a 9-byte reserved header slot at buffer position `header_pos` with the actual
   // payload size. Uses size_code::u64_follows so the reserved slot is always exactly 9 bytes.
   // Callers that want a minimal header must shift the payload themselves.
   template <class B>
   GLZ_ALWAYS_INLINE void patch_header_9(B& b, size_t header_pos, uint8_t type_code, uint64_t payload_size) noexcept
   {
      using V = typename std::decay_t<B>::value_type;
      b[header_pos] = static_cast<V>(make_initial(type_code, size_code::u64_follows));
      uint64_t v = payload_size;
      if constexpr (std::endian::native == std::endian::little) {
         v = std::byteswap(v);
      }
      std::memcpy(&b[header_pos + 1], &v, 8);
   }

   // Read a header starting at `it`, returning the decoded type code and payload size.
   // Advances `it` past the header on success.
   template <class It>
   GLZ_ALWAYS_INLINE bool read_header(is_context auto& ctx, It& it, It end, uint8_t& type_code,
                                      uint64_t& payload_size) noexcept
   {
      if (it >= end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return false;
      }

      uint8_t initial;
      std::memcpy(&initial, it, 1);
      ++it;

      type_code = get_type(initial);
      const uint8_t sz = get_size_nibble(initial);

      if (sz <= 11) {
         payload_size = sz;
         return true;
      }

      switch (sz) {
      case size_code::u8_follows: {
         if ((end - it) < 1) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         uint8_t v;
         std::memcpy(&v, it, 1);
         it += 1;
         payload_size = v;
         return true;
      }
      case size_code::u16_follows: {
         if ((end - it) < 2) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         uint16_t v;
         std::memcpy(&v, it, 2);
         if constexpr (std::endian::native == std::endian::little) {
            v = std::byteswap(v);
         }
         it += 2;
         payload_size = v;
         return true;
      }
      case size_code::u32_follows: {
         if ((end - it) < 4) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         uint32_t v;
         std::memcpy(&v, it, 4);
         if constexpr (std::endian::native == std::endian::little) {
            v = std::byteswap(v);
         }
         it += 4;
         payload_size = v;
         return true;
      }
      case size_code::u64_follows: {
         if ((end - it) < 8) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         uint64_t v;
         std::memcpy(&v, it, 8);
         if constexpr (std::endian::native == std::endian::little) {
            v = std::byteswap(v);
         }
         it += 8;
         payload_size = v;
         return true;
      }
      }
      ctx.error = error_code::syntax_error;
      return false;
   }
}
