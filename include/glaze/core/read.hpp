// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <span>

#include "glaze/api/std/span.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/validate.hpp"

namespace glz
{
   template <opts Opts>
   inline decltype(auto) read_iterators(is_context auto&& ctx, detail::contiguous auto&& buffer) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      auto b = reinterpret_cast<const char*>(buffer.data());
      auto e = reinterpret_cast<const char*>(buffer.data()); // to be incremented

      using Buffer = std::decay_t<decltype(buffer)>;
      if constexpr (is_specialization_v<Buffer, std::basic_string> ||
                    is_specialization_v<Buffer, std::basic_string_view> || span<Buffer> || Opts.format == binary) {
         e += buffer.size();

         if (b == e) {
            ctx.error = error_code::no_read_input;
         }
      }
      else {
         // if not a std::string or a std::string_view, check that the last character is a null character
         // this is not required for binary specification reading, because we require the data to be properly formatted
         if (buffer.empty()) {
            ctx.error = error_code::no_read_input;
         }
         e += buffer.size() - 1;
         if (*e != '\0') {
            ctx.error = error_code::data_must_be_null_terminated;
         }
      }

      return std::pair{b, e};
   }

   // For reading json from a std::vector<char>, std::deque<char> and the like
   template <opts Opts>
   [[nodiscard]] inline parse_error read(auto& value, detail::contiguous auto&& buffer, is_context auto&& ctx) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      auto b = reinterpret_cast<const char*>(buffer.data());
      auto e = reinterpret_cast<const char*>(buffer.data()); // to be incremented

      auto start = b;

      using Buffer = std::decay_t<decltype(buffer)>;
      if constexpr (is_specialization_v<Buffer, std::basic_string> ||
                    is_specialization_v<Buffer, std::basic_string_view> || span<Buffer> || Opts.format == binary) {
         e += buffer.size();

         if (b == e) {
            ctx.error = error_code::no_read_input;
            return {ctx.error, 0};
         }
      }
      else {
         // if not a std::string or a std::string_view, check that the last character is a null character
         // this is not required for binary specification reading, because we require the data to be properly formatted
         if (buffer.empty()) {
            ctx.error = error_code::no_read_input;
            return {ctx.error, 0};
         }
         e += buffer.size() - 1;
         if (*e != '\0') {
            ctx.error = error_code::data_must_be_null_terminated;
            return {ctx.error, 0};
         }
      }

      detail::read<Opts.format>::template op<Opts>(value, ctx, b, e);

      if constexpr (Opts.force_conformance) {
         if (b < e) {
            detail::skip_ws<Opts>(ctx, b, e);
            if (b != e) {
               ctx.error = error_code::syntax_error;
            }
         }
      }

      return {ctx.error, static_cast<size_t>(std::distance(start, b))};
   }

   template <opts Opts>
   [[nodiscard]] inline parse_error read(auto& value, detail::contiguous auto&& buffer) noexcept
   {
      context ctx{};
      return read<Opts>(value, buffer, ctx);
   }

   template <class T>
   concept string_viewable = std::convertible_to<std::decay_t<T>, std::string_view> && !detail::has_data<T>;

   // for char array input
   template <opts Opts, class T, string_viewable Buffer>
   [[nodiscard]] inline parse_error read(T& value, Buffer&& buffer, auto&& ctx) noexcept
   {
      const auto str = std::string_view{std::forward<Buffer>(buffer)};
      if (str.empty()) {
         return {error_code::no_read_input, 0};
      }
      return read<Opts>(value, str, ctx);
   }

   template <opts Opts, class T, string_viewable Buffer>
   [[nodiscard]] inline parse_error read(T& value, Buffer&& buffer) noexcept
   {
      context ctx{};
      return read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }
}
