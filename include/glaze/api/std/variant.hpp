// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <map>
#include <variant>

#include "glaze/api/name.hpp"

namespace glz
{
   namespace detail
   {
      template <class Variant, size_t... I>
      constexpr std::string_view variant_name_impl(std::index_sequence<I...>)
      {
         return join_v<chars<"std::variant<">,
                       ((I != std::variant_size_v<Variant> - 1)
                           ? join_v<name_v<std::variant_alternative_t<I, Variant>>, chars<",">>
                           : join_v<name_v<std::variant_alternative_t<I, Variant>>>)...,
                       chars<">">>;
      }
   }

   template <class T>
   concept variant_specialization = is_specialization_v<T, std::variant>;

   template <variant_specialization T>
   struct meta<T>
   {
      static constexpr std::string_view name =
         detail::variant_name_impl<T>(std::make_index_sequence<std::variant_size_v<T>>());
   };

   template <>
   struct meta<std::monostate>
   {
      static constexpr std::string_view name = "std::monostate";
   };
}
