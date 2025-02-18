// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../../Export.hpp"
#include <set>
#ifdef CPP_MODULES
export module glaze.api.std.set;
import glaze.core.meta;
#else
#include "glaze/core/meta.cppm"
#endif





namespace glz
{
   template <class T>
   struct meta<std::set<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::set<">, name_v<T>, chars<">">>;
   };
}
