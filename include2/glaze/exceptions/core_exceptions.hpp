// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __cpp_exceptions

#include "glaze/core/read.hpp"
#include "glaze/core/write.hpp"

namespace glz::ex
{
   template <opts Opts, class T>
      requires read_supported<Opts.format, T>
   void read(T&& value, auto&& buffer)
   {
      const auto ec = glz::read<Opts>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }
}

namespace glz::ex
{
   // For writing to a std::string, std::vector<char>, std::deque<char> and the like
   template <opts Opts, class T, output_buffer Buffer>
      requires write_supported<Opts.format, T>
   void write(T&& value, Buffer& buffer, is_context auto&& ctx)
   {
      const auto ec = glz::write<Opts>(std::forward<T>(value), buffer, ctx);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }

   template <auto& Partial, opts Opts, class T, output_buffer Buffer>
      requires write_supported<Opts.format, T>
   void write(T&& value, Buffer& buffer)
   {
      const auto ec = glz::write(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }

   template <opts Opts, class T, output_buffer Buffer>
      requires write_supported<Opts.format, T>
   void write(T&& value, Buffer& buffer)
   {
      glz::context ctx{};
      glz::ex::write<Opts>(std::forward<T>(value), buffer, ctx);
   }

   template <opts Opts, class T>
      requires write_supported<Opts.format, T>
   [[nodiscard]] std::string write(T&& value)
   {
      const auto e = glz::write<Opts>(std::forward<T>(value));
      if (not e) [[unlikely]] {
         throw std::runtime_error(format_error(e));
      }
      return e.value();
   }

   template <opts Opts, class T, raw_buffer Buffer>
      requires write_supported<Opts.format, T>
   [[nodiscard]] size_t write(T&& value, Buffer&& buffer)
   {
      const auto e = write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
      if (not e) [[unlikely]] {
         throw std::runtime_error(format_error(e, buffer));
      }
      return e.value();
   }

   // requires file_name to be null terminated
   void buffer_to_file(auto&& buffer, const sv file_name)
   {
      const auto ec = buffer_to_file(buffer, file_name);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }
}

#endif
