// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.exceptions.jsonb_exceptions;

#if __cpp_exceptions

import std;

import glaze.core.context;
import glaze.core.opts;

import glaze.jsonb;

import glaze.util.string_literal;

namespace glz::ex
{
   export template <class T, class Buffer>
   void read_jsonb(T& value, Buffer&& buffer)
   {
      const auto ec = glz::read_jsonb(value, std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("read_jsonb error");
      }
   }

   export template <class T, class Buffer>
   [[nodiscard]] T read_jsonb(Buffer&& buffer)
   {
      const auto ex = glz::read_jsonb<T>(std::forward<Buffer>(buffer));
      if (!ex) {
         throw std::runtime_error("read_jsonb error");
      }
      return ex.value();
   }

   export template <auto Opts = opts{}, class T>
   void read_file_jsonb(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file_jsonb<Opts>(value, file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("read_file_jsonb error for: " + std::string(file_name));
      }
   }
}

namespace glz::ex
{
   export template <class T, class Buffer>
   void write_jsonb(T&& value, Buffer&& buffer)
   {
      const auto ec = glz::write_jsonb(std::forward<T>(value), std::forward<Buffer>(buffer));
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error("write_jsonb error");
      }
   }

   export template <class T>
   [[nodiscard]] auto write_jsonb(T&& value)
   {
      auto result = glz::write_jsonb(std::forward<T>(value));
      if (result) {
         return result.value();
      }
      else {
         throw std::runtime_error("write_jsonb error");
      }
   }

   export template <auto Opts = opts{}, class T>
   void write_file_jsonb(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::write_file_jsonb<Opts>(std::forward<T>(value), file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("write_file_jsonb error for: " + std::string(file_name));
      }
   }

   // Convert JSONB to JSON, throwing on error.
   export template <class JSONBBuffer>
   [[nodiscard]] std::string jsonb_to_json(const JSONBBuffer& input)
   {
      auto result = glz::jsonb_to_json(input);
      if (result) {
         return result.value();
      }
      throw std::runtime_error("jsonb_to_json error");
   }
}

#endif
