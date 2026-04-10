// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.exceptions.cbor_exceptions;

#if __cpp_exceptions

import std;

import glaze.cbor;

import glaze.core.context;
import glaze.core.opts;

import glaze.exceptions.core_exceptions;

import glaze.util.string_literal;

namespace glz::ex
{
   export template <class T, class Buffer>
   void read_cbor(T& value, Buffer&& buffer)
   {
      const auto ec = glz::read_cbor(value, std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("read_cbor error");
      }
   }

   export template <class T, class Buffer>
   [[nodiscard]] T read_cbor(Buffer&& buffer)
   {
      const auto ex = glz::read_cbor<T>(std::forward<Buffer>(buffer));
      if (!ex) {
         throw std::runtime_error("read_cbor error");
      }
      return ex.value();
   }

   export template <auto Opts = opts{}, class T>
   void read_file_cbor(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file_cbor<Opts>(value, file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("read_file_cbor error for: " + std::string(file_name));
      }
   }
}

namespace glz::ex
{
   export template <class T, class Buffer>
   void write_cbor(T&& value, Buffer&& buffer)
   {
      const auto ec = glz::write_cbor(std::forward<T>(value), std::forward<Buffer>(buffer));
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error("write_cbor error");
      }
   }

   export template <class T>
   [[nodiscard]] auto write_cbor(T&& value)
   {
      auto result = glz::write_cbor(std::forward<T>(value));
      if (result) {
         return result.value();
      }
      else {
         throw std::runtime_error("write_cbor error");
      }
   }

   export template <auto Opts = opts{}, class T>
   void write_file_cbor(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::write_file_cbor<Opts>(std::forward<T>(value), file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("write_file_cbor error for: " + std::string(file_name));
      }
   }
}

#endif
