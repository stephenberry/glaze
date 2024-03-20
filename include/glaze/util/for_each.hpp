// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

namespace glz
{
   template <class = void, std::size_t... Is>
   constexpr auto indexer(std::index_sequence<Is...>) noexcept
   {
      return []<class F>(F&& f) noexcept -> decltype(auto) {
         return std::forward<F>(f)(std::integral_constant<std::size_t, Is>{}...);
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
      return indexer<N>()([&](auto&&... i) noexcept { (std::forward<Func>(f)(i), ...); });
   }
}

// versions that allow exceptions to propagate, for when users implement functions that throw
namespace glz
{
   template <class = void, std::size_t... Is>
   constexpr auto indexer_ex(std::index_sequence<Is...>)
   {
      return []<class F>(F&& f) -> decltype(auto) {
         return std::forward<F>(f)(std::integral_constant<std::size_t, Is>{}...);
      };
   }

   // takes a number N
   // returns a function object that, when passed a function object f
   // passes it compile-time values from 0 to N-1 inclusive.
   template <size_t N>
   constexpr auto indexer_ex()
   {
      return indexer_ex(std::make_index_sequence<N>{});
   }

   template <size_t N, class Func>
   constexpr auto for_each_ex(Func&& f)
   {
      return indexer_ex<N>()([&](auto&&... i) { (std::forward<Func>(f)(i), ...); });
   }
}
