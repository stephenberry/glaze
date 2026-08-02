// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/unordered_map.hpp"
// glz:header std=<string_view>
// glz:header std=<unordered_map>
export module glaze.api.std.unordered_map;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class Key, class Mapped>
   struct meta<std::unordered_map<Key, Mapped>>
   {
      static constexpr std::string_view name =
         join_v<chars<"std::unordered_map<">, name_v<Key>, chars<",">, name_v<Mapped>, chars<">">>;
   };
}
