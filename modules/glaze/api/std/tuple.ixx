// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/tuple.hpp"
// glz:header std=<cstddef>
// glz:header std=<string_view>
// glz:header std=<tuple>
// glz:header std=<utility>
export module glaze.api.std.tuple;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

using std::size_t;

export namespace glz
{
   template<class... T>
   struct meta<std::tuple<T...>>
   {
      static constexpr std::string_view name = join_v<chars<"std::tuple<">, comma_separated_names_v<T...>, chars<">">>;
   };
}
