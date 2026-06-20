// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace glz
{
   // Compile-time string for use as a non-type template parameter.
   // Captures a string literal (including its null terminator) so that format
   // specifications can be carried through the type system, e.g.
   // glz::float_format<&T::x, "{:.2f}">.
   template <size_t N>
   struct format_str
   {
      char data[N]{};

      consteval format_str(const char (&str)[N]) noexcept { std::copy_n(str, N, data); }

      constexpr operator std::string_view() const noexcept { return {data, N - 1}; }
   };
}
