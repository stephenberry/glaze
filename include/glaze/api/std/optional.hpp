// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <optional>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   concept optional = is_specialization_v<T, std::optional>;

   template <optional T>
   struct meta<T>
   {
      using V = typename T::value_type;
      static constexpr std::string_view name = detail::join_v<chars<"std::optional<">, name_v<V>, chars<">">>;
   };
}
