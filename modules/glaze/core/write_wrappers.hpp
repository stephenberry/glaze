// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "glaze/core/common.hpp" // glaze_value_t, get_member, context
#include "glaze/core/meta.hpp" // remove_meta_wrapper_t, meta_wrapper_v, custom_write
#include "glaze/util/type_traits.hpp" // is_specialization_v, return_type, function_traits

namespace glz
{
   // === Transparent write wrappers (see issue #2595) ===
   // A struct field may be exposed through a wrapper that the value serializer transparently
   // "sees through": a glaze_value_t "mimic" (a glz::meta whose `value` is a single member
   // pointer) or a glz::custom getter/setter. JSON resolves these at the value-serializer
   // level, so its output is already correct. The TOML and YAML writers, however, must also
   // classify each field by the shape it serializes as (scalar vs. object/array/map) to pick
   // a layout. Classifying by the wrapper type files a wrapped object/array as a scalar and
   // emits malformed output. These helpers report the shape a field actually serializes as,
   // and unwrap to the underlying value, so the block writers can lay it out correctly.
   //
   // These live in their own header (rather than core/reflect.hpp) because only the TOML and
   // YAML block writers need them; keeping them out of reflect.hpp avoids parsing this
   // TOML/YAML-only machinery in every translation unit that includes the reflection core.

   // A wrapper the value serializer sees through, but the block writers must look past to
   // classify a field's serialized shape. glaze_value_t excludes object/array/enum metas, so
   // a true mimic only wraps a single inner value; custom_t is the glz::custom getter/setter.
   template <class V>
   concept transparent_write_wrapper =
      (glaze_value_t<V> && !custom_write<V>) || is_specialization_v<std::remove_cvref_t<V>, custom_t>;

   // Decayed return type of a custom_t getter (the `To` member), invoked on its parent.
   // Mirrors the getter dispatch in to<Format, custom_t> and custom_getter_returns_nullable.
   template <class V>
   struct custom_getter_result
   {
      using type = V;
   };

   template <class T, class From, class To>
   struct custom_getter_result<custom_t<T, From, To>>
   {
      using ParentT = std::remove_reference_t<T>;
      static constexpr auto compute()
      {
         if constexpr (std::is_member_pointer_v<To>) {
            if constexpr (std::is_member_function_pointer_v<To>) {
               return std::type_identity<std::decay_t<typename return_type<To>::type>>{};
            }
            else if constexpr (std::is_member_object_pointer_v<To>) {
               using Value = std::decay_t<decltype(std::declval<ParentT&>().*(std::declval<To>()))>;
               if constexpr (is_specialization_v<Value, std::function>) {
                  return std::type_identity<std::decay_t<typename function_traits<Value>::result_type>>{};
               }
               else {
                  return std::type_identity<Value>{};
               }
            }
            else {
               return std::type_identity<custom_t<T, From, To>>{};
            }
         }
         else if constexpr (std::invocable<To, ParentT&>) {
            return std::type_identity<std::decay_t<std::invoke_result_t<To, ParentT&>>>{};
         }
         else if constexpr (std::invocable<To, const ParentT&>) {
            return std::type_identity<std::decay_t<std::invoke_result_t<To, const ParentT&>>>{};
         }
         else if constexpr (std::invocable<To, ParentT&, context&>) {
            return std::type_identity<std::decay_t<std::invoke_result_t<To, ParentT&, context&>>>{};
         }
         else {
            return std::type_identity<custom_t<T, From, To>>{};
         }
      }
      using type = typename decltype(compute())::type;
   };

   // Resolve, at compile time, the type a field actually serializes as by recursively
   // looking through transparent write wrappers (mimic glaze_value_t and glz::custom getters).
   template <class V>
   struct resolve_write_type
   {
      using type = V;
   };

   template <class V>
      requires(glaze_value_t<V> && !custom_write<V>)
   struct resolve_write_type<V>
   {
      using type = typename resolve_write_type<remove_meta_wrapper_t<V>>::type;
   };

   template <class T, class From, class To>
   struct resolve_write_type<custom_t<T, From, To>>
   {
      using type = typename resolve_write_type<typename custom_getter_result<custom_t<T, From, To>>::type>::type;
   };

   template <class V>
   using resolve_write_type_t = typename resolve_write_type<std::remove_cvref_t<V>>::type;

   // Invoke `func` with the value a field actually serializes, recursively unwrapping any
   // transparent write wrappers. Continuation-passing keeps a by-value custom getter result
   // alive for the duration of `func` (a getter may return a value rather than a reference).
   template <class V, class Ctx, class Func>
   GLZ_ALWAYS_INLINE void unwrap_write_value(V&& v, Ctx&& ctx, Func&& func)
   {
      using T = std::remove_cvref_t<V>;
      if constexpr (glaze_value_t<T> && !custom_write<T>) {
         unwrap_write_value(get_member(std::forward<V>(v), meta_wrapper_v<T>), ctx, std::forward<Func>(func));
      }
      else if constexpr (is_specialization_v<T, custom_t>) {
         using To = typename T::to_t;
         if constexpr (std::is_member_pointer_v<To>) {
            if constexpr (std::is_member_function_pointer_v<To>) {
               unwrap_write_value((v.val.*(v.to))(), ctx, std::forward<Func>(func));
            }
            else if constexpr (std::is_member_object_pointer_v<To>) {
               auto&& to = v.val.*(v.to);
               using ToVal = std::decay_t<decltype(to)>;
               if constexpr (is_specialization_v<ToVal, std::function>) {
                  unwrap_write_value(to(), ctx, std::forward<Func>(func));
               }
               else {
                  unwrap_write_value(to, ctx, std::forward<Func>(func));
               }
            }
            else {
               func(std::forward<V>(v));
            }
         }
         else if constexpr (std::invocable<To, decltype((v.val))>) {
            unwrap_write_value(std::invoke(v.to, v.val), ctx, std::forward<Func>(func));
         }
         else if constexpr (std::invocable<To, decltype((v.val)), context&>) {
            unwrap_write_value(std::invoke(v.to, v.val, ctx), ctx, std::forward<Func>(func));
         }
         else {
            func(std::forward<V>(v));
         }
      }
      else {
         func(std::forward<V>(v));
      }
   }
}
