// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/write.hpp"

namespace glz
{
   template <class CharT, class... Args>
   struct basic_format_string;
   
   template< class... Args >
   using format_string = basic_format_string<char, std::type_identity_t<Args>...>;
   
   template <class... Args>
   std::string format(glz::format_string<Args...> fmt, Args&&... args) {
      
   }
}
