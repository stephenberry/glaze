// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(GLAZE_CXX_MODULE)
#define GLAZE_EXPORT export
#else
#define GLAZE_EXPORT
#include <array>
#endif

#include "glaze/core/meta.hpp"

namespace glz
{
   namespace detail
   {
      template <unsigned... digits>
      struct to_chars
      {
         static constexpr char value[] = {('0' + digits)..., 0};
      };

      template <unsigned rem, unsigned... digits>
      struct explode : explode<rem / 10, rem % 10, digits...>
      {};

      template <unsigned... digits>
      struct explode<0, digits...> : to_chars<digits...>
      {};
   }

   GLAZE_EXPORT template <unsigned num>
   struct num_to_string : detail::explode<num>
   {};

   GLAZE_EXPORT template <class T, size_t N>
   struct meta<std::array<T, N>>
   {
      static constexpr std::string_view name =
         join_v<chars<"std::array<">, name_v<T>, chars<",">, chars<num_to_string<N>::value>, chars<">">>;
   };
}
