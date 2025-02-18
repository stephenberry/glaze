// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../../Export.hpp"
#include <unordered_set>
#ifdef CPP_MODULES
export module glaze.api.std.unordered_set;
import glaze.core.meta;
#else
#include "glaze/core/meta.cppm"
#endif





namespace glz
{
   template <class T>
   struct meta<std::unordered_set<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::unordered_set<">, name_v<T>, chars<">">>;
   };
}
