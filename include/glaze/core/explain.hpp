// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/util/for_each.hpp"

namespace glz
{
   // Diagnostics API to explain compile-time issues with Glaze serialization.
   // Used by developers to trace exactly why a type does not satisfy the `write_supported` concept.
   template <uint32_t Format = JSON, class T>
   consteval bool explain_write_supported()
   {
      using V = std::remove_cvref_t<T>;

      if constexpr (write_supported<V, Format>) {
         return true;
      }
      else {
         if constexpr (glaze_object_t<V> || reflectable<V>) {
            constexpr size_t N = reflect<V>::size;

            for_each<N>([]<size_t I>() {
               using field_type = field_t<V, I>;

               if constexpr (!write_supported<field_type, Format>) {
                  static_assert(write_supported<field_type, Format>,
                                "[Glaze Diagnostics] Structural member is not serializable. Check if member's type "
                                "has glz::meta or is reflectable.");
               }
            });
         }
         else if constexpr (requires { typename V::value_type; }) {
            using element_type = typename V::value_type;
            static_assert(write_supported<element_type, Format>,
                          "[Glaze Diagnostics] Container's value_type is not serializable.");
         }
         else {
            static_assert(write_supported<V, Format>,
                          "[Glaze Diagnostics] Type is not serializable. It has no reflection metadata registered.");
         }

         return false;
      }
   }
}
