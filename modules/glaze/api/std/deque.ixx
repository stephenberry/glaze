// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/deque.hpp"
// glz:header std=<deque>
// glz:header std=<string_view>
export module glaze.api.std.deque;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class T>
   struct meta<std::deque<T>>
   {
      static constexpr std::string_view name = join_v<chars<"std::deque<">, name_v<T>, chars<">">>;
   };
}
