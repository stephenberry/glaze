// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/opts.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // do not decode/encode escaped characters for strings
   template <class T>
   struct raw_string_t
   {
      T& val;
   };

   // oposite of raw_string_t, turns off the option
   template <class T>
   struct escaped_t
   {
      T& val;
   };

   namespace detail
   {
      template <class T>
      struct from_json<raw_string_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<opt_true<Opts, &opts::raw_string>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<raw_string_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::raw_string>>(value.val, ctx, args...);
         }
      };

      template <class T>
      struct from_json<escaped_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<opt_false<Opts, &opts::raw_string>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<escaped_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_false<Opts, &opts::raw_string>>(value.val, ctx, args...);
         }
      };

      template <auto MemPtr>
      GLZ_ALWAYS_INLINE constexpr decltype(auto) raw_string_impl() noexcept
      {
         return [](auto&& val) { return raw_string_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }

      template <auto MemPtr>
      GLZ_ALWAYS_INLINE constexpr decltype(auto) escaped_impl() noexcept
      {
         return [](auto&& val) { return escaped_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   template <auto MemPtr>
   constexpr auto raw_string = detail::raw_string_impl<MemPtr>();

   template <auto MemPtr>
   constexpr auto escaped = detail::escaped_impl<MemPtr>();
}
