// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

namespace glz
{
   // Test that two meta objects are equal, with epsilon support for floating point values
   struct approx_equal_to final
   {
      template <class T> requires (detail::glaze_object_t<T> || detail::reflectable<T>)
      constexpr bool operator()(T&& lhs, T&& rhs) noexcept
      {
         constexpr auto N = [] {
            if constexpr (detail::reflectable<T>) {
               return detail::count_members<T>;
            }
            else {
               return std::tuple_size_v<meta_t<T>>;
            }
         }();
         
         [[maybe_unused]] decltype(auto) t_lhs = [&] {
            if constexpr (detail::reflectable<T>) {
               if constexpr (std::is_const_v<std::remove_reference_t<decltype(lhs)>>) {
                  static constinit auto tuple_of_ptrs = make_const_tuple_from_struct<T>();
                  populate_tuple_ptr(lhs, tuple_of_ptrs);
                  return tuple_of_ptrs;
               }
               else {
                  static constinit auto tuple_of_ptrs = make_tuple_from_struct<T>();
                  populate_tuple_ptr(lhs, tuple_of_ptrs);
                  return tuple_of_ptrs;
               }
            }
            else {
               return nullptr;
            }
         }();
         
         [[maybe_unused]] decltype(auto) t_rhs = [&] {
            if constexpr (detail::reflectable<T>) {
               if constexpr (std::is_const_v<std::remove_reference_t<decltype(rhs)>>) {
                  static constinit auto tuple_of_ptrs = make_const_tuple_from_struct<T>();
                  populate_tuple_ptr(rhs, tuple_of_ptrs);
                  return tuple_of_ptrs;
               }
               else {
                  static constinit auto tuple_of_ptrs = make_tuple_from_struct<T>();
                  populate_tuple_ptr(rhs, tuple_of_ptrs);
                  return tuple_of_ptrs;
               }
            }
            else {
               return nullptr;
            }
         }();
         
         using V = std::decay_t<T>;

         bool equal = true;
         for_each<N>([&](auto I) {
            using Element = detail::glaze_tuple_element<I, N, T>;
            constexpr size_t member_index = Element::member_index;
            
            decltype(auto) lhs_member = [&] {
               if constexpr (detail::reflectable<T>) {
                  return std::get<I>(t_lhs);
               }
               else {
                  return get<member_index>(get<I>(meta_v<V>));
               }
            }();
            
            decltype(auto) rhs_member = [&] {
               if constexpr (detail::reflectable<T>) {
                  return std::get<I>(t_rhs);
               }
               else {
                  return get<member_index>(get<I>(meta_v<V>));
               }
            }();
            
            auto& l = detail::get_member(lhs, lhs_member);
            auto& r = detail::get_member(rhs, rhs_member);
            using V = std::decay_t<decltype(l)>;
            if constexpr (std::floating_point<V> && requires { meta<std::decay_t<T>>::compare_epsilon; }) {
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
   };
}
