// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <optional>

#include "glaze/core/meta.hpp"

namespace glz
{
   template <class T>
   struct name_meta<std::optional<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::optional<">, name_v<T>, chars<">">>;
   };
}
