// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <string_view>

namespace glz
{
   using sv = std::string_view;

   template <size_t N>
   struct string_literal
   {
      using value_type = char;
      using reference = value_type&;
      using const_reference = const value_type&;
      using pointer = value_type*;
      using const_pointer = const value_type*;
      using size_type = size_t;

      static constexpr size_t length = (N > 0) ? (N - 1) : 0;

      [[nodiscard]] constexpr size_t size() const noexcept { return length; }

      constexpr string_literal() noexcept = default;
      constexpr string_literal(const string_literal&) noexcept = default;
      constexpr string_literal(string_literal&&) noexcept = default;
      constexpr string_literal& operator=(const string_literal&) noexcept = default;
      constexpr string_literal& operator=(string_literal&&) noexcept = default;

      constexpr string_literal(const char (&str)[N]) noexcept
      {
         for (size_t i = 0; i < N; ++i) {
            value[i] = str[i];
         }
      }

      char value[N];
      constexpr const char* begin() const noexcept { return value; }
      constexpr const char* end() const noexcept { return value + length; }

      [[nodiscard]] constexpr auto operator<=>(const string_literal&) const = default;

      [[nodiscard]] constexpr const std::string_view sv() const noexcept { return {value, length}; }

      [[nodiscard]] constexpr operator std::string_view() const noexcept { return {value, length}; }

      constexpr reference operator[](size_type index) noexcept { return value[index]; }
      constexpr const_reference operator[](size_type index) const noexcept { return value[index]; }
   };

   template <size_t N>
   constexpr auto string_literal_from_view(sv str)
   {
      string_literal<N + 1> sl{};
      for (size_t i = 0; i < str.size(); ++i) {
         sl[i] = str[i];
      }
      *(sl.value + N) = '\0';
      return sl;
   }

   template <string_literal Str>
   inline constexpr std::string_view chars = Str.sv();

   template <string_literal Str>
   inline constexpr std::string_view root = Str.sv();

   namespace detail
   {
      template <const std::string_view&... Strs>
      inline constexpr std::string_view join()
      {
         // make a null terminated string_view
         static constexpr auto joined_arr = []() {
            constexpr size_t len = (Strs.size() + ... + 0);
            std::array<char, len + 1> arr;
            auto append = [i = 0, &arr](const auto& s) mutable {
               for (auto c : s) arr[i++] = c;
            };
            (append(Strs), ...);
            arr[len] = 0;
            return arr;
         }();
         return {joined_arr.data(), joined_arr.size() - 1};
      }
   }

   // Helper to get the value out
   template <const std::string_view&... Strs>
   inline constexpr auto join_v = detail::join<Strs...>();
}
