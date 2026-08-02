// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/api/std/string.hpp"
// glz:header std=<string>
// glz:header std=<string_view>
export module glaze.api.std.string;

import std;

import glaze.core.meta;

export namespace glz
{
   template<>
   struct meta<std::string>
   {
      static constexpr std::string_view name = "std::string";
   };
}
