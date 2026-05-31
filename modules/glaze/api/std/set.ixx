// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/set.hpp"
// glz:header std=<string_view>
export module glaze.api.std.set;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

namespace glz
{
   template<class T>
   struct meta<std::set<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::set<">, name_v<T>, chars<">">>;
   };
}
