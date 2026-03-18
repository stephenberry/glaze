// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.json.raw_string;

import std;

import glaze.core.context;
import glaze.core.opts;

import glaze.json.read;
import glaze.json.write;

#include "glaze/util/inline.hpp"

export namespace glz
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

   template <class T>
   struct from<JSON, raw_string_t<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args)
      {
         parse<JSON>::op<opt_true<Opts, raw_string_opt_tag{}>>(value.val, args...);
      }
   };

   template <class T>
   struct to<JSON, raw_string_t<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         using val_t = std::remove_cvref_t<decltype(value.val)>;
         to<JSON, val_t>::template op<opt_true<Opts, raw_string_opt_tag{}>>(value.val, ctx, args...);
      }
   };

   template <class T>
   struct from<JSON, escaped_t<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args)
      {
         parse<JSON>::op<opt_false<Opts, raw_string_opt_tag{}>>(value.val, args...);
      }
   };

   template <class T>
   struct to<JSON, escaped_t<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         using val_t = std::remove_cvref_t<decltype(value.val)>;
         to<JSON, val_t>::template op<opt_false<Opts, raw_string_opt_tag{}>>(value.val, ctx, args...);
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

   template <auto MemPtr>
   constexpr auto raw_string = raw_string_impl<MemPtr>();

   template <auto MemPtr>
   constexpr auto escaped = escaped_impl<MemPtr>();
}
