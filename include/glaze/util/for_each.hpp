// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>
#include <utility>

#include "glaze/util/inline.hpp"
#include "glaze/util/utility.hpp"

// We do not mark these functions noexcept so that it can be used in exception contexts
// Furthermore, adding noexcept can increase assembly size because exceptions need to cause termination

namespace glz
{
   // There is no benefit to perfectly forward the lambda (in the internal lambda) because it is immediately invoked
   // It actually removes an assembly instruction on GCC and Clang to pass by reference with O0
   // With O0 GCC and Clang produce better assembly using the templated I approach rather than
   // the approach that passes std::integral_constant
   
   // Compile time iterate over I indices
   template <size_t N>
   inline constexpr void for_each(auto&& lambda) {
      if constexpr (N > 0) {
         // Explicit sizes for small N to help the compiler and make debugging easier
         // These generate less assembly with O0, but make no difference for higher optimization
         if constexpr (N == 1) {
            lambda.template operator()<0>();
         }
         else if constexpr (N == 2) {
            lambda.template operator()<0>();
            lambda.template operator()<1>();
         }
         else if constexpr (N == 3) {
            lambda.template operator()<0>();
            lambda.template operator()<1>();
            lambda.template operator()<2>();
         }
         else if constexpr (N == 4) {
            lambda.template operator()<0>();
            lambda.template operator()<1>();
            lambda.template operator()<2>();
            lambda.template operator()<3>();
         }
         else {
            [&]<size_t... I>(std::index_sequence<I...>) {
               (void)(lambda.template operator()<I>(), ...);
            }(std::make_index_sequence<N>{});
         }
      }
   }
   
   // Runtime short circuiting if function returns true, return false to continue evaluation
   template <size_t N>
   constexpr void for_each_short_circuit(auto&& lambda)
   {
      if constexpr (N > 0) {
         [&]<size_t... I>(std::index_sequence<I...>) {
            (lambda.template operator()<I>() || ...);
         }(std::make_index_sequence<N>{});
      }
   }
   
   template <class Func, class Tuple>
   constexpr void for_each_apply(Func&& f, Tuple&& t)
   {
      constexpr size_t N = std::tuple_size_v<std::decay_t<Tuple>>;
      [&]<size_t... I>(std::index_sequence<I...>) {
         (f(std::get<I>(t)), ...);
      }(std::make_index_sequence<N>{});
   }
   
   // Important: index must be less than N
   template <size_t N>
   inline constexpr void visit(auto&& lambda, const size_t index) {
      if constexpr (N > 0) {
         if constexpr (N == 1) {
            (void)index;
            (void)(lambda.template operator()<0>());
         }
         else {
            [[assume(index < N)]];
            // Clang very efficiently optimizes this even with 01
            // so, we don't bother implementing switch cases or if-else chains for specific N
            [&, index]<size_t... I>(std::index_sequence<I...>) {
               (void)((index == I ? lambda.template operator()<I>() : void()), ...);
            }(std::make_index_sequence<N>{});
         }
      }
   }
}
