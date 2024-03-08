// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <memory>

#include "glaze/api/name.hpp"

namespace glz
{
   template <class T>
   struct meta<std::shared_ptr<T>>
   {
      static constexpr std::string_view name = detail::join_v<chars<"std::shared_ptr<">, name_v<T>, chars<">">>;
   };
}
