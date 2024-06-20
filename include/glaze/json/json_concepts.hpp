// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

namespace glz
{
   template <class T>
   concept json_object =
      detail::glaze_object_t<T> || detail::reflectable<T> || detail::writable_map_t<T> || detail::readable_map_t<T>;

   template <class T>
   concept json_array = detail::array_t<T>;

   template <class T>
   concept json_string = detail::str_t<T>;

   template <class T>
   concept json_boolean = detail::boolean_like<T>;

   template <class T>
   concept json_number = detail::num_t<T>;

   template <class T>
   concept json_integer = detail::int_t<T>;

   template <class T>
   concept json_null = detail::null_t<T>;
}
