// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.api.std.variant;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   template<class... T>
   struct meta<std::variant<T...>>
   {
      static constexpr std::string_view name = []<std::size_t... I>(std::index_sequence<I...>) {
         return join_v<chars<"std::variant<">,
                       ((I != sizeof...(T) - 1) ? join_v<name_v<T>, chars<",">> : join_v<name_v<T>>)..., chars<">">>;
      }(std::make_index_sequence<sizeof...(T)>{});
   };

   template<>
   struct meta<std::monostate>
   {
      static constexpr std::string_view name = "std::monostate";
   };
}
