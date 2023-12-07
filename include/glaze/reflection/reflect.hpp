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

      template <class T, bool use_hash_comparison, size_t... I>
      constexpr auto make_reflection_map_impl(std::index_sequence<I...>)
      {
         using V = decltype(to_tuple(std::declval<T>()));
         constexpr auto n = std::tuple_size_v<V>;
         [[maybe_unused]] constexpr auto members = member_names<T>();
         static_assert(count_members<T>() == n);

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

         return naive_or_normal_hash();
      }

      template <class T, bool use_hash_comparison = false>
      constexpr auto make_reflection_map()
         requires(!glaze_t<T> && !array_t<T> && std::is_aggregate_v<std::remove_cvref_t<T>>)
      {
         using V = decltype(to_tuple(std::declval<T>()));

         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<V>>{};
         return make_reflection_map_impl<std::decay_t<T>, use_hash_comparison>(indices);
      }
   }
}
