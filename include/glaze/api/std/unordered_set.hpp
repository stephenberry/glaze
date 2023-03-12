// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <unordered_set>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept unordered_set = is_specialization_v<T, std::unordered_set>;

   template <unordered_set T>
   struct meta<T>
   {
      using V = typename T::value_type;
      static constexpr std::string_view name = detail::join_v<chars<"std::unordered_set<">, name_v<V>, chars<">">>;
   };
}
