// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <stddef.h>

#include <algorithm>
#include <variant>

#include "glaze/util/for_each.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   [[noreturn]] inline void unreachable() noexcept
   {
      // Uses compiler specific extensions if possible.
      // Even if no extension is used, undefined behavior is still raised by
      // an empty function body and the noreturn attribute.
#ifdef __GNUC__
      // GCC, Clang, ICC
      __builtin_unreachable();
#else
#ifdef _MSC_VER
      // MSVC
      __assume(false);
#endif
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
}
