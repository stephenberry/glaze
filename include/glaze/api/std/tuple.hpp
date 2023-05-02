// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <map>

#include "glaze/api/name.hpp"

namespace glz
{
   namespace detail
   {
      template <class Tuple, size_t... I>
      constexpr std::string_view tuple_name_impl(std::index_sequence<I...>)
      {
         return join_v<chars<"std::tuple<">,
                       ((I != std::tuple_size_v<Tuple> - 1) ? join_v<name_v<std::tuple_element_t<I, Tuple>>, chars<",">>
                                                            : join_v<name_v<std::tuple_element_t<I, Tuple>>>)...,
                       chars<">">>;
      }
   }

   template <class T>
   concept tuple = is_specialization_v<T, std::tuple>;

   template <tuple T>
   struct meta<T>
   {
      static constexpr std::string_view name =
         detail::tuple_name_impl<T>(std::make_index_sequence<std::tuple_size_v<T>>());
   };
}
