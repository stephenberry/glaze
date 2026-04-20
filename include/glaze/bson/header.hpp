// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/util/inline.hpp"

// BSON - https://bsonspec.org/spec.html
//
// A BSON document is the top-level container and is encoded as:
//     int32 length | e_list | 0x00
// The int32 length includes itself and the terminating null byte. All
// multi-byte integers and floats on the wire are little-endian.
//
// Each element in e_list is:
//     byte type | cstring key | value...
// where cstring is a UTF-8 byte sequence terminated by 0x00.
//
// BSON arrays are encoded as documents whose keys are the decimal string
// representations of their indices ("0", "1", "2", ...), in order.

namespace glz::bson
{
   // Element type codes. These identify the wire format of a document value.
   namespace type
   {
      inline constexpr uint8_t double_ = 0x01; // IEEE 754 binary64
      inline constexpr uint8_t string = 0x02; // int32 length (bytes+1) | UTF-8 | 0x00
      inline constexpr uint8_t document = 0x03; // embedded BSON document
      inline constexpr uint8_t array = 0x04; // document with integer-string keys
      inline constexpr uint8_t binary = 0x05; // int32 length | subtype | bytes
      inline constexpr uint8_t undefined = 0x06; // DEPRECATED (read-only support)
      inline constexpr uint8_t object_id = 0x07; // 12 bytes
      inline constexpr uint8_t boolean = 0x08; // 0x00 = false, 0x01 = true
      inline constexpr uint8_t datetime = 0x09; // int64 ms since Unix epoch, UTC
      inline constexpr uint8_t null = 0x0A; // no value bytes
      inline constexpr uint8_t regex = 0x0B; // cstring pattern | cstring options
      inline constexpr uint8_t db_pointer = 0x0C; // DEPRECATED (read-only support)
      inline constexpr uint8_t javascript = 0x0D; // string (length-prefixed)
      inline constexpr uint8_t symbol = 0x0E; // DEPRECATED (read-only support)
      inline constexpr uint8_t code_w_scope = 0x0F; // DEPRECATED (read-only support)
      inline constexpr uint8_t int32 = 0x10;
      inline constexpr uint8_t timestamp = 0x11; // uint64: low 32 increment, high 32 seconds
      inline constexpr uint8_t int64 = 0x12;
      inline constexpr uint8_t decimal128 = 0x13; // IEEE 754-2008 decimal128 (opaque)
      inline constexpr uint8_t min_key = 0xFF; // compares lower than all other values
      inline constexpr uint8_t max_key = 0x7F; // compares higher than all other values
   }

   // Binary data subtype codes, carried in the byte immediately after the
   // length of a type::binary element.
   namespace binary_subtype
   {
      inline constexpr uint8_t generic = 0x00;
      inline constexpr uint8_t function = 0x01;
      inline constexpr uint8_t binary_old = 0x02; // DEPRECATED
      inline constexpr uint8_t uuid_old = 0x03; // DEPRECATED
      inline constexpr uint8_t uuid = 0x04; // RFC 9562 UUID, canonical byte order
      inline constexpr uint8_t md5 = 0x05;
      inline constexpr uint8_t encrypted = 0x06;
      inline constexpr uint8_t column = 0x07; // compressed BSON column
      inline constexpr uint8_t sensitive = 0x08;
      inline constexpr uint8_t vector = 0x09;
      inline constexpr uint8_t user_defined_min = 0x80; // 0x80..0xFF reserved for users
   }

   // Opaque 12-byte MongoDB ObjectId. MongoDB assigns meaning to the bytes
   // (timestamp, random, counter) but the BSON wire format treats it as a
   // fixed-size blob. Glaze does the same — generation is left to the user.
   struct object_id
   {
      std::array<uint8_t, 12> bytes{};

      [[nodiscard]] constexpr bool operator==(const object_id&) const noexcept = default;
      [[nodiscard]] constexpr auto operator<=>(const object_id&) const noexcept = default;
   };

   // Wire representation of a BSON UTC datetime: signed milliseconds since
   // the Unix epoch. Users usually prefer std::chrono::system_clock::time_point,
   // which the BSON codec auto-maps to this type.
   struct datetime
   {
      int64_t ms_since_epoch{};

      [[nodiscard]] constexpr bool operator==(const datetime&) const noexcept = default;
      [[nodiscard]] constexpr auto operator<=>(const datetime&) const noexcept = default;
   };

   // MongoDB-internal timestamp used for replication and sharding. Distinct
   // from `datetime` — on the wire it is a uint64 whose low 32 bits hold an
   // increment counter and whose high 32 bits hold seconds since epoch.
   struct timestamp
   {
      uint32_t increment{};
      uint32_t seconds{};

      [[nodiscard]] constexpr bool operator==(const timestamp&) const noexcept = default;
      [[nodiscard]] constexpr auto operator<=>(const timestamp&) const noexcept = default;
   };

   // PCRE-style regular expression. Options are ASCII flag characters in
   // alphabetical order per spec: 'i' case-insensitive, 'l' locale-dependent,
   // 'm' multiline, 's' dotall, 'u' Unicode, 'x' verbose.
   struct regex
   {
      std::string pattern;
      std::string options;

      [[nodiscard]] bool operator==(const regex&) const noexcept = default;
   };

   struct javascript
   {
      std::string code;

      [[nodiscard]] bool operator==(const javascript&) const noexcept = default;
   };

   // Opaque 16-byte IEEE 754-2008 decimal128. This is not the same encoding
   // as std::float128_t (binary128); the formats share width and parent spec
   // but not bit layout. Arithmetic and string conversion are out of scope
   // for now — this type carries the raw bytes for round-trip interop.
   struct decimal128
   {
      std::array<uint8_t, 16> bytes{};

      [[nodiscard]] constexpr bool operator==(const decimal128&) const noexcept = default;
   };

   // Comparison sentinels used primarily in MongoDB queries. They have no
   // associated value and compare lower/higher than every other BSON value.
   struct min_key
   {
      [[nodiscard]] constexpr bool operator==(const min_key&) const noexcept = default;
   };

   struct max_key
   {
      [[nodiscard]] constexpr bool operator==(const max_key&) const noexcept = default;
   };

   // Binary payload carrying a subtype byte. The container type is left to
   // the user; common choices are std::vector<std::byte>, std::vector<uint8_t>,
   // and std::string. The subtype defaults to `generic` (0x00).
   //
   // Constrained to resizable containers because the reader calls `.resize()`
   // with the wire-provided length. Fixed-size targets (e.g., std::array) would
   // write but not read, so the constraint is enforced at the type level to
   // keep write and read symmetric.
   template <class Bytes = std::vector<std::byte>>
      requires(resizable<Bytes>)
   struct binary
   {
      Bytes data{};
      uint8_t subtype = binary_subtype::generic;

      [[nodiscard]] bool operator==(const binary&) const noexcept = default;
   };
}
