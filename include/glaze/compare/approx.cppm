// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#ifdef CPP_MODULES
export module glaze.compare.approx;
import glaze.core.common;
import glaze.core.reflect;
#else
#include "glaze/core/common.cppm"
#include "glaze/core/reflect.cppm"
#endif




namespace glz
{
   // Test that two meta objects are equal, with epsilon support for floating point values
   struct approx_equal_to final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = reflect<T>::size;

         bool equal = true;
         for_each_short_circuit<N>([&](auto I) {
            auto& l = get_member(lhs, get<I>(reflect<T>::values));
            auto& r = get_member(rhs, get<I>(reflect<T>::values));
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
