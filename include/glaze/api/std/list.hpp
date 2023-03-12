// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <list>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept list = is_specialization_v<T, std::list>;

   template <list T>
   struct meta<T>
   {
      using V = typename T::value_type;
      static constexpr std::string_view name = detail::join_v<chars<"std::list<">, name_v<V>, chars<">">>;
   };
}
