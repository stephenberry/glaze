// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm> // for std::copy_n
#include <string_view>

namespace glz
{
   using sv = std::string_view;

   template <size_t N>
   struct string_literal
   {
      static constexpr size_t length = (N > 0) ? (N - 1) : 0;

      constexpr size_t size() const noexcept { return length; }

      constexpr string_literal() noexcept = default;

      constexpr string_literal(const char (&str)[N]) noexcept { std::copy_n(str, N, value); }

      char value[N];
      constexpr const char* begin() const noexcept { return value; }
      constexpr const char* end() const noexcept { return value + length; }

      [[nodiscard]] constexpr auto operator<=>(const string_literal&) const = default;

      constexpr const std::string_view sv() const noexcept { return {value, length}; }

      constexpr operator std::string_view() const noexcept { return {value, length}; }
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

   template <size_t N>
   struct fixed_string final
   {
      constexpr explicit(true) fixed_string(const auto... cs) : data{cs...} {}
      constexpr explicit(false) fixed_string(const char (&str)[N + 1]) { std::copy_n(str, N + 1, data.data()); }
      [[nodiscard]] constexpr auto operator<=>(const fixed_string&) const = default;
      [[nodiscard]] constexpr explicit(false) operator std::string_view() const { return {data.data(), N}; }
      [[nodiscard]] constexpr auto size() const -> std::size_t { return N; }
      std::array<char, N + 1> data{};
   };

   template <size_t N>
   fixed_string(const char (&str)[N]) -> fixed_string<N - 1>;

   namespace detail
   {
      template <std::array V>
      struct make_static
      {
         static constexpr auto value = V;
      };

      template <const std::string_view&... Strs>
      inline constexpr std::string_view join()
      {
         constexpr auto joined_arr = []() {
            constexpr size_t len = (Strs.size() + ... + 0);
            std::array<char, len + 1> arr{};
            auto append = [i = 0, &arr](const auto& s) mutable {
               for (auto c : s) arr[i++] = c;
            };
            (append(Strs), ...);
            arr[len] = 0;
            return arr;
         }();
         auto& static_arr = make_static<joined_arr>::value;
         return {static_arr.data(), static_arr.size() - 1};
      }
   }

   // Helper to get the value out
   template <const std::string_view&... Strs>
   constexpr auto join_v = detail::join<Strs...>();
}
