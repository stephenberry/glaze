// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/format.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // treat numbers as quoted or array-like types as having quoted numbers
   template <class T>
   struct quoted_num_t
   {
      T& val;
   };

   // treat a value as quoted to avoid double parsing into a value
   template <class T>
   struct quoted_t
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
            read<json>::op<opt_true<Opts, &opts::quoted_num>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<quoted_num_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::quoted_num>>(value.val, ctx, args...);
         }
      };

      template <class T>
      struct from_json<quoted_t<T>>
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
      struct to_json<quoted_t<T>>
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

      template <class T>
      struct to_json<raw_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::raw>>(value.val, args...);
         }
      };

      template <auto MemPtr>
      inline constexpr decltype(auto) quoted_num_impl() noexcept
      {
         return [](auto&& val) { return quoted_num_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }

      template <auto MemPtr>
      inline constexpr decltype(auto) number_impl() noexcept
      {
         return [](auto&& val) { return number_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }

      template <auto MemPtr>
      inline constexpr decltype(auto) quoted_impl() noexcept
      {
         return [](auto&& val) { return quoted_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }

      template <auto MemPtr>
      inline constexpr decltype(auto) raw_impl() noexcept
      {
         return [](auto&& val) { return raw_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   template <auto MemPtr>
   constexpr auto quoted_num = detail::quoted_num_impl<MemPtr>();

   template <auto MemPtr>
   constexpr auto number = detail::number_impl<MemPtr>();

   template <auto MemPtr>
   constexpr auto quoted = detail::quoted_impl<MemPtr>();

   template <auto MemPtr>
   constexpr auto raw = detail::raw_impl<MemPtr>();
}
