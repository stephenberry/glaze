// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>

#include "glaze/api/name.hpp"

namespace glaze
{
   template <>
   struct name_t<std::string> {
      static constexpr std::string_view value = "std::string";
   };
}
