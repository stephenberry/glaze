// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.util.help;

import std;

import glaze.util.string_literal;
import glaze.util.type_traits;

import glaze.core.meta;

export namespace glz
{
   template <class T>
   concept is_help = requires { requires(T::glaze_help == true); };

   template <class T, string_literal HelpMessage>
   struct help
   {
      static constexpr auto glaze_help = true;
      static constexpr sv help_message = HelpMessage.sv();
      using value_type = T;
      T value{};

      constexpr operator T&() noexcept { return value; }

      constexpr operator const T&() const noexcept { return value; }
   };

   template <class T, string_literal HelpMessage>
   struct meta<help<T, HelpMessage>>
   {
      using V = help<T, HelpMessage>;
      static constexpr sv name =
         join_v<chars<"glz::help<">, name_v<typename V::value_type>, chars<",\"">, V::help_message, chars<"\">">>;
      static constexpr auto value{&V::value};
   };
}
