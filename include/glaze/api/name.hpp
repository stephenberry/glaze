// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <string_view>

#include "glaze/util/type_traits.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/util/for_each.hpp"

namespace glz
{
   namespace detail
   {
      inline constexpr auto total_length(const auto& strs) noexcept {
         size_t n{};
         for_each<std::tuple_size_v<std::decay_t<decltype(strs)>>>([&](auto I) { n += std::get<I>(strs).size(); });
         return n;
      }

      template <const std::string_view&... Strs>
      struct join
      {
         // Join all strings into a single std::array of chars
         static constexpr auto impl() noexcept
         {
            // This local copy to a tuple and avoiding of parameter pack expansion is needed to avoid MSVC internal compiler errors
            constexpr auto strs_tuple = std::make_tuple(Strs...);
            //constexpr size_t len = (Strs.size() + ... + 0);
            constexpr size_t len = total_length(strs_tuple);
            std::array<char, len + 1> arr{};
            auto append = [i = 0, &arr](const auto& s) mutable {
               for (auto c : s) arr[i++] = c;
            };

            for_each<sizeof...(Strs)>([&](auto I) { append(std::get<I>(strs_tuple));
            });

            //(append(Strs), ...);
            arr[len] = 0;
            return arr;
         }
         
         static constexpr auto arr = impl(); // Give the joined string static storage
         static constexpr std::string_view value {arr.data(), arr.size() - 1};
      };
      // Helper to get the value out
      template <const std::string_view&... Strs>
      static constexpr auto join_v = join<Strs...>::value;
   }
   
   /*template <class T>
   concept has_glaze_name = requires {
       T::glaze_name;
   };*/
   
   template <size_t N>
   struct string_literal {
      consteval string_literal(const char (&str)[N]) {
         std::copy_n(str, N, value);
      }
      
      char value[N];
   };
   
   template <size_t N>
   constexpr size_t length(char const (&)[N]) {
      return N;
   }
   
   template <string_literal Str>
   struct chars_impl {
      static constexpr std::string_view value{ Str.value, length(Str.value) - 1 };
   };
   
   template <string_literal Str>
   inline constexpr std::string_view chars = chars_impl<Str>::value;
   
   template <const std::string_view& Str>
   struct stringer_impl {
      static constexpr std::string_view value = Str;
   };
   
   template <const std::string_view& Str>
   inline constexpr std::string_view stringer = stringer_impl<Str>::value;
}
