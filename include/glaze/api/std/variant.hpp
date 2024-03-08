// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <variant>

#include "glaze/api/name.hpp"

namespace glz
{
   namespace detail
   {
      template <class... T, size_t... I>
      constexpr std::string_view variant_name_impl(std::index_sequence<I...>)
      {
         return join_v<chars<"std::variant<">,
                       ((I != sizeof...(T) - 1)
                           ? join_v<name_v<T>, chars<",">>
                           : join_v<name_v<T>>)...,
                       chars<">">>;
      }
   }

   template <class... T>
   struct meta<std::variant<T...>>
   {
      static constexpr std::string_view name =
         detail::variant_name_impl<T...>(std::make_index_sequence<sizeof...(T)>());
   };

   template <>
   struct meta<std::monostate>
   {
      static constexpr std::string_view name = "std::monostate";
   };
}
