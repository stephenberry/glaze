// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <vector>

#include "glaze/api/name.hpp"

namespace glaze
{
   template <class T>
   concept vector = detail::is_specialization_v<T, std::vector>;
   
   template <vector T>
   struct name_t<T> {
      using V = typename T::value_type;
      static constexpr std::string_view value = detail::join_v<chars<"std::vector<">,
      name<V>,
      chars<">">>;
   };
}
