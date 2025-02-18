// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../../Export.hpp"
#include <unordered_map>
#ifdef CPP_MODULES
export module glaze.api.std.unordered_map;
import glaze.core.meta;
#else
#include "glaze/core/meta.cppm"
#endif





namespace glz
{
   template <class Key, class Mapped>
   struct meta<std::unordered_map<Key, Mapped>>
   {
      static constexpr std::string_view name =
         join_v<chars<"std::unordered_map<">, name_v<Key>, chars<",">, name_v<Mapped>, chars<">">>;
   };
}
