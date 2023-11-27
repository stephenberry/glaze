// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <source_location>
#include <string_view>

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
      static_assert(false, "MSVC does not support member variable name reflection");
      return {};
#else
      std::string_view str = std::source_location::current().function_name();

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
