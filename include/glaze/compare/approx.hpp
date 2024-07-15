// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/core/refl.hpp"

namespace glz
{
   // Test that two meta objects are equal, with epsilon support for floating point values
   struct approx_equal_to final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = refl<T>.N;

         bool equal = true;
         for_each_short_circuit<N>([&](auto I) {
            auto& l = detail::get_member(lhs, get<I>(refl<T>.values));
            auto& r = detail::get_member(rhs, get<I>(refl<T>.values));
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
