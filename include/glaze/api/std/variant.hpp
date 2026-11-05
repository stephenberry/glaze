// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <variant>

#include "glaze/core/meta.hpp"

namespace glz
{
   template <class... T>
   struct meta<std::variant<T...>>
   {
      static constexpr std::string_view name =
         join_v<chars<"std::variant<">, comma_separated_names_v<T...>, chars<">">>;
   };

   template <>
   struct meta<std::monostate>
   {
      static constexpr std::string_view name = "std::monostate";
   };
}
