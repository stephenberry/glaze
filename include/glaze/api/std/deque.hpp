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
   struct meta<T>
   {
      using V = typename T::value_type;
      static constexpr std::string_view name = detail::join_v<chars<"std::deque<">, name_v<V>, chars<">">>;
   };
}
