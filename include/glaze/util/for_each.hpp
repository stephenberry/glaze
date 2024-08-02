// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

#include "glaze/util/inline.hpp"

namespace glz
{
   // Compile time iterate over I indices
   // We do not make this noexcept so that it can be used in exception contexts
   template <std::size_t N, class Func>
   constexpr void for_each(Func&& f)
   {
      if constexpr (N == 1) {
         f(std::integral_constant<std::size_t, 0>{});
      }
      else if constexpr (N == 2) {
         f(std::integral_constant<std::size_t, 0>{});
         f(std::integral_constant<std::size_t, 1>{});
      }
      else if constexpr (N > 0) {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (f(std::integral_constant<std::size_t, I>{}), ...);
         }(std::make_index_sequence<N>{});
      }
   }

   template <std::size_t N, class Func>
   GLZ_FLATTEN constexpr void for_each_flatten(Func&& f)
   {
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::integral_constant<std::size_t, I>{}), ...);
      }(std::make_index_sequence<N>{});
   }

   // Runtime short circuiting if function returns true, return false to continue evaluation
   template <std::size_t N, class Func>
   constexpr void for_each_short_circuit(Func&& f)
   {
      if constexpr (N == 1) {
         f(std::integral_constant<std::size_t, 0>{});
      }
      else if constexpr (N == 2) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{});
      }
      else if constexpr (N == 3) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{});
      }
      else if constexpr (N == 4) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{});
      }
      else if constexpr (N == 5) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{});
      }
      else if constexpr (N == 6) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{});
      }
      else if constexpr (N == 7) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{});
      }
      else if constexpr (N == 8) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{}) //
         || f(std::integral_constant<std::size_t, 7>{});
      }
      else if constexpr (N == 9) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{}) //
         || f(std::integral_constant<std::size_t, 7>{}) //
         || f(std::integral_constant<std::size_t, 8>{});
      }
      else if constexpr (N == 10) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{}) //
         || f(std::integral_constant<std::size_t, 7>{}) //
         || f(std::integral_constant<std::size_t, 8>{}) //
         || f(std::integral_constant<std::size_t, 9>{});
      }
      else if constexpr (N == 11) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{}) //
         || f(std::integral_constant<std::size_t, 7>{}) //
         || f(std::integral_constant<std::size_t, 8>{}) //
         || f(std::integral_constant<std::size_t, 9>{}) //
         || f(std::integral_constant<std::size_t, 10>{});
      }
      else if constexpr (N == 12) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{}) //
         || f(std::integral_constant<std::size_t, 7>{}) //
         || f(std::integral_constant<std::size_t, 8>{}) //
         || f(std::integral_constant<std::size_t, 9>{}) //
         || f(std::integral_constant<std::size_t, 10>{}) //
         || f(std::integral_constant<std::size_t, 11>{});
      }
      else if constexpr (N == 13) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{}) //
         || f(std::integral_constant<std::size_t, 7>{}) //
         || f(std::integral_constant<std::size_t, 8>{}) //
         || f(std::integral_constant<std::size_t, 9>{}) //
         || f(std::integral_constant<std::size_t, 10>{}) //
         || f(std::integral_constant<std::size_t, 11>{}) //
         || f(std::integral_constant<std::size_t, 12>{});
      }
      else if constexpr (N == 14) {
         f(std::integral_constant<std::size_t, 0>{}) //
         || f(std::integral_constant<std::size_t, 1>{}) //
         || f(std::integral_constant<std::size_t, 2>{}) //
         || f(std::integral_constant<std::size_t, 3>{}) //
         || f(std::integral_constant<std::size_t, 4>{}) //
         || f(std::integral_constant<std::size_t, 5>{}) //
         || f(std::integral_constant<std::size_t, 6>{}) //
         || f(std::integral_constant<std::size_t, 7>{}) //
         || f(std::integral_constant<std::size_t, 8>{}) //
         || f(std::integral_constant<std::size_t, 9>{}) //
         || f(std::integral_constant<std::size_t, 10>{}) //
         || f(std::integral_constant<std::size_t, 11>{}) //
         || f(std::integral_constant<std::size_t, 12>{}) //
         || f(std::integral_constant<std::size_t, 13>{});
      }
      else if constexpr (N > 0) {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (f(std::integral_constant<std::size_t, I>{}) || ...);
         }(std::make_index_sequence<N>{});
      }
   }

   template <class Func, class Tuple>
   constexpr void for_each_apply(Func&& f, Tuple&& t)
   {
      constexpr size_t N = glz::tuple_size_v<std::decay_t<Tuple>>;
      if constexpr (N == 1) {
         f(std::get<0>(t));
      }
      else if constexpr (N > 0) {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (f(std::get<I>(t)), ...);
         }(std::make_index_sequence<N>{});
      }
   }
}
