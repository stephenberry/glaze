// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/core/format.hpp"
#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/dump.hpp"

namespace glz::detail
{
   template <opts Opts>
   inline void skip_value_binary(is_context auto&&, auto&&, auto&&) noexcept;

   template <opts Opts>
   inline void skip_string_binary(is_context auto&&, auto&& it, auto&& end) noexcept
   {
      const auto tag = uint8_t(*it);
      const uint8_t byte_count = (tag & 0b000'11'000) >> 3;
      ++it;

      const auto n = int_from_compressed(it, end);
      std::advance(it, byte_count * n);
   }

   template <opts Opts>
   inline void skip_number_binary(is_context auto&&, auto&& it, auto&& end) noexcept
   {
      const auto tag = uint8_t(*it);
      const uint8_t byte_count = tag >> 5;
      ++it;

      const auto n = int_from_compressed(it, end);
      std::advance(it, byte_count * n);
   }

   template <opts Opts>
   inline void skip_object_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      ++it;

      const auto n_keys = int_from_compressed(it, end);

      for (size_t i = 0; i < n_keys; ++i) {
         if ((uint8_t(*it) & 0b00000'111) == tag::string) {
            skip_string_binary(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            skip_value_binary<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }
         else if ((uint8_t(*it) & 0b00000'111) == tag::number) {
            skip_number_binary(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            skip_value_binary<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }
      }
   }

   template <opts Opts>
   inline void skip_typed_array_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      const auto tag = uint8_t(*it);
      const uint8_t type = (tag & 0b000'11'000) >> 3;
      switch (type) {
      case 0:
      case 1:
      case 2: {
         const uint8_t byte_count = tag >> 5;
         ++it;
         const auto n = int_from_compressed(it, end);
         std::advance(it, byte_count * n);
         break;
      }
      case 3: {
         const bool is_bool = (tag & 0b00'1'00'000) >> 5;
         if (is_bool) {
            ++it;
            const auto n = int_from_compressed(it, end);
            const auto num_bytes = (n + 7) / 8;
            std::advance(it, num_bytes);
         }
         else {
            const uint8_t byte_count = tag >> 6;
            ++it;
            const auto n = int_from_compressed(it, end);
            std::advance(it, byte_count * n);
         }
         break;
      }
      default:
         ctx.error = error_code::syntax_error;
      }
   }

   template <opts Opts>
   inline void skip_untyped_array_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      ++it;
      const auto n = int_from_compressed(it, end);
      for (size_t i = 0; i < n; ++i) {
         skip_value_binary(ctx, it, end);
      }
   }

   template <opts Opts>
   inline void skip_additional_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      ++it;
      const auto n = int_from_compressed(it, end);
      skip_value_binary(ctx, it, end);
   }

   template <opts Opts>
   inline void skip_value_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      switch (uint8_t(*it) & 0b00000'111) {
      case tag::null: {
         ++it;
         break;
      }
      case tag::boolean: {
         ++it;
         break;
      };
      case tag::number: {
         skip_number_binary(ctx, it, end);
         break;
      }
      case tag::string: {
         skip_string_binary(ctx, it, end);
         break;
      }
      case tag::object: {
         skip_object_binary(ctx, it, end);
         break;
      }
      case tag::typed_array: {
         skip_typed_array_binary(ctx, it, end);
         break;
      }
      case tag::generic_array: {
         unskip_typed_array_binary(ctx, it, end);
         break;
      }
      case tag::extensions: {
         skip_additional_binary(ctx, it, end);
         break;
      }
      default:
         ctx.error = error_code::syntax_error;
      }
   }
}
