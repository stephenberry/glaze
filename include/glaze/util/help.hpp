// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>

#include "glaze/core/meta.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   template <class T, string_literal HelpMessage>
   struct help
   {
      static constexpr auto glaze_help = true;
      static constexpr sv help_message = HelpMessage.sv();
      using value_type = T;
      T value{};
      
      constexpr operator T&() noexcept {
         return value;
      }
      
      constexpr operator const T&() const noexcept {
         return value;
      }
   };
   
   template <class T>
   concept is_help = requires {
      T::glaze_help;
      requires(T::glaze_help == true);
   };
   
   template <is_help T>
   struct meta<T> {
      static constexpr sv name = join_v<chars<"glz::help<">, name_v<typename T::value_type>, chars<",\"">, T::help_message, chars<"\">">>;
      static constexpr auto value{&T::value};
   };
}
