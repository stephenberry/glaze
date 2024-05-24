// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "glaze/core/context.hpp"
#include "glaze/util/inline.hpp"

namespace glz::tag
{
   constexpr uint8_t null = 0;
   constexpr uint8_t boolean = 0b00001'000;
   constexpr uint8_t number = 1;
   constexpr uint8_t string = 2;
   constexpr uint8_t object = 3;
   constexpr uint8_t typed_array = 4;
   constexpr uint8_t generic_array = 5;
   constexpr uint8_t extensions = 6;

   constexpr uint8_t bool_false = 0b000'01'000;
   constexpr uint8_t bool_true = 0b000'11'000;

   constexpr uint8_t i8 = 0b000'01'001;
   constexpr uint8_t i16 = 0b001'01'001;
   constexpr uint8_t i32 = 0b010'01'001;
   constexpr uint8_t i64 = 0b011'01'001;
   constexpr uint8_t i128 = 0b100'01'001;

   constexpr uint8_t u8 = 0b000'10'001;
   constexpr uint8_t u16 = 0b001'10'001;
   constexpr uint8_t u32 = 0b010'10'001;
   constexpr uint8_t u64 = 0b011'10'001;
   constexpr uint8_t u128 = 0b100'10'001;

   constexpr uint8_t bf16 = 0b000'00'001; // brain float
   constexpr uint8_t f16 = 0b001'00'001;
   constexpr uint8_t f32 = 0b010'00'001;
   constexpr uint8_t f64 = 0b011'00'001;
   constexpr uint8_t f128 = 0b100'00'001;
}

namespace glz::detail
{
   template <class T>
   constexpr uint8_t byte_count = uint8_t(std::bit_width(sizeof(T)) - 1);

   constexpr std::array<uint8_t, 8> byte_count_lookup{1, 2, 4, 8, 16, 32, 64, 128};

   [[nodiscard]] GLZ_ALWAYS_INLINE constexpr size_t int_from_compressed(is_context auto&& ctx, auto&& it,
                                                                        auto&& end) noexcept
   {
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
         it += 2;
         return h >> 2;
      }
      case 2: {
         uint32_t h;
         std::memcpy(&h, it, 4);
         it += 4;
         return h >> 2;
      }
      case 3: {
         uint64_t h;
         std::memcpy(&h, it, 8);
         it += 8;
         return h >> 2;
      }
      default:
         return 0;
      }
   }

   GLZ_ALWAYS_INLINE constexpr void skip_compressed_int(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
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
