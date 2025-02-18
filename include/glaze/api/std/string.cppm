// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../../Export.hpp"
#include <string>
#include <string_view>
#ifdef CPP_MODULES
export module glaze.api.std.string;
import glaze.core.meta;
#else
#include "glaze/core/meta.cppm"
#endif





namespace glz
{
   template <>
   struct meta<std::string>
   {
      static constexpr std::string_view name = "std::string";
   };
}
