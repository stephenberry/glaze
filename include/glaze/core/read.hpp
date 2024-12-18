// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <span>

#include "glaze/api/std/span.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/parse.hpp"

namespace glz
{
   template <opts Opts, bool Padded = false>
   auto read_iterators(contiguous auto&& buffer) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      auto it = reinterpret_cast<const char*>(buffer.data());
      auto end = reinterpret_cast<const char*>(buffer.data()); // to be incremented

      if constexpr (Padded) {
         end += buffer.size() - padding_bytes;
      }
      else {
         end += buffer.size();
      }

      return std::pair{it, end};
   }

   template <opts Opts, class T>
      requires read_supported<Opts.format, T>
   [[nodiscard]] error_ctx read(T& value, contiguous auto&& buffer, is_context auto&& ctx)
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);
      using Buffer = std::remove_reference_t<decltype(buffer)>;

      if constexpr (Opts.format != NDJSON) {
         if (buffer.empty()) [[unlikely]] {
            ctx.error = error_code::no_read_input;
            return {ctx.error, ctx.custom_error_message, 0, ctx.includer_error};
         }
      }

      constexpr bool use_padded = resizable<Buffer> && non_const_buffer<Buffer> && !has_disable_padding(Opts);

      if constexpr (use_padded) {
         // Pad the buffer for SWAR
         buffer.resize(buffer.size() + padding_bytes);
      }

      auto [it, end] = read_iterators<Opts, use_padded>(buffer);
      auto start = it;
      if (bool(ctx.error)) [[unlikely]] {
         goto finish;
      }

      if constexpr (use_padded) {
         detail::read<Opts.format>::template op<is_padded_on<Opts>()>(value, ctx, it, end);
      }
      else {
         detail::read<Opts.format>::template op<is_padded_off<Opts>()>(value, ctx, it, end);
      }

      if (bool(ctx.error)) [[unlikely]] {
         goto finish;
      }

      // The JSON RFC 8259 defines: JSON-text = ws value ws
      // So, trailing whitespace is permitted and sometimes we want to
      // validate this, even though this memory will not affect Glaze.
      if constexpr (Opts.validate_trailing_whitespace) {
         if (it < end) {
            detail::skip_ws<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               goto finish;
            }
            if (it != end) [[unlikely]] {
               ctx.error = error_code::syntax_error;
            }
         }
      }

   finish:
      // We don't do depth validation for partial reading
      if constexpr (Opts.partial_read) {
         if (ctx.error == error_code::partial_read_complete) [[likely]] {
            ctx.error = error_code::none;
         }
         else if (ctx.error == error_code::end_reached && ctx.indentation_level == 0) {
            ctx.error = error_code::none;
         }
      }
      else {
         if (ctx.error == error_code::end_reached && ctx.indentation_level == 0) {
            ctx.error = error_code::none;
         }
      }

      if constexpr (use_padded) {
         // Restore the original buffer state
         buffer.resize(buffer.size() - padding_bytes);
      }

      return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
   }

   template <opts Opts, class T>
      requires read_supported<Opts.format, T>
   [[nodiscard]] error_ctx read(T& value, contiguous auto&& buffer)
   {
      context ctx{};
      return read<Opts>(value, buffer, ctx);
   }

   template <class T>
   concept c_style_char_buffer = std::convertible_to<std::remove_cvref_t<T>, std::string_view> && !has_data<T>;

   template <class T>
   concept is_buffer = c_style_char_buffer<T> || contiguous<T>;

   // for char array input
   template <opts Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<Opts.format, T>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer, auto&& ctx)
   {
      const auto str = std::string_view{std::forward<Buffer>(buffer)};
      if (str.empty()) {
         return {error_code::no_read_input};
      }
      return read<Opts>(value, str, ctx);
   }

   template <opts Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<Opts.format, T>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer)
   {
      context ctx{};
      return read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }
}
