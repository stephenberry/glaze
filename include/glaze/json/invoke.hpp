// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // invoker_t is intended to cause a funtion invocation when read
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
      // using F = typename std_function_signature_decayed_keep_non_const_ref<T>::type;
      using mem_fun = T;
      typename parent_of_fn<T>::type& val;
      mem_fun ptr;
   };

   namespace detail
   {
      template <class T>
      struct from_json<invoke_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<decltype(value.val)>;

            if constexpr (std::is_member_function_pointer_v<T>) {
               using M = typename std::decay_t<decltype(value)>::mem_fun;
               using Ret = typename return_type<M>::type;

               if constexpr (std::is_void_v<Ret>) {
                  using Tuple = typename inputs_as_tuple<M>::type;
                  if constexpr (std::tuple_size_v<Tuple> == 0) {
                     skip_array<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     (value.val.*value.ptr)();
                  }
                  else {
                     Tuple inputs{};
                     read<json>::op<Opts>(inputs, ctx, it, end);
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
                  if constexpr (std::tuple_size_v<Tuple> == 0) {
                     skip_array<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     value.val();
                  }
                  else {
                     Tuple inputs{};
                     read<json>::op<Opts>(inputs, ctx, it, end);
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

      template <class T>
      struct to_json<invoke_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value.val)>;
            dump<'['>(args...);
            if constexpr (is_specialization_v<V, std::function>) {
               using Ret = typename function_traits<V>::result_type;

               if constexpr (std::is_void_v<Ret>) {
                  using Tuple = typename function_traits<V>::arguments;
                  Tuple inputs{};
                  write<json>::op<Opts>(inputs, ctx, args...);
               }
               else {
                  static_assert(false_v<T>, "std::function must have void return");
               }
            }
            dump<']'>(args...);
         }
      };

      template <auto MemPtr>
      inline constexpr decltype(auto) invoke_impl() noexcept
      {
         using V = decltype(MemPtr);
         if constexpr (std::is_member_function_pointer_v<V>) {
            return [](auto&& val) { return invoke_t<std::decay_t<V>>{val, MemPtr}; };
         }
         else {
            return [](auto&& val) { return invoke_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
         }
      }
   }

   template <auto MemPtr>
   constexpr auto invoke = detail::invoke_impl<MemPtr>();
}

namespace glz
{
   template <class Signature>
      requires(std::is_void_v<typename function_traits<std::function<Signature>>::result_type>)
   struct invoke_update
   {
      invoke_update() = default;
      invoke_update(const invoke_update&) = default;
      invoke_update(invoke_update&&) = default;
      invoke_update& operator=(const invoke_update&) = default;
      invoke_update& operator=(invoke_update&&) = default;

      template <class F>
      invoke_update(F&& f) : func(std::forward<F>(f))
      {}

      std::function<Signature> func{};
      std::string prev{};
      bool initialized = false;
      static constexpr auto glaze = true;
   };

   template <class T>
   concept is_invoke_update = requires {
      T::func;
      T::prev;
      T::initialized;
      T::glaze;
   };

   namespace detail
   {
      template <is_invoke_update T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<decltype(value.func)>;

            using Tuple = typename function_traits<V>::arguments;
            if constexpr (std::tuple_size_v<Tuple> == 0) {
               auto start = it;
               skip_array<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               const sv input = {start, size_t(it - start)};
               if (value.initialized) {
                  if (input != value.prev) {
                     value.func();
                  }
               }
               else {
                  value.initialized = true;
               }
               value.prev = input;
            }
            else {
               auto start = it;
               skip_array<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               const sv input = {start, size_t(it - start)};
               if (value.initialized) {
                  if (input != value.prev) {
                     Tuple inputs{};
                     it = start;
                     read<json>::op<Opts>(inputs, ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     std::apply(value.func, inputs);
                  }
               }
               else {
                  value.initialized = true;
               }
               value.prev = input;
            }
         }
      };

      template <is_invoke_update T>
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value.val)>;
            dump<'['>(args...);
            using Tuple = typename function_traits<V>::arguments;
            Tuple inputs{};
            write<json>::op<Opts>(inputs, ctx, args...);
            dump<']'>(args...);
         }
      };
   }
}
