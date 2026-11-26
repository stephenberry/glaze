// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/vector.hpp"
// glz:header std=<string_view>
// glz:header std=<vector>
export module glaze.api.std.vector;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class T>
   struct meta<std::vector<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::vector<">, name_v<T>, chars<">">>;
   };
}
