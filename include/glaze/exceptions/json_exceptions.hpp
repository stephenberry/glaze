// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __cpp_exceptions

#include "glaze/glaze.hpp"

namespace glz::ex
{
   template <class Buffer>
   void validate_json(Buffer&& buffer)
   {
      const auto ec = glz::validate_json(std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("validate_json error: " + glz::format_error(ec, buffer));
      }
   }

   template <class T, class Buffer>
   void read_json(T& value, Buffer&& buffer)
   {
      const auto ec = glz::read_json(value, std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("read_json error: " + glz::format_error(ec, buffer));
      }
   }

   template <class T, class Buffer>
   [[nodiscard]] T read_json(Buffer&& buffer)
   {
      const auto ex = glz::read_json<T>(std::forward<Buffer>(buffer));
      if (!ex) {
         throw std::runtime_error("read_json error: " + glz::format_error(ex.error(), buffer));
      }
      return ex.value();
   }

   template <auto Opts = opts{}, class T>
   void read_file_json(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file_json(value, file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("read_file_json error: " + glz::format_error(ec, buffer));
      }
   }

   template <class T, class Buffer>
   void read_ndjson(T& value, Buffer&& buffer)
   {
      const auto ec = glz::read_ndjson(value, std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("read_ndjson error: " + glz::format_error(ec, buffer));
      }
   }

   template <class T, class Buffer>
   [[nodiscard]] T read_ndjson(Buffer&& buffer)
   {
      const auto ex = glz::read_ndjson<T>(std::forward<Buffer>(buffer));
      if (!ex) {
         throw std::runtime_error("read_ndjson error: " + glz::format_error(ex.error(), buffer));
      }
      return ex.value();
   }

   template <auto Opts = opts{}, class T>
   void read_file_ndjson(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file_ndjson(value, file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("read_file_ndjson error: " + glz::format_error(ec, buffer));
      }
   }
}

namespace glz::ex
{
   template <class T, class Buffer>
   void write_json(T&& value, Buffer&& buffer)
   {
      glz::write_json(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   [[nodiscard]] auto write_json(T&& value)
   {
      return glz::write_json(std::forward<T>(value));
   }

   template <class T, class Buffer>
   void write_jsonc(T&& value, Buffer&& buffer)
   {
      glz::write_jsonc(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   [[nodiscard]] auto write_jsonc(T&& value)
   {
      return glz::write_jsonc(std::forward<T>(value));
   }

   template <class T>
   void write_file_json(T&& value, const std::string& file_name, auto&& buffer)
   {
      const auto ec = glz::write_file_json(std::forward<T>(value), file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("write_file_json error");
      }
   }

   template <class T, class Buffer>
   void write_ndjson(T&& value, Buffer&& buffer)
   {
      glz::write_ndjson(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   [[nodiscard]] auto write_ndjson(T&& value)
   {
      return glz::write_ndjson(std::forward<T>(value));
   }

   template <class T>
   void write_file_ndjson(T&& value, const std::string& file_name, auto&& buffer)
   {
      const auto ec = glz::write_file_ndjson(std::forward<T>(value), file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open: " + std::string(file_name));
      }
      else if (ec) {
         throw std::runtime_error("write_file_ndjson error");
      }
   }
}

#endif
