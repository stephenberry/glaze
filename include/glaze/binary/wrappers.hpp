// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/opts.hpp"
#include "glaze/core/wrappers.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz::detail
{
   template <is_opts_wrapper T>
   struct from_binary<T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
      {
         read<binary>::op<opt_true<Opts, T::opts_member>>(value.val, args...);
      }
   };

   template <is_opts_wrapper T>
   struct to_binary<T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         write<binary>::op<opt_true<Opts, T::opts_member>>(value.val, ctx, args...);
      }
   };
}
