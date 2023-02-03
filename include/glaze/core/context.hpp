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
      // INTERNAL USE
      uint32_t indentation_level{};
      std::string current_file;  // top level file path
   };
   
   enum class error_code : uint32_t
   {
      none,
      no_read_input,
      data_must_be_null_terminated,
      parse_number_failure,
      expected_brace,
      expected_bracket,
      exceeded_static_array_size
   };
   
   struct noexcept_context final
   {
      // INTERNAL USE
      uint32_t indentation_level{};
      std::string current_file;  // top level file path
      error_code error{};
   };
   
   template <class T>
   concept is_context = std::same_as<std::decay_t<T>, context> || std::same_as<std::decay_t<T>, noexcept_context>;
}
