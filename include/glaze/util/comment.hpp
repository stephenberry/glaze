// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>

namespace glz
{
   struct comment final
   {
      std::string_view value{};

      constexpr comment(const char* str) noexcept : value(str) {}
      constexpr comment(const char* str, size_t n) noexcept : value(str, n) {}
   };

   constexpr comment operator""_c(const char* data, size_t n) noexcept { return {data, n}; }
}
