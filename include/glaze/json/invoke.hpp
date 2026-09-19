// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // invoker_t is intended to cause a function invocation when read
   template <class T>
   struct invoke_t;

   template <class T>
      requires(!std::is_member_function_pointer_v<T>)
   struct invoke_t<T> final
   {
      T& val;
   };

   template <class T>
      requires(std::is_member_function_pointer_v<T>)
   struct invoke_t<T> final
   {
      using mem_fun = T;
      typename parent_of_fn<T>::type& val;
      mem_fun ptr;
   };

   template <class T>
   struct from<JSON, invoke_t<T>>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         using V = std::decay_t<decltype(value.val)>;

         if constexpr (std::is_member_function_pointer_v<T>) {
            using M = typename std::decay_t<decltype(value)>::mem_fun;
            using Ret = typename return_type<M>::type;

            if constexpr (std::is_void_v<Ret>) {
               using Tuple = typename inputs_as_tuple<M>::type;
               if constexpr (glz::tuple_size_v<Tuple> == 0) {
                  skip_array<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  (value.val.*value.ptr)();
               }
               else {
                  Tuple inputs{};
                  parse<JSON>::op<Opts>(inputs, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  std::apply(
                     [&](auto&&... args) { return (value.val.*value.ptr)(std::forward<decltype(args)>(args)...); },
                     inputs);
               }
            }
            else {
               static_assert(false_v<T>, "function must have void return");
            }
         }
         else if constexpr (is_specialization_v<V, std::function>) {
            using Ret = typename function_traits<V>::result_type;

            if constexpr (std::is_void_v<Ret>) {
               using Tuple = typename function_traits<V>::arguments;
               if constexpr (glz::tuple_size_v<Tuple> == 0) {
                  skip_array<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  value.val();
               }
               else {
                  Tuple inputs{};
                  parse<JSON>::op<Opts>(inputs, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  std::apply(value.val, inputs);
               }
            }
            else {
               static_assert(false_v<T>, "std::function must have void return");
            }
         }
         else {
            static_assert(false_v<T>, "type must be invocable");
         }
      }
   };

   // An invoke member is a call site rather than state, so there is nothing to serialize.
   // Exclude it from output instead of inventing a value for it.
   template <class T>
   struct to<JSON, invoke_t<T>>
   {
      template <auto Opts>
      static void op(auto&&, is_context auto&&, auto&&...)
      {
         static_assert(false_v<T>,
                       "glz::invoke members cannot be written: a function has no value to serialize. "
                       "Exclude the member from output with a meta<T>::skip(key, ctx) that returns true when "
                       "ctx.op == glz::operation::serialize (see docs/skip-keys.md).");
      }
   };

   template <auto MemPtr>
   inline constexpr decltype(auto) invoke_impl()
   {
      using V = decltype(MemPtr);
      if constexpr (std::is_member_function_pointer_v<V>) {
         return [](auto&& val) { return invoke_t<std::decay_t<V>>{val, MemPtr}; };
      }
      else {
         return [](auto&& val) { return invoke_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   template <auto MemPtr>
   constexpr auto invoke = invoke_impl<MemPtr>();
}
