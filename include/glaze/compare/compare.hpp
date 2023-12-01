// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <functional>

#include "glaze/core/common.hpp"

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
            constexpr auto N = std::tuple_size_v<meta_t<T>>;

            bool equal = true;
            for_each<N>([&](auto I) {
               auto& l = detail::get_member(lhs, get<1>(get<I>(meta_v<T>)));
               auto& r = detail::get_member(rhs, get<1>(get<I>(meta_v<T>)));
               if (!std::equal_to{}(l, r)) {
                  equal = false;
               }
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
         constexpr auto N = std::tuple_size_v<meta_t<T>>;

         bool less_than = true;
         for_each<N>([&](auto I) {
            auto& l = detail::get_member(lhs, get<1>(get<I>(meta_v<T>)));
            auto& r = detail::get_member(rhs, get<1>(get<I>(meta_v<T>)));
            if (!std::less{}(l, r)) {
               less_than = false;
            }
         });

         return less_than;
      }
   };

   struct less_equal final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = std::tuple_size_v<meta_t<T>>;

         bool less_than = true;
         for_each<N>([&](auto I) {
            auto& l = detail::get_member(lhs, get<1>(get<I>(meta_v<T>)));
            auto& r = detail::get_member(rhs, get<1>(get<I>(meta_v<T>)));
            if (!std::less_equal{}(l, r)) {
               less_than = false;
            }
         });

         return less_than;
      }
   };

   struct greater final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = std::tuple_size_v<meta_t<T>>;

         bool greater_than = true;
         for_each<N>([&](auto I) {
            auto& l = detail::get_member(lhs, get<1>(get<I>(meta_v<T>)));
            auto& r = detail::get_member(rhs, get<1>(get<I>(meta_v<T>)));
            if (!std::greater{}(l, r)) {
               greater_than = false;
            }
         });

         return greater_than;
      }
   };

   struct greater_equal final
   {
      template <detail::glaze_object_t T>
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = std::tuple_size_v<meta_t<T>>;

         bool greater_than = true;
         for_each<N>([&](auto I) {
            auto& l = detail::get_member(lhs, get<1>(get<I>(meta_v<T>)));
            auto& r = detail::get_member(rhs, get<1>(get<I>(meta_v<T>)));
            if (!std::greater_equal{}(l, r)) {
               greater_than = false;
            }
         });

         return greater_than;
      }
   };
}
