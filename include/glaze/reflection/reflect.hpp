// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <initializer_list>

#include "glaze/core/common.hpp"
#include "glaze/reflection/get_name.hpp"
#include "glaze/reflection/to_tuple.hpp"
#include "glaze/core/refl.hpp"

namespace glz
{
   // Use a dummy struct for make_reflectable so that we don't conflict with any user defined constructors
   struct dummy final
   {};

   // If you want to make an empty struct or a struct with constructors visible in reflected structs,
   // add the folllwing constructor to your type:
   // my_struct(glz::make_reflectable) {}
   using make_reflectable = std::initializer_list<dummy>;

   namespace detail
   {
      template <class Tuple>
      using reflection_value_tuple_variant_t = typename tuple_ptr_variant<Tuple>::type;

      template <class T, size_t I>
      struct named_member
      {
         static constexpr sv value = get<I>(member_names<T>);
      };

      template <class T, bool use_hash_comparison, size_t... I>
      constexpr auto make_reflection_map_impl(std::index_sequence<I...>)
      {
         using V = decltype(to_tuple(std::declval<T>()));
         constexpr auto n = glz::tuple_size_v<V>;
         constexpr auto members = member_names<T>;
         static_assert(members.size() == n);

         using value_t = reflection_value_tuple_variant_t<V>;

         if constexpr (n == 0) {
            return nullptr; // Hack to fix MSVC
            // static_assert(false_v<T>, "Empty object map is illogical. Handle empty upstream.");
         }
         else if constexpr (n == 1) {
            return micro_map1<value_t, named_member<T, I>::value...>{
               pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n == 2) {
            return micro_map2<value_t, named_member<T, I>::value...>{
               pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n < 64) // don't even attempt a first character hash if we have too many keys
         {
            constexpr auto& keys = member_names<T>;
            constexpr auto front_desc = single_char_hash<n>(keys);

            if constexpr (front_desc.valid) {
               return make_single_char_map<value_t, front_desc>(
                  {{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
            }
            else {
               constexpr single_char_hash_opts rear_hash{.is_front_hash = false};
               constexpr auto back_desc = single_char_hash<n, rear_hash>(keys);

               if constexpr (back_desc.valid) {
                  return make_single_char_map<value_t, back_desc>(
                     {{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
               }
               else {
                  constexpr single_char_hash_opts sum_hash{.is_front_hash = true, .is_sum_hash = true};
                  constexpr auto sum_desc = single_char_hash<n, sum_hash>(keys);

                  if constexpr (sum_desc.valid) {
                     return make_single_char_map<value_t, sum_desc>(
                        {{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
                  }
                  else {
                     if constexpr (n <= naive_map_max_size) {
                        constexpr auto naive_desc = naive_map_hash<use_hash_comparison, n>(keys);
                        return glz::detail::make_naive_map<value_t, naive_desc>(std::array{
                           pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
                     }
                     else {
                        return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(std::array{
                           pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
                     }
                  }
               }
            }
         }
         else {
            return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(
               std::array{pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
         }
      }

      template <reflectable T, bool use_hash_comparison = false>
      constexpr auto make_map()
         requires(!glaze_t<T> && !array_t<T> && std::is_aggregate_v<std::remove_cvref_t<T>>)
      {
         constexpr auto indices = std::make_index_sequence<count_members<T>>{};
         return make_reflection_map_impl<decay_keep_volatile_t<T>, use_hash_comparison>(indices);
      }

      template <reflectable T>
      GLZ_ALWAYS_INLINE constexpr void populate_map(T&& value, auto& cmap) noexcept
      {
         // we have to populate the pointers in the reflection map from the structured binding
         auto t = to_tuple(std::forward<T>(value));
         [&]<size_t... I>(std::index_sequence<I...>) {
            ((std::get<std::add_pointer_t<decay_keep_volatile_t<decltype(std::get<I>(t))>>>(
                 std::get<I>(cmap.items).second) = &std::get<I>(t)),
             ...);
         }(std::make_index_sequence<count_members<T>>{});
      }
   }
}
