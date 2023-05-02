// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <map>

#include "glaze/api/name.hpp"
#include "glaze/tuplet/tuple.hpp"

namespace glz
{
   namespace detail
   {
      template <class Tuple, size_t... I>
      constexpr std::string_view tuplet_name_impl(std::index_sequence<I...>)
      {
         return join_v<chars<"glz::tuplet::tuple<">,
                       ((I != std::tuple_size_v<Tuple> - 1) ? join_v<name_v<std::tuple_element_t<I, Tuple>>, chars<",">>
                                                            : join_v<name_v<std::tuple_element_t<I, Tuple>>>)...,
                       chars<">">>;
      }
   }

   template <class T>
      requires is_specialization_v<T, glz::tuplet::tuple>
   struct meta<T>
   {
      static constexpr std::string_view name =
         detail::tuplet_name_impl<T>(std::make_index_sequence<std::tuple_size_v<T>>());
   };
}
