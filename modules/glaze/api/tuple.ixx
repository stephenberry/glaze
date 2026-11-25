// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/tuple.hpp"
// glz:header std=<string_view>
export module glaze.api.tuple;

import std;
import glaze.core.meta;
import glaze.util.string_literal;
import glaze.core.tuple;


export namespace glz
{
   template <class... T>
   struct meta<glz::tuple<T...>>
   {
      static constexpr std::string_view name = join_v<chars<"glz::tuple<">, comma_separated_names_v<T...>, chars<">">>;
   };
}
