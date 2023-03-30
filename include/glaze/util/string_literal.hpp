// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/string_view.hpp"

namespace glz
{
   template <size_t N>
   struct string_literal
   {
      static constexpr size_t size = (N > 0) ? (N - 1) : 0;

      constexpr string_literal() = default;

      constexpr string_literal(const char (&str)[N]) { std::copy_n(str, N, value); }

      char value[N];
      constexpr const char* end() const noexcept { return value + size; }

      constexpr const std::string_view sv() const noexcept { return {value, size}; }
   };

   template <size_t N>
   constexpr auto string_literal_from_view(sv str)
   {
      string_literal<N + 1> sl{};
      std::copy_n(str.data(), str.size(), sl.value);
      *(sl.value + N) = '\0';
      return sl;
   }

   template <size_t N>
   constexpr size_t length(const char (&)[N]) noexcept
   {
      return N;
   }

   template <string_literal Str>
   struct chars_impl
   {
      static constexpr std::string_view value{Str.value, length(Str.value) - 1};
   };

   template <string_literal Str>
   inline constexpr std::string_view chars = chars_impl<Str>::value;

   template <const std::string_view& Str>
   struct stringer_impl
   {
      static constexpr std::string_view value = Str;
   };

   template <const std::string_view& Str>
   inline constexpr std::string_view stringer = stringer_impl<Str>::value;
}
