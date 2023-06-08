// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __cpp_exceptions

#include "glaze/glaze.hpp"

// This file provides convenience functions that throw C++ exceptions, which can make code cleaner for users

namespace glz::ex
{
   template <class Buffer>
   void validate_json(Buffer&& buffer)
   {
      auto ec = glz::validate_json(std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("validate_json error: " + glz::format_error(ec, buffer));
      }
   }

   template <class T, class Buffer>
   void read_json(T& value, Buffer&& buffer)
   {
      auto ec = glz::read_json(value, std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("read_json error: " + glz::format_error(ec, buffer));
      }
   }

   template <class T, class Buffer>
   [[nodiscard]] T read_json(Buffer&& buffer) noexcept
   {
      const auto ex = glz::read_json<T>(std::forward<Buffer>(buffer));
      if (ex) {
         throw std::runtime_error("read_json error: " + glz::format_error(ex.error(), buffer));
      }
      return ex.value();
   }

   template <auto Opts = opts{}, class T>
   void read_file_json(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file_json(value, file_name, buffer);
      if (ec) {
         throw std::runtime_error("read_file_json error: " + glz::format_error(ec, buffer));
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
      auto ec = glz::write_file_json(std::forward<T>(value), file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open");
      }
      else {
         throw std::runtime_error("write_file_json");
      }
   }
}

namespace glz::ex
{
   template <class T, class Buffer>
   void read_binary(T& value, Buffer&& buffer)
   {
      auto ec = glz::read_binary(value, std::forward<Buffer>(buffer));
      if (ec) {
         throw std::runtime_error("read_binary error");
      }
   }

   template <class T, class Buffer>
   [[nodiscard]] T read_binary(Buffer&& buffer) noexcept
   {
      const auto ex = glz::read_binary<T>(std::forward<Buffer>(buffer));
      if (ex) {
         throw std::runtime_error("read_binary error");
      }
      return ex.value();
   }

   template <auto Opts = opts{}, class T>
   void read_file_binary(T& value, const sv file_name, auto&& buffer)
   {
      const auto ec = glz::read_file_binary(value, file_name, buffer);
      if (ec) {
         throw std::runtime_error("read_file_binary error");
      }
   }
}

namespace glz::ex
{
   template <class T, class Buffer>
   void write_binary(T&& value, Buffer&& buffer)
   {
      glz::write_binary(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   [[nodiscard]] auto write_binary(T&& value)
   {
      return glz::write_binary(std::forward<T>(value));
   }

   template <class T>
   void write_file_binary(T&& value, const std::string& file_name, auto&& buffer)
   {
      auto ec = glz::write_file_binary(std::forward<T>(value), file_name, buffer);
      if (ec == glz::error_code::file_open_failure) {
         throw std::runtime_error("file failed to open");
      }
      else {
         throw std::runtime_error("write_file_json");
      }
   }
}

#endif
