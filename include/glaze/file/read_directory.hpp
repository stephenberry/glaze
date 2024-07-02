// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <filesystem>

#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"

namespace glz
{
   [[nodiscard]] inline error_ctx directory_to_buffers(std::unordered_map<std::filesystem::path, std::string>& files,
                                                       const sv directory_path, const sv target_extension = ".json")
   {
      for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
         if (entry.is_regular_file() && (target_extension.empty() || (entry.path().extension() == target_extension))) {
            if (auto ec = file_to_buffer(files[entry.path()], entry.path().string()); bool(ec)) {
               return {ec};
            }
         }
      }
      return {};
   }

   template <opts Opts = opts{}, detail::readable_map_t T>
   [[nodiscard]] error_ctx read_directory(T& value, const sv directory_path, const sv target_extension = ".json")
   {
      std::unordered_map<std::filesystem::path, std::string> files{};
      if (auto ec = directory_to_buffers(files, directory_path, target_extension)) {
         return ec;
      }

      for (auto& [path, content] : files) {
         if (auto ec = read<Opts>(value[path], content)) {
            return ec;
         }
      }

      return {};
   }
}
