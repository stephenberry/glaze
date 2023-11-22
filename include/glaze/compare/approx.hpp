// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

namespace glz
{
   // Test that two meta objects are equal, with epsilon support for floating point values
   template <detail::glaze_object_t T>
   inline constexpr bool approx_equal(T&& lhs, T&& rhs) noexcept {
      constexpr auto N = std::tuple_size_v<meta_t<T>>;
      
      bool equal = true;
      for_each<N>([&](auto I) {
         auto& l = detail::get_member(lhs, tuplet::get<1>(tuplet::get<I>(meta_v<T>)));
         auto& r = detail::get_member(rhs, tuplet::get<1>(tuplet::get<I>(meta_v<T>)));
         using V = std::decay_t<decltype(l)>;
         if constexpr (std::floating_point<V> && requires { meta<std::decay_t<T>>::compare_epsilon; })
         {
            if (std::abs(l - r) >= meta<std::decay_t<T>>::compare_epsilon) {
               equal = false;
            }
         }
         else {
            if (l != r) {
               equal = false;
            }
         }
      });
      
      return equal;
   }
}
