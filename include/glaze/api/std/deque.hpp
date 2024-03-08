// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <deque>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   struct meta<std::deque<T>>
   {
      static constexpr std::string_view name = detail::join_v<chars<"std::deque<">, name_v<T>, chars<">">>;
   };
}
