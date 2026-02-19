// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(GLAZE_CXX_MODULE)
#define GLAZE_EXPORT export
#else
#define GLAZE_EXPORT
#include <variant>
#endif

#include "glaze/core/meta.hpp"

GLAZE_EXPORT namespace glz
{
   template <class... T>
   struct meta<std::variant<T...>>
   {
      static constexpr std::string_view name = []<size_t... I>(std::index_sequence<I...>) {
         return join_v<chars<"std::variant<">,
                       ((I != sizeof...(T) - 1) ? join_v<name_v<T>, chars<",">> : join_v<name_v<T>>)..., chars<">">>;
      }(std::make_index_sequence<sizeof...(T)>{});
   };

   template <>
   struct meta<std::monostate>
   {
      static constexpr std::string_view name = "std::monostate";
   };
}
