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
   [[nodiscard]] error_code file_to_buffer(T& buffer, const std::string_view file_name) noexcept
   {
      std::ifstream file(std::string(file_name), std::ios::binary);

      if (!file) {
         return error_code::file_open_failure;
      }

      file.seekg(0, std::ios::end);
      buffer.reserve(file.tellg());
      file.seekg(0, std::ios::beg);

      buffer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

      return {};
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
