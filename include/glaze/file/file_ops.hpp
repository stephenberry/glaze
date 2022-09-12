// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <fstream>  // for ifstream, basic_istream...
#include <string>
#include <filesystem>

namespace glz
{
   template <class T>
   void file_to_buffer(T &buffer, const std::string &file_name)
   {
      std::ifstream file(file_name);

      if (!file) {
         throw std::runtime_error("glaze::file_to_buffer: File with path (" +
                                  file_name +
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
   T file_to_buffer(const std::string &file_name)
   {
      T buffer{};
      file_to_buffer(buffer, file_name);
      return buffer;
   }

   inline std::filesystem::path relativize_if_not_absolute(
      std::filesystem::path const &working_directory,
      std::filesystem::path const &filepath)
   {
      if (filepath.is_absolute()) {
         return filepath;
      }

      return working_directory / filepath;
   }
}  // namespace glaze
