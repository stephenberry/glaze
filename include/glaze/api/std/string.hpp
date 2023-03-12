// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>

#include "glaze/api/name.hpp"

namespace glz
{
   template <>
   struct meta<std::string>
   {
      static constexpr std::string_view name = "std::string";
   };
}
