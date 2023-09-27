// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // invoker_t is intended to cause a funtion invocation when read
   template <class From, class To>
   struct custom_t;

   template <class From, class To>
      requires(!std::is_member_function_pointer_v<From> && !std::is_member_function_pointer_v<To>)
   struct custom_t<From, To> final
   {
      using from_t = From;
      using to_t = To;
      From& from;
      To& to;
   };

   template <class From, class To>
      requires(std::is_member_function_pointer_v<From> && std::is_member_function_pointer_v<To>)
   struct custom_t<From, To> final
   {
      using from_t = From;
      using to_t = To;
      typename parent_of_fn<From>::type& val;
      From from;
      To to;
   };
   
   template <class From, class To>
      requires(!std::is_member_function_pointer_v<From> && std::is_member_function_pointer_v<To>)
   struct custom_t<From, To> final
   {
      using from_t = From;
      using to_t = To;
      typename parent_of_fn<To>::type& val;
      From& from;
      To to;
   };
   
   template <class From, class To>
      requires(std::is_member_function_pointer_v<From> && !std::is_member_function_pointer_v<To>)
   struct custom_t<From, To> final
   {
      using from_t = From;
      using to_t = To;
      typename parent_of_fn<From>::type& val;
      From from;
      To& to;
   };
   
   template <class T>
   concept is_custom = requires{
      typename T::from_t;
      typename T::to_t;
   };

   namespace detail
   {
      template <is_custom T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using From = typename V::from_t;
            
            if constexpr (std::is_member_function_pointer_v<From>) {
               using Ret = typename return_type<From>::type;
               if constexpr (std::is_void_v<Ret>) {
                  using Tuple = typename inputs_as_tuple<From>::type;
                  if constexpr (std::tuple_size_v<Tuple> == 0) {
                     skip_array<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     (value.val.*value.from)();
                  }
                  else if constexpr (std::tuple_size_v<Tuple> == 1) {
                     std::decay_t<std::tuple_element_t<0, Tuple>> input{};
                     read<json>::op<Opts>(input, ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     (value.val.*value.from)(input);
                  }
                  else {
                     static_assert(false_v<T>, "function cannot have more than one input");
                  }
               }
               else {
                  static_assert(false_v<T>, "function must have void return");
               }
            }
            else if constexpr (is_specialization_v<From, std::function>) {
               using Ret = typename function_traits<From>::result_type;

               if constexpr (std::is_void_v<Ret>) {
                  using Tuple = typename function_traits<From>::arguments;
                  if constexpr (std::tuple_size_v<Tuple> == 0) {
                     skip_array<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     value.from();
                  }
                  else {
                     Tuple inputs{};
                     read<json>::op<Opts>(inputs, ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     std::apply(value.from, inputs);
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

      template <is_custom T>
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using To = typename V::to_t;
            
            if constexpr (is_specialization_v<To, std::function>) {
               using Ret = typename function_traits<To>::result_type;

               if constexpr (std::is_void_v<Ret>) {
                  static_assert(false_v<To>, "conversion to JSON must return a value");
               }
               else {
                  using Tuple = typename function_traits<V>::arguments;
                  Tuple inputs{};
                  write<json>::op<Opts>(inputs, ctx, args...);
               }
            }
         }
      };
   }

   template <auto From, auto To>
   inline constexpr decltype(auto) custom() noexcept
   {
      using F = decltype(From);
      using T = decltype(To);
      if constexpr (std::is_member_function_pointer_v<F> && std::is_member_function_pointer_v<T>) {
         return [](auto&& val) { return custom_t<std::decay_t<F>, std::decay_t<T>>{val, From, To}; };
      }
      else if constexpr (!std::is_member_function_pointer_v<F> && std::is_member_function_pointer_v<T>) {
         return [](auto&& val) { return custom_t<std::decay_t<decltype(val.*From)>, std::decay_t<T>>{val, val.*From, To}; };
      }
      else if constexpr (!std::is_member_function_pointer_v<F> && std::is_member_function_pointer_v<T>) {
         return [](auto&& val) { return custom_t<std::decay_t<F>, std::decay_t<decltype(val.*To)>>{val, From, val.*To}; };
      }
      else {
         return [](auto&& val) { return custom_t<std::decay_t<decltype(val.*From)>, std::decay_t<decltype(val.*To)>>{val.*From, val.*To}; };
      }
   }
}
