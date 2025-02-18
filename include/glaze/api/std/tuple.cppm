// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../../Export.hpp"
#include <tuple>
#ifdef CPP_MODULES
export module glaze.api.std.tuple;
import glaze.core.meta;
#else
#include "glaze/core/meta.cppm"
#endif





namespace glz
{
   template <class... T>
   struct meta<std::tuple<T...>>
   {
      static constexpr std::string_view name = []<size_t... I>(std::index_sequence<I...>) {
         return join_v<chars<"std::tuple<">,
                       ((I != sizeof...(T) - 1) ? join_v<name_v<T>, chars<",">> : join_v<name_v<T>>)..., chars<">">>;
      }(std::make_index_sequence<sizeof...(T)>{});
   };
}
