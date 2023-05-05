// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json.hpp"

namespace glz
{
   template <class T>
   struct quoted_t
   {
      T& val;
   };

   template <class T>
   struct number_t
   {
      T& val;
   };

   namespace detail
   {
      template <class T>
      struct from_json<quoted_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<opt_true<Opts, &opts::quoted>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<quoted_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::quoted>>(value.val, ctx, args...);
         }
      };

      template <class T>
      struct from_json<number_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<opt_true<Opts, &opts::number>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<number_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::number>>(value.val, ctx, args...);
         }
      };
   }

   template <auto MemPtr>
   inline constexpr decltype(auto) quoted() noexcept
   {
      return [](auto&& val) { return quoted_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }

   template <auto MemPtr>
   inline constexpr decltype(auto) number() noexcept
   {
      return [](auto&& val) { return number_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }
}
