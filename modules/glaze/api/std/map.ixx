// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/map.hpp"
// glz:header std=<map>
// glz:header std=<string_view>
export module glaze.api.std.map;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class Key, class Mapped>
   struct meta<std::map<Key, Mapped>>
   {
      static constexpr std::string_view name =
         join_v<chars<"std::map<">, name_v<Key>, chars<",">, name_v<Mapped>, chars<">">>;
   };
}
