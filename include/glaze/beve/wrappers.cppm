// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#include <type_traits>
#ifdef CPP_MODULES
export module glaze.beve.wrappers;
import glaze.core.custom;
import glaze.core.opts;
import glaze.core.wrappers;
import glaze.json.read;
import glaze.json.write;
#else
#include "glaze/core/custom.cppm"
#include "glaze/core/opts.cppm"
#include "glaze/core/wrappers.cppm"
#include "glaze/json/read.cppm"
#include "glaze/json/write.cppm"
#endif





namespace glz::detail
{
   template <is_opts_wrapper T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args)
      {
         read<BEVE>::op<opt_true<Opts, T::opts_member>>(value.val, args...);
      }
   };

   template <is_opts_wrapper T>
   struct to<BEVE, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         write<BEVE>::op<opt_true<Opts, T::opts_member>>(value.val, ctx, args...);
      }
   };
}
