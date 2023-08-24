// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <string_view>

namespace glz
{
   template <class CharType, unsigned N>
   struct [[nodiscard]] basic_fixed_string
   {
      using char_type = CharType;

      char_type data_[N + 1]{};

      constexpr basic_fixed_string() noexcept : data_{} {}

      template <class other_char_type>
         requires std::same_as<other_char_type, char_type>
      constexpr basic_fixed_string(const other_char_type (&foo)[N + 1]) noexcept
      {
         std::copy_n(foo, N + 1, data_);
      }

      [[nodiscard]] constexpr std::basic_string_view<char_type> view() const noexcept { return {&data_[0], N}; }

      constexpr operator std::basic_string_view<char_type>() const noexcept { return {&data_[0], N}; }

      template <unsigned M>
      constexpr auto operator==(const basic_fixed_string<char_type, M>& r) const noexcept
      {
         return N == M && view() == r.view();
      }
   };

   template <class char_type, unsigned N>
   basic_fixed_string(char_type const (&str)[N]) -> basic_fixed_string<char_type, N - 1>;
}
