// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "glaze/core/context.hpp"

namespace glz
{
   template <class T>
   [[nodiscard]] error_code file_to_buffer(T& buffer, std::ifstream& file, const std::string_view path) noexcept
   {
      if (!file) {
         return error_code::file_open_failure;
      }
      
      const auto n = std::filesystem::file_size(path);
      buffer.resize(n);
      file.read(buffer.data(), n);

      return {};
   }

   template <class T>
   [[nodiscard]] error_code file_to_buffer(T& buffer, const std::string& file_name) noexcept
   {
      std::ifstream file(file_name, std::ios::binary);
      return file_to_buffer(buffer, file, file_name);
   }

   template <class T>
   [[nodiscard]] error_code file_to_buffer(T& buffer, const std::string_view file_name) noexcept
   {
      std::ifstream file(std::string(file_name), std::ios::binary);
      return file_to_buffer(buffer, file, file_name);
   }

   template <class T>
   std::string file_to_buffer(T&& file_name) noexcept
   {
      std::string buffer{};
      file_to_buffer(buffer, std::forward<T>(file_name));
      return buffer;
   }

   inline std::filesystem::path relativize_if_not_absolute(const std::filesystem::path& working_directory,
                                                           const std::filesystem::path& filepath) noexcept
   {
      if (filepath.is_absolute()) {
         return filepath;
      }

      return working_directory / filepath;
   }
}
