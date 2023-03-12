// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <vector>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept vector = is_specialization_v<T, std::vector>;

   template <vector T>
   struct meta<T>
   {
      using V = typename T::value_type;
      static constexpr std::string_view name = detail::join_v<chars<"std::vector<">, name_v<V>, chars<">">>;
   };
}
