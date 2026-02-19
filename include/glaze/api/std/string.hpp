// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(GLAZE_CXX_MODULE)
#define GLAZE_EXPORT export
#else
#define GLAZE_EXPORT
#include <string>
#include <string_view>
#endif

#include "glaze/core/meta.hpp"

GLAZE_EXPORT namespace glz
{
   template <>
   struct meta<std::string>
   {
      static constexpr std::string_view name = "std::string";
   };
}
