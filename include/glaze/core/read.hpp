// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <span>

#include "glaze/api/std/span.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/parse.hpp"

namespace glz
{
   template <opts Opts>
   auto read_iterators(is_context auto&& ctx,contiguous auto&& buffer) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      auto it = reinterpret_cast<const char*>(buffer.data());
      auto end = reinterpret_cast<const char*>(buffer.data()); // to be incremented

      using Buffer = std::remove_cvref_t<decltype(buffer)>;
      if constexpr (is_specialization_v<Buffer, std::basic_string> ||
                    is_specialization_v<Buffer, std::basic_string_view> || span<Buffer>) {
         end += buffer.size();

         if (it == end) {
            ctx.error = error_code::no_read_input;
         }
      }
      else {
         // if not a std::string, std::string_view, or span, check that the last character is a null character
         if (buffer.empty()) {
            ctx.error = error_code::no_read_input;
         }
         else {
            end += buffer.size() - 1;
            if (*end != '\0') {
               ctx.error = error_code::data_must_be_null_terminated;
            }
         }
      }

      return std::pair{it, end};
   }

   // For reading json from a std::vector<char>, std::deque<char> and the like
   template <opts Opts, class T>
      requires read_supported<Opts.format, T>
   [[nodiscard]] parse_error read(T& value, contiguous auto&& buffer, is_context auto&& ctx) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);
      
      auto [it, end] = read_iterators<Opts>(ctx, buffer);
      if (bool(ctx.error)) [[unlikely]] {
         return {ctx.error, 0};
      }

      auto start = it;

      detail::read<Opts.format>::template op<Opts>(value, ctx, it, end);
      if (bool(ctx.error)) [[unlikely]] {
         return {ctx.error, size_t(it - start), ctx.includer_error};
      }

      if constexpr (Opts.force_conformance) {
         if (it < end) {
            detail::skip_ws_no_pre_check<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return {ctx.error, size_t(it - start), ctx.includer_error};
            }
            if (it != end) {
               ctx.error = error_code::syntax_error;
            }
         }
      }

      return {ctx.error, size_t(it - start), ctx.includer_error};
   }

   template <opts Opts, class T>
      requires read_supported<Opts.format, T>
   [[nodiscard]] parse_error read(T& value, contiguous auto&& buffer) noexcept
   {
      context ctx{};
      return read<Opts>(value, buffer, ctx);
   }

   template <class T>
   concept c_style_char_buffer = std::convertible_to<std::remove_cvref_t<T>, std::string_view> && !has_data<T>;

   // for char array input
   template <opts Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<Opts.format, T>
   [[nodiscard]] parse_error read(T& value, Buffer&& buffer, auto&& ctx) noexcept
   {
      const auto str = std::string_view{std::forward<Buffer>(buffer)};
      if (str.empty()) {
         return {error_code::no_read_input, 0};
      }
      return read<Opts>(value, str, ctx);
   }

   template <opts Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<Opts.format, T>
   [[nodiscard]] parse_error read(T& value, Buffer&& buffer) noexcept
   {
      context ctx{};
      return read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }
}
