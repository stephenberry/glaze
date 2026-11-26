// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/unordered_set.hpp"
// glz:header std=<string_view>
// glz:header std=<unordered_set>
export module glaze.api.std.unordered_set;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class T>
   struct meta<std::unordered_set<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::unordered_set<">, name_v<T>, chars<">">>;
   };
}
