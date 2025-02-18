// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#include <functional>
#ifdef CPP_MODULES
export module glaze.compare.compare;
import glaze.core.common;
import glaze.core.reflect;
#else
#include "glaze/core/common.cppm"
#include "glaze/core/reflect.cppm"
#endif





namespace glz
{
   struct equal_to final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         if constexpr (std::equality_comparable<T>) {
            return lhs == rhs;
         }
         else {
            constexpr auto N = reflect<T>::size;

            bool equal = true;
            for_each_short_circuit<N>([&](auto I) {
               auto& l = get_member(lhs, get<I>(reflect<T>::values));
               auto& r = get_member(rhs, get<I>(reflect<T>::values));
               if (!std::equal_to{}(l, r)) {
                  equal = false;
                  return true; // exit
               }
               return false; // continue
            });

            return equal;
         }
      }
   };

   struct less final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = reflect<T>::size;

         bool less_than = true;
         for_each_short_circuit<N>([&](auto I) {
            auto& l = get_member(lhs, get<I>(reflect<T>::values));
            auto& r = get_member(rhs, get<I>(reflect<T>::values));
            if (!std::less{}(l, r)) {
               less_than = false;
               return true; // exit
            }
            return false; // continue
         });

         return less_than;
      }
   };

   struct less_equal final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = reflect<T>::size;

         bool less_than = true;
         for_each_short_circuit<N>([&](auto I) {
            auto& l = get_member(lhs, get<I>(reflect<T>::values));
            auto& r = get_member(rhs, get<I>(reflect<T>::values));
            if (!std::less_equal{}(l, r)) {
               less_than = false;
               return true; // exit
            }
            return false; // continue
         });

         return less_than;
      }
   };

   struct greater final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = reflect<T>::size;

         bool greater_than = true;
         for_each_short_circuit<N>([&](auto I) {
            auto& l = get_member(lhs, get<I>(reflect<T>::values));
            auto& r = get_member(rhs, get<I>(reflect<T>::values));
            if (!std::greater{}(l, r)) {
               greater_than = false;
               return true; // exit
            }
            return false; // continue
         });

         return greater_than;
      }
   };

   struct greater_equal final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = reflect<T>::size;

         bool greater_than = true;
         for_each_short_circuit<N>([&](auto I) {
            auto& l = get_member(lhs, get<I>(reflect<T>::values));
            auto& r = get_member(rhs, get<I>(reflect<T>::values));
            if (!std::greater_equal{}(l, r)) {
               greater_than = false;
               return true; // exit
            }
            return false; // continue
         });

         return greater_than;
      }
   };
}
