// Glaze Library
// For the license information refer to glaze.ixx
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
