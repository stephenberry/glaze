// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <memory>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   struct meta<std::unique_ptr<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::unique_ptr<">, name_v<T>, chars<">">>;
   };
}
