// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

namespace glz::detail
{
   template <class T>
   concept is_value_proxy = requires { T::glaze_value_proxy; };

   template <is_value_proxy T>
   struct from<JSON, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         read<JSON>::op<Opts>(value.value(), ctx, it, end);
      }
   };
}
