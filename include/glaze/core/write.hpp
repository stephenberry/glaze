// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <fstream>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"

namespace glz
{
   // For writing to a std::string, std::vector<char>, std::deque<char> and the like
   template <opts Opts, class T, output_buffer Buffer>
      requires write_supported<Opts.format, T>
   inline void write(T&& value, Buffer& buffer, is_context auto&& ctx) noexcept
   {
      if constexpr (resizable<Buffer>) {
         if (buffer.empty()) {
            buffer.resize(2 * write_padding_bytes);
         }
      }
      size_t ix = 0; // overwrite index
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      if constexpr (resizable<Buffer>) {
         buffer.resize(ix);
      }
   }

   template <auto& Partial, opts Opts, class T, output_buffer Buffer>
      requires write_supported<Opts.format, T>
   [[nodiscard]] inline write_error write(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (resizable<Buffer>) {
         if (buffer.empty()) {
            buffer.resize(2 * write_padding_bytes);
         }
      }
      context ctx{};
      size_t ix = 0;
      const auto error =
         detail::write_partial<Opts.format>::template op<Partial, Opts>(std::forward<T>(value), ctx, buffer, ix);
      if constexpr (resizable<Buffer>) {
         buffer.resize(ix);
      }
      return error;
   }

   template <opts Opts, class T, output_buffer Buffer>
      requires write_supported<Opts.format, T>
   inline void write(T&& value, Buffer& buffer) noexcept
   {
      context ctx{};
      write<Opts>(std::forward<T>(value), buffer, ctx);
   }

   template <opts Opts, class T>
      requires write_supported<Opts.format, T>
   [[nodiscard]] inline std::string write(T&& value) noexcept
   {
      std::string buffer{};
      context ctx{};
      write<Opts>(std::forward<T>(value), buffer, ctx);
      return buffer;
   }

   template <opts Opts, class T, raw_buffer Buffer>
      requires write_supported<Opts.format, T>
   [[nodiscard]] inline size_t write(T&& value, Buffer&& buffer, is_context auto&& ctx) noexcept
   {
      size_t ix = 0;
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      return ix;
   }

   template <opts Opts, class T, raw_buffer Buffer>
      requires write_supported<Opts.format, T>
   [[nodiscard]] inline size_t write(T&& value, Buffer&& buffer) noexcept
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer), ctx);
   }

   // requires file_name to be null terminated
   [[nodiscard]] inline error_code buffer_to_file(auto&& buffer, const sv file_name) noexcept
   {
      auto file = std::ofstream(file_name.data(), std::ios::out);
      if (!file) {
         return error_code::file_open_failure;
      }
      file.write(buffer.data(), buffer.size());
      return {};
   }
}
