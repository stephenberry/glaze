// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>

namespace glz
{
   // Runtime context for configuration
   // We do not template the context on iterators so that it can be easily shared across buffer implementations
   struct context final
   {
      // USER CONFIGURABLE
      char indentation_char = ' ';
      uint8_t indentation_width = 3;
      std::string current_file; // top level file path
      
      // INTERNAL USE
      uint32_t indentation_level{};
   };
   
   template <class T>
   concept is_context = std::same_as<std::decay_t<T>, context>;
}
