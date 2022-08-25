// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

namespace glaze
{
   template <class T>
   concept is_tuple = is_specialization_v<T, std::tuple>;
   
   inline constexpr auto size_impl(auto&& t) {
       return std::tuple_size_v<std::decay_t<decltype(t)>>;
   }

   template <class T>
   inline constexpr size_t size_v = std::tuple_size_v<std::decay_t<T>>;
   
   namespace detail
   {
      template <size_t Offset, class Tuple, std::size_t...Is>
      auto tuple_split_impl(Tuple&& tuple, std::index_sequence<Is...>) {
         return std::make_tuple(std::get<Is * 2 + Offset>(tuple)...);
      }
   }
   
   template <class Tuple, std::size_t...Is>
   auto tuple_split(Tuple&& tuple) {
      static constexpr auto N = std::tuple_size_v<Tuple>;
      static constexpr auto is = std::make_index_sequence<N / 2>{};
      return std::make_pair(detail::tuple_split_impl<0>(tuple, is), detail::tuple_split_impl<1>(tuple, is));
   }
}
