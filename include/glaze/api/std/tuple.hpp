// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

#include "glaze/core/meta.hpp"

namespace glz
{
   namespace detail
   {
      template <class... T, size_t... I>
      constexpr std::string_view tuple_name_impl(std::index_sequence<I...>)
      {
         return join_v<chars<"std::tuple<">,
                       ((I != sizeof...(T) - 1) ? join_v<name_v<T>, chars<",">> : join_v<name_v<T>>)..., chars<">">>;
      }
   }

   template <class... T>
   struct meta<std::tuple<T...>>
   {
      static constexpr std::string_view name = detail::tuple_name_impl<T...>(std::make_index_sequence<sizeof...(T)>());
   };
}
