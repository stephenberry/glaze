// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

namespace glz
{
   // Test that two meta objects are equal, with epsilon support for floating point values
   struct approx_equal_to final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = glz::tuple_size_v<meta_t<T>>;

         bool equal = true;
         for_each_short_circuit<N>([&](auto I) {
            auto& l = detail::get_member(lhs, get<1>(get<I>(meta_v<T>)));
            auto& r = detail::get_member(rhs, get<1>(get<I>(meta_v<T>)));
            using V = std::decay_t<decltype(l)>;
            if constexpr (std::floating_point<V> && requires { meta<std::decay_t<T>>::compare_epsilon; }) {
               if (std::abs(l - r) >= meta<std::decay_t<T>>::compare_epsilon) {
                  equal = false;
                  return true; // exit
               }
               return false; // continue
            }
            else {
               if (l != r) {
                  equal = false;
                  return true; // exit
               }
               return false; // continue
            }
         });

         return equal;
      }
   };
}
