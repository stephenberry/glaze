// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/optional.hpp"
// glz:header std=<optional>
// glz:header std=<string_view>
export module glaze.api.std.optional;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class T>
   struct meta<std::optional<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::optional<">, name_v<T>, chars<">">>;
   };
}
