// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <variant>

namespace glaze
{
   template <class... T>
   auto to_variant_pointer(std::variant<T...>&&)
   {
      return std::variant<T*...>{};
   }
}
