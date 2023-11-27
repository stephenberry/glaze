// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// TODO: Use std::source_location when deprecating clang 14
//#include <source_location>
#include <string_view>

#if defined(__clang__) || defined(__GNUC__)
#define GLZ_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define GLZ_PRETTY_FUNCTION __FUNCSIG__
#endif

namespace glz {
#if defined(__clang__)
   inline constexpr auto pretty_function_tail = "]";
#elif defined(__GNUC__) || defined(__GNUG__)
   inline constexpr auto pretty_function_tail = ";";
#elif defined(_MSC_VER)
#endif
   
   template <auto P> requires (std::is_member_object_pointer_v<decltype(P)>)
   constexpr std::string_view get_name() noexcept {
#if defined(_MSC_VER)
      static_assert(false_v<decltype(P)>, "MSVC does not support member variable name reflection");
      return {};
#else
      // TODO: Use std::source_location when deprecating clang 14
      //std::string_view str = std::source_location::current().function_name();
      std::string_view str = GLZ_PRETTY_FUNCTION;

      size_t i = str.find("&");
      str = str.substr(i + 2);
      i = str.find("::");
      str = str.substr(i + 2);
      i = str.find(pretty_function_tail);
      str = str.substr(0, i);

      return str;
#endif
   }
}
