// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/core/context.hpp"
#include "glaze/util/inline.hpp"

namespace glz
{
   GLZ_ALWAYS_INLINE bool invalid_end(is_context auto& ctx, auto&& it, auto end) noexcept
   {
      if (it >= end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return true;
      }
      else [[likely]] {
         return false;
      }
   }

   // Bounds-checks a typed-array body of `count` elements of `element_size` bytes (element_size >= 1),
   // optionally preceded by `padding` alignment bytes, against the bytes remaining in [it, end).
   // Callers maintain it <= end (every advance is bounds-checked), so end - it is non-negative.
   // Returns true and sets unexpected_end when the body does not fit.
   //
   // On 64-bit, int_from_compressed caps wire counts at 2^48 and element_size <= 128, so
   // padding + count * element_size <= ~2^55 cannot overflow size_t: the fit is a single multiply
   // and compare against end - it (which, unlike it + offset, is never out-of-bounds pointer
   // arithmetic), so the 64-bit path pays nothing for the 32-bit safety. On 32-bit that product
   // overflows size_t -- wrapping small so an oversized array would slip past the check -- so there
   // the fit is tested as count > (remaining - padding) / element_size, a division that cannot
   // overflow.
   [[nodiscard]] GLZ_ALWAYS_INLINE bool typed_array_out_of_bounds(is_context auto& ctx, auto&& it, auto&& end,
                                                                  size_t count, size_t element_size,
                                                                  size_t padding = 0) noexcept
   {
      const uint64_t available = uint64_t(end - it);
      if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
         if (available < padding + count * element_size) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return true;
         }
      }
      else {
         if (available < padding || count > (available - padding) / element_size) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return true;
         }
      }
      return false;
   }

   // Byteswaps a numeric value in-place for little-endian wire format compatibility.
   // Call ONLY inside: if constexpr (std::endian::native == std::endian::big) blocks.
   // On little-endian systems, this function is never instantiated (zero overhead).
   template <class T>
      requires(std::integral<T> || std::floating_point<T> || std::is_enum_v<T>)
   GLZ_ALWAYS_INLINE void byteswap_le(T& value) noexcept
   {
      if constexpr (std::is_enum_v<T>) {
         using U = std::underlying_type_t<T>;
         if constexpr (sizeof(U) > 1) {
            U underlying_val;
            std::memcpy(&underlying_val, &value, sizeof(T));
            underlying_val = std::byteswap(underlying_val);
            std::memcpy(&value, &underlying_val, sizeof(T));
         }
      }
      else if constexpr (std::integral<T>) {
         if constexpr (sizeof(T) > 1) {
            value = std::byteswap(value);
         }
      }
      else if constexpr (std::floating_point<T>) {
         using Int = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
         Int int_val;
         std::memcpy(&int_val, &value, sizeof(T));
         int_val = std::byteswap(int_val);
         std::memcpy(&value, &int_val, sizeof(T));
      }
   }
}

namespace glz::tag
{
   inline constexpr uint8_t null = 0;
   inline constexpr uint8_t boolean = 0b00001'000;
   inline constexpr uint8_t number = 1;
   inline constexpr uint8_t string = 2;
   inline constexpr uint8_t object = 3;
   inline constexpr uint8_t typed_array = 4;
   inline constexpr uint8_t generic_array = 5;
   inline constexpr uint8_t extensions = 6;

   // Data delimiter for separating multiple BEVE values in a stream/buffer
   // Used like NDJSON's newline delimiter - when converted to JSON outputs '\n'
   inline constexpr uint8_t delimiter = 0b00000'110; // extensions type (6) with subtype 0

   inline constexpr uint8_t bool_false = 0b000'01'000;
   inline constexpr uint8_t bool_true = 0b000'11'000;

   inline constexpr uint8_t i8 = 0b000'01'001;
   inline constexpr uint8_t i16 = 0b001'01'001;
   inline constexpr uint8_t i32 = 0b010'01'001;
   inline constexpr uint8_t i64 = 0b011'01'001;
   inline constexpr uint8_t i128 = 0b100'01'001;

