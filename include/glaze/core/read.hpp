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
   
   constexpr std::array<bool, 256> sentinel_table = [] {
      std::array<bool, 256> t{};
      t[']'] = true;
      t['}'] = true;
      return t;
   }();
   
   template <class T, class Buffer>
   consteval opts make_read_options(const opts& in) {
      opts out = in;
      const bool use_padded = resizable<Buffer> && non_const_buffer<Buffer> && !has_disable_padding(in);
      if (use_padded) {
         out.internal |= uint32_t(opts::internal::is_padded);
      }
      
      // std::string is null terminated
      using B = std::remove_cvref_t<Buffer>;
      if constexpr (is_specialization_v<B, std::basic_string>) {
         out.null_terminated = true;
      }
      
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

      auto p = read_iterators<Opts, use_padded>(ctx, buffer);
      // explicitly get [it, end] because the clang debugger keeps jumping back to structured bindings
      // someday you can write: auto [it, end] = ... and the debugger won't be a pain
      auto it = p.first;
      auto end = p.second;
      auto start = it;
      if (bool(ctx.error)) [[unlikely]] {
         goto finish;
      }
      
      static constexpr opts options = make_read_options<T, Buffer>(Opts);
      
      // Whether to use contextual sentinels for non-null terminated inputs
      static constexpr bool use_json_sentinels = (Opts.format == json) && not (options.null_terminated) && (json_read_object<T> || json_read_array<T>);
      
      // Custom decoding and types like glz::skip consume the entire thing they are decoding
      // Indeed, most all parse implementation should be expected to consume the terminating characters
      // This does pose an issue when dealing with non-null terminated buffers because
      // if we progress beyond the length of the buffer we must not de-reference.
      // By setting using error context we block de-referencing, so we can progress the iterator
      // when using contextual sentinels.
      // In short, we don't assume compile time knowledge of the sentinel character at this level.
      
      // glz::skip needs to handle either terminating `]` or `}`
      // Rather than matching exactly an object or array, either of these types will consider
      // the opening character and match to the opening.
      // This allows dynamic types that support both arrays or objects.
      if constexpr (use_json_sentinels) {
         --end; // We move back to the last allocated character that must exist
         if (not sentinel_table[uint8_t(*end)]) [[unlikely]] {
            it = end;
            ctx.error = error_code::expected_sentinel;
            goto finish;
         }
      }
      
      detail::read<Opts.format>::template op<options>(value, ctx, it, end);
      
      if constexpr (use_json_sentinels) {
         ++end; // reset the end iterator so that we can know if we have decoded the entire buffer
      }
      
      static constexpr uint32_t normal_errors = uint32_t(error_code::no_read_input);
      if constexpr (use_json_sentinels) {
         if ((uint32_t(ctx.error) < normal_errors) //
             && (ctx.indentation_level != 0) //
             && (not ctx.partial)) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
         }
         
         if (bool(ctx.error) >= normal_errors) [[unlikely]] {
            goto finish;
         }
      }
      else {
         if (bool(ctx.error)) [[unlikely]] {
            goto finish;
         }
      }

      // The JSON RFC 8259 defines: JSON-text = ws value ws
      // So, trailing whitespace is permitted and sometimes we want to
      // validate this, even though this memory will not affect Glaze.
      if constexpr (options.validate_trailing_whitespace) {
         if constexpr (options.null_terminated) {
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
         else {
            if (it < end) {
               detail::skip_ws_end_checks<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  goto finish;
               }
               if (it != end) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
               }
            }
         }
      }

   finish:
      if constexpr (Opts.format == json && not options.null_terminated && (json_read_object<T> || json_read_array<T>)) {
         if (bool(ctx.error) && uint32_t(ctx.error) < normal_errors) [[likely]] {
            // If we hit a sentinel then we clear the error
            // We could have only hit a sentinel if we reached the end of the buffer
            // If we didn't hit a sentinel then we assume we have partial reading
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
