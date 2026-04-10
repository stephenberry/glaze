// Glaze Library
// For the license information refer to glaze.ixx
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
