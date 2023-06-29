// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json.hpp"

namespace glz
{
   // invoker_t is intended to cause a funtion invocation when read
   template <class T>
   struct invoker_t
   {
      T& val;
   };

   namespace detail
   {
      template <class T>
      struct from_json<invoker_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<decltype(value.val)>;
            
            
            /*if constexpr (std::is_member_function_pointer_v<V>) {
               using F = typename std_function_signature_decayed_keep_non_const_ref<V>::type;
               using Ret = typename return_type<V>::type;
               using Tuple = typename inputs_as_tuple<V>::type;
               static constexpr auto N = std::tuple_size_v<Tuple>;

               if constexpr (std::is_void_v<Ret>) {
                  detail::call_args<Tuple>(std::mem_fn(val), parent, args, std::make_index_sequence<N>{});
               }
               else {
                  ctx.error = error_code::attempt_member_func_read;
               }
            }*/
            
            if constexpr (is_specialization_v<V, std::function>) {
               using Ret = typename function_traits<V>::result_type;
               
               if constexpr (std::is_void_v<Ret>)
               {
                  using Tuple = typename function_traits<V>::arguments;                  
                  Tuple inputs{};
                  read<json>::op<Opts>(inputs, ctx, it, end);
                  std::apply(value.val, inputs);
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
      struct to_json<invoker_t<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value.val)>;
            dump<'['>(args...);
            if constexpr (is_specialization_v<V, std::function>) {
               using Ret = typename function_traits<V>::result_type;
               
               if constexpr (std::is_void_v<Ret>)
               {
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
   }

   template <auto MemPtr>
   inline constexpr decltype(auto) invoker() noexcept
   {
      return [](auto&& val) { return invoker_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }
}
