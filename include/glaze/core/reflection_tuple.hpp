// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/core/refl.hpp"

namespace glz::detail
{
   template <class T>
   decltype(auto) reflection_tuple(auto&& value, auto&&...) noexcept
   {
      if constexpr (reflectable<T>) {
         using V = decay_keep_volatile_t<decltype(value)>;
         if constexpr (std::is_const_v<std::remove_reference_t<decltype(value)>>) {
#if ((defined _MSC_VER) && (!defined __clang__))
            static thread_local auto tuple_of_ptrs = make_const_tuple_from_struct<V>();
#else
            static thread_local constinit auto tuple_of_ptrs = make_const_tuple_from_struct<V>();
#endif
            populate_tuple_ptr(value, tuple_of_ptrs);
            return tuple_of_ptrs;
         }
         else {
#if ((defined _MSC_VER) && (!defined __clang__))
            static thread_local auto tuple_of_ptrs = make_tuple_from_struct<V>();
#else
            static thread_local constinit auto tuple_of_ptrs = make_tuple_from_struct<V>();
#endif
            populate_tuple_ptr(value, tuple_of_ptrs);
            return tuple_of_ptrs;
         }
      }
      else {
         return nullptr;
      }
   }
}
