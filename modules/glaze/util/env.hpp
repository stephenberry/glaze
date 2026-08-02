// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdlib>
#include <optional>
#include <string>

namespace glz
{
   // Copy an environment variable into owned storage.
   // Returns std::nullopt if the variable is not present.
   inline std::optional<std::string> getenv_copy(const char* name)
   {
#if defined(_MSC_VER) && !defined(__clang__)
      char* value = nullptr;
      size_t len = 0;
      const int rc = _dupenv_s(&value, &len, name);
      if (rc == 0 && value) {
         std::string out{value};
         std::free(value);
         return out;
      }
      std::free(value);
      return std::nullopt;
#else
      if (const char* value = std::getenv(name)) {
         return std::string{value};
      }
      return std::nullopt;
#endif
   }

   // Like getenv_copy(), but treats empty values as "not set".
   inline std::optional<std::string> getenv_nonempty(const char* name)
   {
      if (auto value = getenv_copy(name); value && !value->empty()) {
         return value;
      }
      return std::nullopt;
   }
}
