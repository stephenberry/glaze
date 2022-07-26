// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <map>

#include "glaze/name.hpp"

namespace glaze
{
   template <class T>
   concept map = is_specialization_v<T, std::map>;
   
   template <map T>
   struct name_t<T> {
      using Key = typename T::key_type;
      using V = typename T::mapped_type;
      static constexpr std::string_view value = detail::join_v<chars<"std::map<">,
      name<Key>,
      chars<",">,
      name<V>,
      chars<">">>;
   };
}
