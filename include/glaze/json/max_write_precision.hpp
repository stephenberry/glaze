// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/opts.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // set the maximum precision for writing floats
   template <class T>
   struct max_write_precision_t
   {
      static constexpr bool glaze_wrapper = true;
      using value_type = T;
      T& val;
   };

   namespace detail
   {
      template <class T>
      struct from_json<max_write_precision_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<Opts>(value.val, args...); // no difference reading
         }
      };

      template <class T>
      struct to_json<max_write_precision_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::float_max_write_precision>>(value.val, ctx, args...);
         }
      };

      template <auto MemPtr>
      inline constexpr decltype(auto) max_write_precision_impl() noexcept
      {
         return [](auto&& val) { return max_write_precision_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   template <auto MemPtr>
   constexpr auto max_write_precision = detail::max_write_precision_impl<MemPtr>();
}
