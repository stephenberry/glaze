#pragma once

#include <format>
#include <fstream>
#include <string>

namespace bencher
{
   inline bool save_file(const std::string& contents, const std::string& path)
   {
      std::ofstream file(path.data());
      if (not file) {
         return false;
      }
      file.write(contents.data(), static_cast<int64_t>(contents.size()));
      return true;
   }
}
