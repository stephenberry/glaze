// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(GLAZE_CXX_MODULE)
#define GLAZE_EXPORT export
#else
#define GLAZE_EXPORT
#include <unordered_set>
#endif

#include "glaze/core/meta.hpp"

GLAZE_EXPORT namespace glz
{
   template <class T>
   struct meta<std::unordered_set<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::unordered_set<">, name_v<T>, chars<">">>;
   };
}
