// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <fstream>
#include <string>
#include <filesystem>

#include "glaze/core/context.hpp"

namespace glz
{
   template <class T>
   void file_to_buffer(T& buffer, const std::string_view file_name)
   {
      std::ifstream file{ std::string(file_name) };

      if (!file) {
         throw std::runtime_error("glaze::file_to_buffer: File with path (" +
                                  std::string(file_name) +
                                  ") could not be loaded. Ensure that file "
                                  "exists at the given path.");
      }

      file.seekg(0, std::ios::end);
      buffer.reserve(file.tellg());
      file.seekg(0, std::ios::beg);

      buffer.assign((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
   }

   template <class T>
   std::string file_to_buffer(T&& file_name)
   {
      std::string buffer{};
      file_to_buffer(buffer, std::forward<T>(file_name));
      return buffer;
   }

   inline std::filesystem::path relativize_if_not_absolute(
      const std::filesystem::path& working_directory,
      const std::filesystem::path& filepath)
   {
      if (filepath.is_absolute()) {
         return filepath;
      }

      return working_directory / filepath;
   }
}
