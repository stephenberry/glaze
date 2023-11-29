// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <fstream>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/validate.hpp"

namespace glz
{
   template <class Buffer>
   concept raw_buffer = std::same_as<std::decay_t<Buffer>, char*>;

   template <class Buffer>
   concept output_buffer = range<Buffer> && (sizeof(range_value_t<Buffer>) == sizeof(char));

   template <class T>
   GLZ_ALWAYS_INLINE auto data_ptr(T& buffer) noexcept
   {
      if constexpr (detail::resizeable<T>) {
         return buffer.data();
      }
      else {
         return buffer;
      }
   }

   // For writing to a std::string, std::vector<char>, std::deque<char> and the like
   template <opts Opts, class T, output_buffer Buffer>
   inline void write(T&& value, Buffer& buffer, is_context auto&& ctx) noexcept
   {
      if constexpr (detail::resizeable<Buffer>) {
         if (buffer.empty()) {
            buffer.resize(128);
         }
      }
      size_t ix = 0; // overwrite index
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      if constexpr (detail::resizeable<Buffer>) {
         buffer.resize(ix);
      }
   }

   template <opts Opts, class T, output_buffer Buffer>
   inline void write(T&& value, Buffer& buffer) noexcept
   {
      context ctx{};
      write<Opts>(std::forward<T>(value), buffer, ctx);
   }

   template <opts Opts, class T>
   inline std::string write(T&& value) noexcept
   {
      std::string buffer{};
      context ctx{};
      write<Opts>(std::forward<T>(value), buffer, ctx);
      return buffer;
   }

   template <opts Opts, class T, raw_buffer Buffer>
   inline size_t write(T&& value, Buffer&& buffer, is_context auto&& ctx) noexcept
   {
      size_t ix = 0;
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      return ix;
   }

   template <opts Opts, class T, raw_buffer Buffer>
   inline size_t write(T&& value, Buffer&& buffer) noexcept
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer), ctx);
   }

   [[nodiscard]] GLZ_ALWAYS_INLINE error_code buffer_to_file(auto&& buffer, auto&& file_name) noexcept
   {
      auto file = std::ofstream(file_name, std::ios::out);
      if (!file) {
         return error_code::file_open_failure;
      }
      file.write(buffer.data(), buffer.size());
      return {};
   }
}
