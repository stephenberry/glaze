// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <cstddef>
#include <variant>

#include "glaze/util/for_each.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/type_traits.hpp"

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
         std::array<T, N> ret{};
         for_each<N>([&](auto I) { ret[I] = std::variant_alternative_t<I, T>{}; });
         return ret;
      }
   }
}
