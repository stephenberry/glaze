#pragma once

#include "glaze/core/wrappers.hpp"

namespace glz
{
   namespace detail
   {
      template <is_opts_wrapper T>
      struct from<ERLANG, T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args)
         {
            read<ERLANG>::op<opt_true<Opts, T::opts_bit>>(value.val, args...);
         }
      };

      template <is_opts_wrapper T>
      struct to<ERLANG, T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            write<ERLANG>::op<opt_true<Opts, T::opts_bit>>(value.val, ctx, args...);
         }
      };
   } // namespace detail

   template <auto MemPtr>
   constexpr auto atom_as_string = detail::opts_wrapper<MemPtr, option::atom_as_string>();
}
