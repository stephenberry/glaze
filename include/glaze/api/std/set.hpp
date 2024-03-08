// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <set>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   struct meta<std::set<T>>
   {
      static constexpr std::string_view name = detail::join_v<chars<"std::set<">, name_v<T>, chars<">">>;
   };
}
