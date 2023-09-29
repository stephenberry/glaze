// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json.hpp"

namespace glz
{
   // treat numbers as quoted or array-like types as having quoted numbers
   template <class T>
   struct quoted_num_t
   {
      T& val;
   };

   // unquote a string input to avoid double parsing into a value
   template <class T>
   struct unquote_t
   {
      T& val;
   };

   // read numbers as strings and write these string as numbers
   template <class T>
   struct number_t
   {
      T& val;
   };

   namespace detail
   {
      template <class T>
      struct from_json<quoted_num_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<opt_true<Opts, &opts::quoted>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<quoted_num_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::quoted>>(value.val, ctx, args...);
         }
      };

      template <class T>
      struct from_json<unquote_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            static thread_local std::string s{};
            read<json>::op<Opts>(s, ctx, args...);
            auto pe = glz::read<Opts>(value.val, s);
            if (pe) {
               ctx.error = pe.ec;
            }
         }
      };

      template <class T>
      struct to_json<unquote_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            static thread_local std::string s{};
            glz::write<Opts>(value.val, s);
            write<json>::op<Opts>(s, ctx, args...);
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
      
      template <auto MemPtr>
      inline constexpr decltype(auto) quoted_num_impl() noexcept
      {
         return [](auto&& val) { return quoted_num_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }

      template <auto MemPtr>
      inline constexpr decltype(auto) unquote_impl() noexcept
      {
         return [](auto&& val) { return unquote_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   template <auto MemPtr>
   constexpr auto quoted_num = detail::quoted_num_impl<MemPtr>();

   template <auto MemPtr>
   inline constexpr decltype(auto) number() noexcept
   {
      return [](auto&& val) { return number_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }

   template <auto MemPtr>
   constexpr auto unquote = detail::unquote_impl<MemPtr>();
}
