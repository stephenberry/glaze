// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/reflection/get_name.hpp"
#include "glaze/reflection/to_tuple.hpp"

namespace glz
{
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
         constexpr auto n = std::tuple_size_v<V>;
         [[maybe_unused]] constexpr auto members = member_names<T>;
         static_assert(count_members<T> == n);

         using value_t = reflection_value_tuple_variant_t<V>;

         auto naive_or_normal_hash = [&] {
            if constexpr (n <= 20) {
               return glz::detail::naive_map<value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{get<I>(members), std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
            else {
               return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{get<I>(members), std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
         };

         if constexpr (n == 0) {
            return nullptr; // Hack to fix MSVC
            // static_assert(false_v<T>, "Empty object map is illogical. Handle empty upstream.");
         }
         else if constexpr (n == 1) {
            return micro_map1<value_t, named_member<T, I>::value...>{
               std::pair<sv, value_t>{get<I>(members), std::add_pointer_t<std::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n == 2) {
            return micro_map2<value_t, named_member<T, I>::value...>{
               std::pair<sv, value_t>{get<I>(members), std::add_pointer_t<std::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n < 64) // don't even attempt a first character hash if we have too many keys
         {
            constexpr auto front_desc = single_char_hash<n>(member_names<T>);

            if constexpr (front_desc.valid) {
               return make_single_char_map<value_t, front_desc>(
                  {{get<I>(members), std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
            else {
               constexpr auto back_desc = single_char_hash<n, false>(member_names<T>);

               if constexpr (back_desc.valid) {
                  return make_single_char_map<value_t, back_desc>(
                     {{get<I>(members), std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
               }
               else {
                  return naive_or_normal_hash();
               }
            }
         }
         else {
            return naive_or_normal_hash();
         }
      }

      template <reflectable T, bool use_hash_comparison = false>
      constexpr auto make_map()
         requires(!glaze_t<T> && !array_t<T> && std::is_aggregate_v<std::remove_cvref_t<T>>)
      {
         constexpr auto indices = std::make_index_sequence<count_members<T>>{};
         return make_reflection_map_impl<std::decay_t<T>, use_hash_comparison>(indices);
      }

      template <reflectable T>
      constexpr void populate_map(T&& value, auto& cmap) noexcept
      {
         // we have to populate the pointers in the reflection map from the structured binding
         auto t = to_tuple(std::forward<T>(value));
         for_each<count_members<T>>([&](auto I) {
            std::get<std::add_pointer_t<std::decay_t<decltype(std::get<I>(t))>>>(std::get<I>(cmap.items).second) =
               &std::get<I>(t);
         });
      }

      // We create const and not-const versions for when our reflected struct is const or non-const qualified
      template <class Tuple>
      struct tuple_ptr;

      template <class... Ts>
      struct tuple_ptr<std::tuple<Ts...>>
      {
         using type = std::tuple<std::add_pointer_t<Ts>...>;
      };

      template <class Tuple>
      struct tuple_ptr_const;

      template <class... Ts>
      struct tuple_ptr_const<std::tuple<Ts...>>
      {
         using type = std::tuple<std::add_pointer_t<std::add_const_t<std::remove_reference_t<Ts>>>...>;
      };

      // This is needed to hack a fix for MSVC evaluating wrong `if constexpr` branches
      template <class T>
         requires(!reflectable<T>)
      constexpr auto make_tuple_from_struct() noexcept
      {
         return std::tuple{};
      }

      // This needs to produce const qualified pointers so that we can write out const structs
      template <reflectable T>
      constexpr auto make_tuple_from_struct() noexcept
      {
         using V = std::decay_t<decltype(to_tuple(std::declval<T>()))>;
         return typename tuple_ptr<V>::type{};
      }

      template <reflectable T>
      constexpr auto make_const_tuple_from_struct() noexcept
      {
         using V = std::decay_t<decltype(to_tuple(std::declval<T>()))>;
         return typename tuple_ptr_const<V>::type{};
      }

      template <reflectable T, class TuplePtrs>
         requires(!std::is_const_v<TuplePtrs>)
      constexpr void populate_tuple_ptr(T&& value, TuplePtrs& tuple_of_ptrs) noexcept
      {
         // we have to populate the pointers in the reflection tuple from the structured binding
         auto t = to_tuple(std::forward<T>(value));
         for_each<count_members<T>>([&](auto I) { std::get<I>(tuple_of_ptrs) = &std::get<I>(t); });
      }
   }
}
