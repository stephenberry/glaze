// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <filesystem>

#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"

namespace glz
{
   [[nodiscard]] error_ctx directory_to_buffers(std::unordered_map<std::filesystem::path, std::string>& files, const sv directory_path) {
       for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
           if (entry.is_regular_file()) {
              if (auto ec = file_to_buffer(files[entry.path()], entry.path().string()); bool(ec)) {
                 return {ec};
              }
           }
       }
      return {};
   }
   
   template <opts Opts = opts{}, class T>
      requires detail::readable_map_t<std::decay_t<T>>
   [[nodiscard]] error_ctx read_directory(T&& value, const sv directory_path) {
      std::unordered_map<std::filesystem::path, std::string> files{};
      if (auto ec = directory_to_buffers(files, directory_path); bool(ec)) {
         return {ec};
      }
      
      for (auto&[path, content] : files) {
         if (auto ec = read<Opts>(value[path], content)) {
            return ec;
         }
      }
      
      return {};
   }
}
