// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __cpp_exceptions

// These files provide convenience functions that throw C++ exceptions, which can make code cleaner for users

#include "glaze/exceptions/binary_exceptions.hpp"
#include "glaze/exceptions/csv_exceptions.hpp"
#include "glaze/exceptions/json_exceptions.hpp"

namespace glz::ex
{
   template <opts Opts>
   void read(auto& value, auto&& buffer)
   {
      auto ec = glz::read<Opts>(value, buffer);
      if (ec) {
         if constexpr (Opts.format == json) {
            throw std::runtime_error("read error: " + glz::format_error(ec, buffer));
         }
         else {
            throw std::runtime_error("read error");
         }
      }
   }

   template <opts Opts, class T, output_buffer Buffer>
   void write(T&& value, Buffer& buffer) noexcept
   {
      glz::write<Opts>(std::forward<T>(value), buffer);
   }

   template <opts Opts, class T, raw_buffer Buffer>
   size_t write(T&& value, Buffer&& buffer) noexcept
   {
      return glz::write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}

namespace glz::ex
{
   template <class T>
   void read_file(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file(value, file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("read error");
      }
   }

   template <class T>
   void write_file(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::write_file(value, file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("write error");
      }
   }
}

#endif
