// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.api.std.unique_ptr;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class T>
   struct meta<std::unique_ptr<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::unique_ptr<">, name_v<T>, chars<">">>;
   };
}
