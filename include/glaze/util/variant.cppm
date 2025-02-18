// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#include <algorithm>
#include <cstddef>
#include <variant>
#ifdef CPP_MODULES
export module glaze.util.variant;
import glaze.util.for_each;
import glaze.util.parse;
import glaze.util.type_traits;
#else
#include "glaze/util/for_each.cppm"
#include "glaze/util/parse.cppm"
#include "glaze/util/type_traits.cppm"
#endif





namespace glz
{
   template <class T>
   concept is_variant = is_specialization_v<T, std::variant>;

   namespace detail
   {
      template <is_variant T>
      GLZ_ALWAYS_INLINE constexpr auto runtime_variant_map()
      {
         constexpr auto N = std::variant_size_v<T>;
         return [&]<size_t... I>(std::index_sequence<I...>) {
            return std::array<T, N>{std::variant_alternative_t<I, T>{}...};
         }(std::make_index_sequence<N>{});
      }
   }
}
