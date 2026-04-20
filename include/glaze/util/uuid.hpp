// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "glaze/util/inline.hpp"

namespace glz
{
   // A 16-byte universally unique identifier (RFC 9562 / RFC 4122).
   //
   // Byte order is the canonical RFC 9562 big-endian layout used by the
   // 36-character textual form (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx) and
   // by BSON binary subtype 0x04. bytes[0] is the most significant byte of
   // the first hex group; bytes[15] is the last byte of the final group.
   struct uuid
   {
      std::array<uint8_t, 16> bytes{};

      [[nodiscard]] constexpr bool operator==(const uuid&) const noexcept = default;
      [[nodiscard]] constexpr auto operator<=>(const uuid&) const noexcept = default;

      [[nodiscard]] constexpr bool is_nil() const noexcept
      {
         for (auto b : bytes) {
            if (b != 0) return false;
         }
         return true;
      }
   };

   namespace detail
   {
      [[nodiscard]] constexpr int uuid_hex_nibble(char c) noexcept
      {
         if (c >= '0' && c <= '9') return c - '0';
         if (c >= 'a' && c <= 'f') return c - 'a' + 10;
         if (c >= 'A' && c <= 'F') return c - 'A' + 10;
         return -1;
      }

      // Offsets into a 36-character canonical UUID string for each of the 16
      // bytes. The 4-2-2-2-6 byte groups are separated by hyphens at positions
      // 8, 13, 18, 23.
      inline constexpr std::array<uint8_t, 16> uuid_byte_offsets{
         0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34};
   }

   // Write the 36-character canonical form of `u` into `out`. The caller must
   // provide at least 36 writable bytes. No null terminator is appended.
   GLZ_ALWAYS_INLINE constexpr void format_uuid(const uuid& u, char* out) noexcept
   {
      constexpr char hex[] = "0123456789abcdef";
      size_t byte_idx = 0;
      size_t out_idx = 0;
      constexpr int group_sizes[5] = {4, 2, 2, 2, 6};
      for (int g = 0; g < 5; ++g) {
         if (g > 0) {
            out[out_idx++] = '-';
         }
         for (int i = 0; i < group_sizes[g]; ++i) {
            const uint8_t b = u.bytes[byte_idx++];
            out[out_idx++] = hex[(b >> 4) & 0xF];
            out[out_idx++] = hex[b & 0xF];
         }
      }
   }

   [[nodiscard]] inline std::string to_string(const uuid& u)
   {
      std::string s(36, '\0');
      format_uuid(u, s.data());
      return s;
   }

   // Parse a 36-character canonical hyphenated UUID string. Accepts upper- or
   // lower-case hex digits. Returns true on success and writes the result to
   // `out`; returns false on malformed input, leaving `out` unmodified.
   [[nodiscard]] constexpr bool parse_uuid(std::string_view s, uuid& out) noexcept
   {
      if (s.size() != 36) return false;
      if (s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-') return false;

      std::array<uint8_t, 16> result{};
      for (size_t i = 0; i < 16; ++i) {
         const size_t pos = detail::uuid_byte_offsets[i];
         const int hi = detail::uuid_hex_nibble(s[pos]);
         const int lo = detail::uuid_hex_nibble(s[pos + 1]);
         if (hi < 0 || lo < 0) return false;
         result[i] = static_cast<uint8_t>((hi << 4) | lo);
      }
      out.bytes = result;
      return true;
   }
}

template <>
struct std::hash<glz::uuid>
{
   [[nodiscard]] size_t operator()(const glz::uuid& u) const noexcept
   {
      // FNV-1a over the 16 bytes. Deterministic and allocation-free.
      size_t h = 14695981039346656037ULL;
      for (auto b : u.bytes) {
         h ^= b;
         h *= 1099511628211ULL;
      }
      return h;
   }
};
