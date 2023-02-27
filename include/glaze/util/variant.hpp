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
      x(0) x(1) x(2) x(3) x(4) x(5) x(6) x(7) x(8) x(9) x(10) x(11) x(12) x(13) x(14) x(15) x(16) x(17) x(18) x(19) x(20) \
         x(21) x(22) x(23) x(24) x(25) x(26) x(27) x(28) x(29) x(30) x(31)                                     \
   }

namespace glz
{
   [[noreturn]] inline void unreachable() noexcept {
      // Uses compiler specific extensions if possible.
      // Even if no extension is used, undefined behavior is still raised by
      // an empty function body and the noreturn attribute.
   #ifdef __GNUC__
      // GCC, Clang, ICC
      __builtin_unreachable();
   #elifdef _MSC_VER
      // MSVC
      __assume(false);
   #endif
   }
   
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
   GLZ_ALWAYS_INLINE decltype(auto) visit(auto&& f, auto&& v) noexcept
   {
      constexpr auto vs = std::variant_size_v<std::remove_cvref_t<decltype(v)>>;
      
      using V = decltype(v);
      using dV = std::decay_t<V>;
      using F = decltype(f);
      
      if constexpr (vs == 1) {
         return std::forward<F>(f)(*std::get_if<std::variant_alternative_t<0, dV>>(&std::forward<V>(v)));
      }
      else if constexpr (vs == 2) {
         if (v.index() == 0) {
            return std::forward<F>(f)(*std::get_if<std::variant_alternative_t<0, dV>>(&std::forward<V>(v)));
         }
         else {
            return std::forward<F>(f)(*std::get_if<std::variant_alternative_t<1, dV>>(&std::forward<V>(v)));
         }
      }
      else {
#define GLZ_VISIT_CASE(N)                                                                       \
   case I + N: {                                                                             \
      if constexpr (I + N < vs) {                                                            \
         return std::forward<F>(f)(*std::get_if<I + N>(&std::forward<V>(v))); \
      }                                                                                      \
   }                                                                                         \

         switch (v.index()) {
            GLZ_REPEAT_32_I(GLZ_VISIT_CASE)
         }
#undef GLZ_VISIT_CASE

         constexpr auto next_idx = I + 32;

         if constexpr (next_idx < vs) {
            return visit1<next_idx>(std::forward<F>(f), std::forward<V>(v));
         }
         unreachable();
      }
   }
}
