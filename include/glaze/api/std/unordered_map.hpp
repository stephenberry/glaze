// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <unordered_map>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept unordered_map = is_specialization_v<T, std::unordered_map>;

   template <unordered_map T>
   struct meta<T>
   {
      using Key = typename T::key_type;
      using V = typename T::mapped_type;
      static constexpr std::string_view name =
         detail::join_v<chars<"std::unordered_map<">, name_v<Key>, chars<",">, name_v<V>, chars<">">>;
   };
}
