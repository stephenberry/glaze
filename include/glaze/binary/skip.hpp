// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/dump.hpp"

namespace glz::detail
{
   template <opts Opts>
   inline void skip_value_binary(is_context auto&&, auto&&, auto&&) noexcept;

   inline void skip_string_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      ++it;
      const auto n = int_from_compressed(ctx, it, end);
      if (bool(ctx.error)) [[unlikely]] {
         return;
      }
      it += n;
   }

   inline void skip_number_binary(is_context auto&&, auto&& it, auto&&) noexcept
   {
      const auto tag = uint8_t(*it);
      const uint8_t byte_count = byte_count_lookup[tag >> 5];
      ++it;
      it += byte_count;
   }

   template <opts Opts>
   inline void skip_object_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      const auto tag = uint8_t(*it);
      ++it;

      const auto n_keys = int_from_compressed(ctx, it, end);

      if ((tag & 0b00000'111) == tag::string) {
         for (size_t i = 0; i < n_keys; ++i) {
            const auto string_length = int_from_compressed(ctx, it, end);
            it += string_length;
            if (bool(ctx.error)) [[unlikely]]
               return;

            skip_value_binary<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }
      }
      else if ((tag & 0b00000'111) == tag::number) {
         const uint8_t byte_count = byte_count_lookup[tag >> 5];
         for (size_t i = 0; i < n_keys; ++i) {
            const auto n = int_from_compressed(ctx, it, end);
            it += byte_count * n;
            if (bool(ctx.error)) [[unlikely]]
               return;

            skip_value_binary<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }
      }
      else {
         ctx.error = error_code::syntax_error;
         return;
      }
   }

   template <opts Opts>
   inline void skip_typed_array_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      const auto tag = uint8_t(*it);
      const uint8_t type = (tag & 0b000'11'000) >> 3;
      switch (type) {
      case 0: // floating point (fallthrough)
      case 1: // signed integer (fallthrough)
      case 2: { // unsigned integer
         ++it;
         const auto n = int_from_compressed(ctx, it, end);
         const uint8_t byte_count = byte_count_lookup[tag >> 5];
         it += byte_count * n;
         break;
      }
      case 3: { // bool or string
         const bool is_bool = (tag & 0b00'1'00'000) >> 5;
         ++it;
         if (is_bool) {
            const auto n = int_from_compressed(ctx, it, end);
            const auto num_bytes = (n + 7) / 8;
            it += num_bytes;
         }
         else {
            const auto n = int_from_compressed(ctx, it, end);
            it += n;
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
      const auto n = int_from_compressed(ctx, it, end);
      for (size_t i = 0; i < n; ++i) {
         skip_value_binary<Opts>(ctx, it, end);
      }
   }

   template <opts Opts>
   inline void skip_additional_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      ++it;
      skip_value_binary<Opts>(ctx, it, end);
   }

   template <opts Opts>
   inline void skip_value_binary(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      switch (uint8_t(*it) & 0b00000'111) {
      case tag::null: {
         ++it;
         break;
      }
      case tag::number: {
         skip_number_binary(ctx, it, end);
         break;
      }
      case tag::string: {
         skip_string_binary(ctx, it, end);
         break;
      }
      case tag::object: {
         skip_object_binary<Opts>(ctx, it, end);
         break;
      }
      case tag::typed_array: {
         skip_typed_array_binary<Opts>(ctx, it, end);
         break;
      }
      case tag::generic_array: {
         skip_untyped_array_binary<Opts>(ctx, it, end);
         break;
      }
      case tag::extensions: {
         skip_additional_binary<Opts>(ctx, it, end);
         break;
      }
      default:
         ctx.error = error_code::syntax_error;
      }
   }
}
