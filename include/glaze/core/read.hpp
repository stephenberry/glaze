// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <span>

#include "glaze/api/std/span.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/parse.hpp"

#include "glaze/json/json_concepts.hpp"

namespace glz
{
   template <opts Opts, bool Padded = false>
   auto read_iterators(is_context auto&& ctx, contiguous auto&& buffer) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      // TODO: Should these be unsigned char to avoid needing to cast to uint8_t for lookup tables???
      auto it = reinterpret_cast<const char*>(buffer.data());
      auto end = reinterpret_cast<const char*>(buffer.data()); // to be incremented

      if (buffer.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
         return std::pair{it, end};
      }

      using Buffer = std::remove_cvref_t<decltype(buffer)>;
      if constexpr (is_specialization_v<Buffer, std::basic_string> ||
                    is_specialization_v<Buffer, std::basic_string_view> || span<Buffer>) {
         if constexpr (Padded) {
            end += buffer.size() - padding_bytes;
         }
         else {
            end += buffer.size();
         }
      }
      else {
         // if not a std::string, std::string_view, or span, check that the last character is a null character
         end += buffer.size() - 1;
         if constexpr (Padded) {
            end -= padding_bytes;
         }
         if (*end != '\0') {
            ctx.error = error_code::data_must_be_null_terminated;
         }
      }

      return std::pair{it, end};
   }
   
   template <class T, class Buffer>
   consteval opts make_read_options(const opts& in) {
      opts out = in;
      const bool use_padded = resizable<Buffer> && non_const_buffer<Buffer> && !has_disable_padding(in);
      if (use_padded) {
         out.internal |= uint32_t(opts::internal::is_padded);
      }
      
      // TODO: Add is_null_terminated option for std::string
      //using Buffer = std::remove_cvref_t<decltype(buffer)>;
      //if constexpr (is_specialization_v<Buffer, std::basic_string>) {
      //
      //}
      
      return out;
   }

   template <opts Opts, class T>
      requires read_supported<Opts.format, T>
   [[nodiscard]] error_ctx read(T& value, contiguous auto&& buffer, is_context auto&& ctx) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);
      using Buffer = std::remove_reference_t<decltype(buffer)>;

      if (buffer.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
         return {ctx.error, ctx.custom_error_message, 0, ctx.includer_error};
      }

      constexpr bool use_padded = resizable<Buffer> && non_const_buffer<Buffer> && !has_disable_padding(Opts);

      if constexpr (use_padded) {
         // Pad the buffer for SWAR
         buffer.resize(buffer.size() + padding_bytes);
      }

      auto [it, end] = read_iterators<Opts, use_padded>(ctx, buffer);
      auto start = it;
      if (bool(ctx.error)) [[unlikely]] {
         goto finish;
      }
      
      if constexpr (json_read_object<T>) {
         // Require closing `}` and use as sentinel
         --end;
         if (*end != '}') [[unlikely]] {
            ctx.error = error_code::syntax_error;
            goto finish;
         }
      }
      else if constexpr (json_read_array<T>) {
         // Require closing `]` and use as sentinel
         --end;
         if (*end != ']') [[unlikely]] {
            ctx.error = error_code::syntax_error;
            goto finish;
         }
      }
      
      static constexpr opts options = make_read_options<T, Buffer>(Opts);
      detail::read<Opts.format>::template op<options>(value, ctx, it, end);
      
      if constexpr (not options.null_terminated) {
         if constexpr (json_read_object<T> || json_read_array<T>) {
            static constexpr uint32_t end_of_sentinels = 2;
            if (uint32_t(ctx.error) > end_of_sentinels && ctx.indentation_level != 0) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
            }
         }
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
      if constexpr (not Opts.null_terminated && json_read_object<T> || json_read_array<T>) {
         if (it == end) [[likely]] {
            if constexpr (json_read_object<T>) {
               if (ctx.error == error_code::brace_sentinel) [[likely]] {
                  ctx.error = error_code::none;
                  ++it;
               }
               else [[unlikely]] {
                  ctx.error = error_code::expected_brace;
               }
            }
            else if constexpr (json_read_array<T>) {
               if (ctx.error == error_code::bracket_sentinel) [[likely]] {
                  ctx.error = error_code::none;
                  ++it;
               }
               else [[unlikely]] {
                  ctx.error = error_code::expected_bracket;
               }
            }
         }
         else if (ctx.error == error_code::none) {
            ctx.error = error_code::syntax_error;
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
   [[nodiscard]] error_ctx read(T& value, contiguous auto&& buffer) noexcept
   {
      context ctx{};
      return read<Opts>(value, buffer, ctx);
   }

   template <class T>
   concept c_style_char_buffer = std::convertible_to<std::remove_cvref_t<T>, std::string_view> && !has_data<T>;

   // for char array input
   template <opts Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<Opts.format, T>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer, auto&& ctx) noexcept
   {
      const auto str = std::string_view{std::forward<Buffer>(buffer)};
      if (str.empty()) {
         return {error_code::no_read_input};
      }
      return read<Opts>(value, str, ctx);
   }

   template <opts Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<Opts.format, T>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer) noexcept
   {
      context ctx{};
      return read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }
}
