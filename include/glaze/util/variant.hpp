// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/type_traits.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/parse.hpp"

#include <variant>
#include <stddef.h>
#include <algorithm>

#define GLZ_REPEAT_32_I(x)                                                                                           \
   {                                                                                                                 \
      x(1) x(2) x(3) x(4) x(5) x(6) x(7) x(8) x(9) x(10) x(11) x(12) x(13) x(14) x(15) x(16) x(17) x(18) x(19) x(20) \
         x(21) x(22) x(23) x(24) x(25) x(26) x(27) x(28) x(29) x(30) x(31) x(32)                                     \
   }


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

   // Source: https://www.reddit.com/r/cpp/comments/kst2pu/comment/giilcxv/
   template <size_t I = 0>
   GLZ_ALWAYS_INLINE decltype(auto) visit(auto&& f, auto&& v)
   {
      constexpr auto vs = std::variant_size_v<std::remove_cvref_t<decltype(v)>>;

#define _VISIT_CASE(N)                                                                       \
   case I + N: {                                                                             \
      if constexpr (I + N < vs) {                                                            \
         return std::forward<decltype(f)>(f)(*std::get_if<std::variant_alternative_t<I + N, std::decay_t<decltype(v)>>>(&std::forward<decltype(v)>(v))); \
      }                                                                                      \
   }                                                                                         \
      /**/

      switch (v.index()) {
         GLZ_REPEAT_32_I(_VISIT_CASE)
      }

      constexpr auto next_idx = (std::min)(I + 32, vs);

      // if constexpr(next_idx < vs) causes some weird msvc bug
      if constexpr (next_idx + 0 < vs) {
         return visit1<next_idx>(std::forward<decltype(f)>(f), std::forward<decltype(v)>(v));
      }
      else {
         return std::forward<decltype(f)>(f)(std::get<0>(std::forward<decltype(v)>(v)));
      }
   }
}
