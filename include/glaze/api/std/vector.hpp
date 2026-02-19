// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(GLAZE_CXX_MODULE)
#define GLAZE_EXPORT export
#else
#define GLAZE_EXPORT
#include <vector>
#endif

#include "glaze/core/meta.hpp"

GLAZE_EXPORT namespace glz
{
   template <class T>
   struct meta<std::vector<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::vector<">, name_v<T>, chars<">">>;
   };
}
