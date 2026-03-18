// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.api.std.array;

import std;

import glaze.core.meta;
import glaze.util.string_literal;

export namespace glz
{
   namespace detail
   {
      template <unsigned... digits>
      struct to_chars
      {
         static constexpr char value[] = {('0' + digits)..., 0};
      };

      template <unsigned rem, unsigned... digits>
      struct explode : explode<rem / 10, rem % 10, digits...>
      {};

      template <unsigned... digits>
      struct explode<0, digits...> : to_chars<digits...>
      {};
   }

   template <unsigned num>
   struct num_to_string : detail::explode<num>
   {};

   template <class T, std::size_t N>
   struct meta<std::array<T, N>>
   {
      static constexpr std::string_view name =
         join_v<chars<"std::array<">, name_v<T>, chars<",">, chars<num_to_string<N>::value>, chars<">">>;
   };
}
