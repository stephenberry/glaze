// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

namespace glz
{
   // Compile time iterate over I indices
   // We do not make this noexcept so that it can be used in exception contexts
   template <std::size_t N, class Func>
   constexpr void for_each(Func&& f)
   {
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::integral_constant<std::size_t, I>{}), ...);
      }(std::make_index_sequence<N>{});
   }

   // Runtime short circuiting if function returns true, return false to continue evaluation
   template <std::size_t N, class Func>
   constexpr void for_each_short_circuit(Func&& f)
   {
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::integral_constant<std::size_t, I>{}) || ...);
      }(std::make_index_sequence<N>{});
   }
}
