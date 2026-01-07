// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

namespace glz
{
   // is_value_proxy concept is defined in common.hpp

   template <uint32_t Format, is_value_proxy T>
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         parse<JSON>::op<Opts>(value.value(), ctx, it, end);
      }
   };
}
