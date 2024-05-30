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

   template <class Func, class Tuple>
   constexpr void for_each_apply(Func&& f, Tuple&& t)
   {
      constexpr size_t N = glz::tuple_size_v<std::decay_t<Tuple>>;
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::get<I>(t)), ...);
      }(std::make_index_sequence<N>{});
   }
}
