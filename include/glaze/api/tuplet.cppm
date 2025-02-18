// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#ifdef CPP_MODULES
export module glaze.api.tuplet;
import glaze.core.meta;
import glaze.tuplet.tuple;
#else
#include "glaze/core/meta.cppm"
#include "glaze/tuplet/tuple.cppm"
#endif




namespace glz
{
   template <class... T>
   struct meta<glz::tuple<T...>>
   {
      static constexpr std::string_view name = []<size_t... I>(std::index_sequence<I...>) {
         return join_v<chars<"glz::tuple<">,
                       ((I != sizeof...(T) - 1) ? join_v<name_v<T>, chars<",">> : join_v<name_v<T>>)..., chars<">">>;
      }(std::make_index_sequence<sizeof...(T)>{});
   };
}
