// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <span>

#include "glaze/api/name.hpp"

namespace glaze
{
   template <class, template<class, size_t> class>
   inline constexpr bool is_span_v = false;
   template <template<class, size_t> class T, class Element, size_t Extent>
   inline constexpr bool is_span_v<T<Element, Extent>, T> = true;
   
   template <class T>
   concept span = is_span_v<T, std::span>;
   
   template <span T>
   struct name_t<T> {
      using V = typename T::element_type;
      static constexpr std::string_view extent = to_sv<T::extent>();
      static constexpr std::string_view value = detail::join_v<chars<"std::span<">,
      name<V>,
      chars<",">,
      extent,
      chars<">">>;
   };
}
