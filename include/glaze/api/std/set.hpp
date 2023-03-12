// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <set>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept set = is_specialization_v<T, std::set>;

   template <set T>
   struct meta<T>
   {
      using V = typename T::value_type;
      static constexpr std::string_view name = detail::join_v<chars<"std::set<">, name_v<V>, chars<">">>;
   };
}
