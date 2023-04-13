// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

namespace glz
{
   template <class = void, std::size_t... Is>
   constexpr auto indexer(std::index_sequence<Is...>) noexcept
   {
      return [](auto&& f) noexcept -> decltype(auto) {
         return decltype(f)(f)(std::integral_constant<std::size_t, Is>{}...);
      };
   }

   // takes a number N
   // returns a function object that, when passed a function object f
   // passes it compile-time values from 0 to N-1 inclusive.
   template <size_t N>
   constexpr auto indexer() noexcept
   {
      return indexer(std::make_index_sequence<N>{});
   }

   template <size_t N, class Func>
   constexpr auto for_each(Func&& f) noexcept
   {
      return indexer<N>()([&](auto&&... i) { (std::forward<Func>(f)(i), ...); });
   }

   template <size_t N, size_t I = 0, class Func, class Value>
   constexpr void for_each_value(Func&& f, Value&& v) noexcept
   {
      if constexpr (I != N) {
         for_each_value<N, I + 1>(f, f(std::integral_constant<size_t, I>{}, v));
      }
   }
}
