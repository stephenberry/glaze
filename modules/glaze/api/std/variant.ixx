// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/variant.hpp"
// glz:header std=<cstddef>
// glz:header std=<string_view>
// glz:header std=<utility>
// glz:header std=<variant>
export module glaze.api.std.variant;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

using std::size_t;

export namespace glz
{
   template<class... T>
   struct meta<std::variant<T...>>
   {
      static constexpr std::string_view name =
         join_v<chars<"std::variant<">, comma_separated_names_v<T...>, chars<">">>;
   };

   template<>
   struct meta<std::monostate>
   {
      static constexpr std::string_view name = "std::monostate";
   };
}
