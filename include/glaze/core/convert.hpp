// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/core/reflect.hpp"

// For generic struct to struct conversion based on reflected fields
// This will convert a struct with optionals to non-optionals
// More conversion can be added in the future

namespace glz
{
   template <class In, class Out>
   void convert(In&& in, Out&& out)
   {
      auto in_tuple = detail::to_tuple(std::forward<In>(in));
      auto out_tuple = detail::to_tuple(std::forward<Out>(out));
      
      constexpr auto N = tuple_size_v<std::decay_t<decltype(in_tuple)>>;
      static_assert(N == tuple_size_v<std::decay_t<decltype(out_tuple)>>);
      
      constexpr auto in_keys = reflect<In>::keys;
      constexpr auto out_keys = reflect<Out>::keys;
      
      for_each<N>([&](auto I){
         static_assert(in_keys[I] == out_keys[I]);
         decltype(auto) l = get<I>(out_tuple);
         decltype(auto) r = get<I>(in_tuple);
         if constexpr (requires { l = r; }) {
            l = r;
         }
         else if constexpr (requires { l = r.value(); }) {
           l = r.value();
         }
         else {
            static_assert(false_v<pair<In, Out>>, "Types are not convertible");
         }
      });
   }
}
