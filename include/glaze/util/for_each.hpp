// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>
#include <utility>

#include "glaze/util/inline.hpp"
#include "glaze/util/utility.hpp"

namespace glz
{
   // Compile time iterate over I indices
   // We do not make this noexcept so that it can be used in exception contexts
   template <std::size_t N, class Func>
   constexpr void for_each(Func&& f)
   {
      if constexpr (N == 0) {
         return;
      }
      else {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (f(std::integral_constant<std::size_t, I>{}), ...);
         }(std::make_index_sequence<N>{});
      }
   }

   // Runtime short circuiting if function returns true, return false to continue evaluation
   template <std::size_t N, class Func>
   constexpr void for_each_short_circuit(Func&& f)
   {
      if constexpr (N == 0) {
         return;
      }
      else {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (f(std::integral_constant<std::size_t, I>{}) || ...);
         }(std::make_index_sequence<N>{});
      }
   }

   template <class Func, class Tuple>
   constexpr void for_each_apply(Func&& f, Tuple&& t)
   {
      constexpr size_t N = std::tuple_size_v<std::decay_t<Tuple>>;
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::get<I>(t)), ...);
      }(std::make_index_sequence<N>{});
   }
}

namespace glz
{
   // There is no benefit of perfectly forwarding the lambda because it is immediately invoked.
   // It actually removes an assembly instruction on GCC and Clang to pass by reference with O0
   
   template <size_t N>
   inline constexpr void visit(auto&& lambda, const size_t index) {
      if constexpr (N > 0) {
         [&, index]<size_t... I>(std::index_sequence<I...>) constexpr {
            (void)((index == I ? lambda.template operator()<I>() : void()), ...);
         }(std::make_index_sequence<N>{});
      }
   }
   
   template <size_t N>
   inline constexpr void visit_all(auto&& lambda) {
      if constexpr (N > 0) {
         [&]<size_t... I>(std::index_sequence<I...>) constexpr {
            (void)(lambda.template operator()<I>(), ...);
         }(std::make_index_sequence<N>{});
      }
   }
}
