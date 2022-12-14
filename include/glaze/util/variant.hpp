// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <variant>

#include "glaze/util/type_traits.hpp"

namespace glz
{
   template <class T>
   concept is_variant = is_specialization_v<T, std::variant>;
   
   template <class ...T>
   size_t variant_container_size(const std::variant<T...>& v)
   {
       return std::visit([](auto&& x) -> size_t {
          using Container = std::decay_t<decltype(x)>;
          if constexpr (std::same_as<Container, std::monostate>) {
             throw std::runtime_error("container_size: container is monostate");
          }
          else {
             return x.size();
          }
       }, v);
   }
}
