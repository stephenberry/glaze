// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../../Export.hpp"
#include <deque>
#ifdef CPP_MODULES
export module glaze.api.std.deque;
import glaze.core.meta;
#else
#include "glaze/core/meta.cppm"
#endif





namespace glz
{
   template <class T>
   struct meta<std::deque<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::deque<">, name_v<T>, chars<">">>;
   };
}
