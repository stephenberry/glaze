// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <functional>

#include "glaze/core/common.hpp"
#include "glaze/core/reflect.hpp"

namespace glz
{
   struct equal_to final
   {
      template <glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         if constexpr (std::equality_comparable<T>) {
            return lhs == rhs;
         }
         else {
            constexpr auto N = reflect<T>::size;

            bool equal = true;
            for_each_short_circuit<N>([&]<auto I>() {
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

   namespace detail
   {
      // Lexicographic: the first member that differs decides, and later members are not looked at.
      template <class Compare, class T>
      constexpr bool lexicographic(const T& lhs, const T& rhs) noexcept
      {
         constexpr auto N = reflect<T>::size;

         bool result = false;
         for_each_short_circuit<N>([&]<auto I>() {
            auto& l = get_member(lhs, get<I>(reflect<T>::values));
            auto& r = get_member(rhs, get<I>(reflect<T>::values));
            if (Compare{}(l, r)) {
               result = true;
               return true; // exit
            }
            if (Compare{}(r, l)) {
               return true; // exit
            }
            return false; // continue
         });

         return result;
      }
   }

   struct less final
   {
      template <glaze_object_t T>
      constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
      {
         return detail::lexicographic<std::less<>>(lhs, rhs);
      }
   };

   struct less_equal final
   {
      template <glaze_object_t T>
      constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
      {
         return !detail::lexicographic<std::greater<>>(lhs, rhs);
      }
   };

   struct greater final
   {
      template <glaze_object_t T>
      constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
      {
         return detail::lexicographic<std::greater<>>(lhs, rhs);
      }
   };

   struct greater_equal final
   {
      template <glaze_object_t T>
      constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
      {
         return !detail::lexicographic<std::less<>>(lhs, rhs);
      }
   };
}
