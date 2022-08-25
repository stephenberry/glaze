// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <variant>

#include "glaze/util/type_traits.hpp"

namespace glaze
{
   template <class T>
   concept is_variant = is_specialization_v<T, std::variant>;
   
   template <class... T>
   auto to_variant_pointer(std::variant<T...>&&)
   {
      return std::variant<T*...>{};
   }
}