   inline constexpr uint8_t u8 = 0b000'10'001;
   inline constexpr uint8_t u16 = 0b001'10'001;
   inline constexpr uint8_t u32 = 0b010'10'001;
   inline constexpr uint8_t u64 = 0b011'10'001;
   inline constexpr uint8_t u128 = 0b100'10'001;

   inline constexpr uint8_t bf16 = 0b000'00'001; // brain float
   inline constexpr uint8_t f16 = 0b001'00'001;
   inline constexpr uint8_t f32 = 0b010'00'001;
   inline constexpr uint8_t f64 = 0b011'00'001;
   inline constexpr uint8_t f128 = 0b100'00'001;

   // Aligned typed array: category 3, sub-type 2 (bit 6 set, bits 5 and 7 clear)
   // Layout: ALIGNED_HEADER | NUMERIC_HEADER | SIZE | PADDING_LENGTH | PADDING | DATA
   inline constexpr uint8_t aligned_typed_array = 0b010'11'100; // 0x5C
}

namespace glz
{
   template <class T>
   constexpr uint8_t byte_count = uint8_t(std::bit_width(sizeof(T)) - 1);

   inline constexpr std::array<uint8_t, 8> byte_count_lookup{1, 2, 4, 8, 16, 32, 64, 128};

   // Types that BEVE encodes as elements of a numeric typed array.
   //
   // std::byte is serialized identically to a uint8_t (an unsigned 8-bit value): the
   // numeric formulas below produce byte_count == 0, an unsigned type, and a u8 typed-array
   // header for it, exactly matching uint8_t. It is the only byte_like type that is not
   // already num_t (uint8_t and unsigned char are integral), so the numeric typed-array
   // code paths must opt it in explicitly. This keeps an array of std::byte compact (a
   // typed u8 array) and consistent with how a scalar std::byte is encoded (a u8 number),
   // instead of an inflated generic array that stores a type header per element.
   template <class T>
   concept beve_num_t = num_t<T> || std::same_as<std::remove_cvref_t<T>, std::byte>;

   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr size_t int_from_compressed(auto&& ctx, auto&& it, auto end) noexcept
   {
      if (it >= end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return 0;
      }

      uint8_t header;
      std::memcpy(&header, it, 1);
      const uint8_t config = header & 0b000000'11;

      if ((it + byte_count_lookup[config]) > end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return 0;
      }

      switch (config) {
      case 0:
         ++it;
         return header >> 2;
      case 1: {
         uint16_t h;
         std::memcpy(&h, it, 2);
         if constexpr (std::endian::native == std::endian::big) {
            h = std::byteswap(h);
         }
         it += 2;
         return h >> 2;
      }
      case 2: {
         uint32_t h;
         std::memcpy(&h, it, 4);
         if constexpr (std::endian::native == std::endian::big) {
            h = std::byteswap(h);
         }
         it += 4;
         return h >> 2;
      }
      case 3: {
         // On 32-bit systems it's impossible to address more than 4 GBiB of memory, so we should verify first if we are
         // running in 64-bit mode here
         if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
            uint64_t h;
            std::memcpy(&h, it, 8);
            if constexpr (std::endian::native == std::endian::big) {
               h = std::byteswap(h);
            }
            it += 8;
            h = h >> 2;
            static constexpr uint64_t safety_limit = 1ull << 48; // 2^48
            if (h > safety_limit) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return 0;
            }
            return h;
         }
         else {
            // 8-byte length encoding not supported on 32-bit systems
            ctx.error = error_code::invalid_length;
            return 0;
         }
      }
      default:
         ctx.error = error_code::syntax_error;
         return 0;
      }
   }

   GLZ_ALWAYS_INLINE constexpr void skip_compressed_int(is_context auto&& ctx, auto&& it, auto end) noexcept
   {
      if (invalid_end(ctx, it, end)) {
         return;
      }

      uint8_t header;
      std::memcpy(&header, it, 1);
      const uint8_t config = header & 0b000000'11;

      if ((it + byte_count_lookup[config]) > end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }

      switch (config) {
      case 0:
         ++it;
         return;
      case 1: {
         it += 2;
         return;
      }
      case 2: {
         it += 4;
         return;
      }
      case 3: {
         it += 8;
         return;
      }
      default:
         return;
      }
   }
}
