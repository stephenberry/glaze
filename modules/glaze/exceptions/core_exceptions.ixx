// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.exceptions.core_exceptions;

#if __cpp_exceptions

import glaze.core.read;
import glaze.core.write;

import glaze.util.string_literal;

namespace glz::ex
{
   export template <auto Opts, class T>
      requires read_supported<T, Opts.format>
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
   export template <auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   void write(T&& value, Buffer& buffer, is_context auto&& ctx)
   {
      const auto ec = glz::write<Opts>(std::forward<T>(value), buffer, ctx);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }

   export template <auto& Partial, auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   void write(T&& value, Buffer& buffer)
   {
      const auto ec = glz::write(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }

   export template <auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   void write(T&& value, Buffer& buffer)
   {
      glz::context ctx{};
      glz::ex::write<Opts>(std::forward<T>(value), buffer, ctx);
   }

   export template <auto Opts, class T>
      requires write_supported<T, Opts.format>
   [[nodiscard]] std::string write(T&& value)
   {
      const auto e = glz::write<Opts>(std::forward<T>(value));
      if (not e) [[unlikely]] {
         throw std::runtime_error(format_error(e));
      }
      return e.value();
   }

   export template <auto Opts, class T, raw_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] std::size_t write(T&& value, Buffer&& buffer)
   {
      const auto e = write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
      if (not e) [[unlikely]] {
         throw std::runtime_error(format_error(e, buffer));
      }
      return e.value();
   }

   // requires file_name to be null terminated
   export void buffer_to_file(auto&& buffer, const sv file_name)
   {
      const auto ec = buffer_to_file(buffer, file_name);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }
}

#endif
