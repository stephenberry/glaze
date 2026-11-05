// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/meta.hpp"
#include "glaze/core/tuple.hpp"

namespace glz
{
   template <class... T>
   struct meta<glz::tuple<T...>>
   {
      static constexpr std::string_view name = join_v<chars<"glz::tuple<">, comma_separated_names_v<T...>, chars<">">>;
   };
}
