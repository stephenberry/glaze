// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <memory>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept shared_ptr = is_specialization_v<T, std::shared_ptr>;

   template <shared_ptr T>
   struct meta<T>
   {
      using V = typename T::element_type;
      static constexpr std::string_view name = detail::join_v<chars<"std::shared_ptr<">, name_v<V>, chars<">">>;
   };
}
