// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.exceptions.msgpack_exceptions;

#if __cpp_exceptions

import std;

import glaze.core.context;
import glaze.core.opts;
import glaze.exceptions.core_exceptions;
import glaze.msgpack;
import glaze.util.string_literal;

namespace glz::ex
{
   export template <class T, class Buffer>
   void read_msgpack(T& value, Buffer&& buffer)
   {
      const auto ec = glz::read_msgpack(value, std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("read_msgpack error");
      }
   }

   export template <class T, class Buffer>
   [[nodiscard]] T read_msgpack(Buffer&& buffer)
   {
      const auto ex = glz::read_msgpack<T>(std::forward<Buffer>(buffer));
      if (!ex) {
         throw std::runtime_error("read_msgpack error");
      }
      return ex.value();
   }

   export template <auto Opts = opts{}, class T>
   void read_file_msgpack(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file_msgpack<Opts>(value, file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("read_file_msgpack error for: " + std::string(file_name));
      }
   }
}

namespace glz::ex
{
   export template <class T, class Buffer>
   void write_msgpack(T&& value, Buffer&& buffer)
   {
      const auto ec = glz::write_msgpack(std::forward<T>(value), std::forward<Buffer>(buffer));
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error("write_msgpack error");
      }
   }

   export template <class T>
   [[nodiscard]] auto write_msgpack(T&& value)
   {
      auto result = glz::write_msgpack(std::forward<T>(value));
      if (result) {
         return result.value();
      }
      else {
         throw std::runtime_error("write_msgpack error");
      }
   }

   export template <auto Opts = opts{}, class T>
   void write_file_msgpack(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::write_file_msgpack<Opts>(std::forward<T>(value), file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("write_file_msgpack error for: " + std::string(file_name));
      }
   }
}

#endif
