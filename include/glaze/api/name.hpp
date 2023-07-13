// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <string_view>

#include "glaze/core/meta.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
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
      // Helper to get the value out
      template <const std::string_view&... Strs>
      constexpr auto join_v = join<Strs...>();
   }

   /*template <class T>
   concept has_glaze_name = requires {
       T::glaze_name;
   };*/
}
