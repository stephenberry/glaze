// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/tuplet/tuple.hpp"

namespace glz
{
   // `cast` allows a user to register a type that Glaze will deserialize/serialize to
   // and then cast to the underlying value
   // glz::cast<&T::integer, double>
   // ^^^ This example would read and write the integer as a double
   template <class T, auto Target, class CastType>
   struct cast_t
   {
      static constexpr auto glaze_reflect = false;
      using target_t = decltype(Target);
      using cast_type = CastType;
      T& val;
      static constexpr auto target = Target;
   };

   template <class T>
   concept is_cast = requires {
      requires !T::glaze_reflect;
      typename T::target_t;
      typename T::cast_type;
   };

   template <uint32_t Format, is_cast T>
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         using V = std::decay_t<decltype(value)>;
         using Target = typename V::target_t;
         using Cast = typename V::cast_type;

         Cast temp{};
         parse<Format>::template op<Opts>(temp, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;

         if constexpr (std::is_member_object_pointer_v<Target>) {
            auto& field = value.val.*(value.target);
            using Field = std::remove_cvref_t<decltype(field)>;
            field = static_cast<Field>(temp);
         }
         else if constexpr (std::invocable<Target, decltype(value.val)>) {
            auto& field = value.target(value.val);
            using Field = std::remove_cvref_t<decltype(field)>;
            field = static_cast<Field>(temp);
         }
         else {
            static_assert(false_v<Target>, "invalid type for cast_t");
         }
      }
   };

   template <uint32_t Format, is_cast T>
   struct to<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         using V = std::decay_t<decltype(value)>;
         using Target = typename V::target_t;
         using Cast = typename V::cast_type;

         if constexpr (std::is_member_object_pointer_v<Target>) {
            serialize<Format>::template op<Opts>(static_cast<Cast>(value.val.*(value.target)), ctx, args...);
         }
         else if constexpr (std::invocable<Target, decltype(value.val)>) {
            serialize<Format>::template op<Opts>(static_cast<Cast>(value.target(value.val)), ctx, args...);
         }
         else {
            static_assert(false_v<T>, "invalid type for cast_t");
         }
      }
   };

   template <auto Target, class CastType>
   constexpr auto cast_impl() noexcept
   {
      return [](auto&& v) { return cast_t<std::remove_cvref_t<decltype(v)>, Target, CastType>{v}; };
   }

   template <auto Target, class CastType>
   constexpr auto cast = cast_impl<Target, CastType>();
}
