// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <deque>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept deque = is_specialization_v<T, std::deque>;
   
   template <deque T>
   struct name_t<T> {
      using V = typename T::value_type;
      static constexpr std::string_view value = detail::join_v<chars<"std::vector<">,
      name<V>,
      chars<">">>;
   };
}
