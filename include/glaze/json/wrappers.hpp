// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/opts.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // treat a value as quoted to avoid double parsing into a value
   template <class T>
   struct quoted_t
   {
      static constexpr bool glaze_wrapper = true;
      using value_type = T;
      T& val;
   };

   template <class T, auto OptsMemPtr>
   struct opts_wrapper_t
   {
      static constexpr bool glaze_wrapper = true;
      static constexpr auto glaze_reflect = false;
      static constexpr auto opts_member = OptsMemPtr;
      using value_type = T;
      T& val;
   };

   template <class T>
   concept is_opts_wrapper = requires {
      requires T::glaze_wrapper == true;
      requires T::glaze_reflect == false;
      T::opts_member;
      typename T::value_type;
      requires std::is_lvalue_reference_v<decltype(T::val)>;
   };

   namespace detail
   {
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

      template <is_opts_wrapper T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<opt_true<Opts, T::opts_member>>(value.val, args...);
         }
      };

      template <is_opts_wrapper T>
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, T::opts_member>>(value.val, ctx, args...);
         }
      };

      template <auto MemPtr, auto OptsMemPtr>
      inline constexpr decltype(auto) opts_wrapper() noexcept
      {
         return [](auto&& val) {
            using V = std::remove_reference_t<decltype(val.*MemPtr)>;
            return opts_wrapper_t<V, OptsMemPtr>{val.*MemPtr};
         };
      }

      template <auto MemPtr>
      inline constexpr decltype(auto) quoted_impl() noexcept
      {
         return [](auto&& val) { return quoted_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   // Read and write booleans as numbers
   template <auto MemPtr>
   constexpr auto bools_as_numbers = detail::opts_wrapper<MemPtr, &opts::bools_as_numbers>();

   // Read and write numbers as strings
   template <auto MemPtr>
   constexpr auto quoted_num = detail::opts_wrapper<MemPtr, &opts::quoted_num>();

   // Read numbers as strings and write these string as numbers
   template <auto MemPtr>
   constexpr auto number = detail::opts_wrapper<MemPtr, &opts::number>();

   // Read a value as a string and unescape, to avoid the user having to parse twice
   template <auto MemPtr>
   constexpr auto quoted = detail::quoted_impl<MemPtr>();

   // Write out string like types without quotes
   template <auto MemPtr>
   constexpr auto raw = detail::opts_wrapper<MemPtr, &opts::raw>();
   
   // Reads into only allocated memory and then exits without parsing the rest of the input
   template <auto MemPtr>
   constexpr auto read_allocated = detail::opts_wrapper<MemPtr, &opts::read_allocated>();
}
