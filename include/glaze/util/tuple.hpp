// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

#include "glaze/reflection/get_name.hpp"
#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   template <class T>
   concept is_std_tuple = is_specialization_v<T, std::tuple>;

   // group builder code
   template <size_t N>
   constexpr auto shrink_index_array(auto&& arr)
   {
      std::array<size_t, N> res{};
      for (size_t i = 0; i < N; ++i) {
         res[i] = arr[i];
      }
      return res;
   }

   template <class Type>
   concept is_schema_class = requires {
      requires std::is_class_v<Type>;
      requires Type::schema_attributes;
   };
}
