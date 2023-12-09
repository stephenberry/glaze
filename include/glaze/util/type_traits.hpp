// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <functional>
#include <tuple>

namespace glz
{
   template <class... Args>
   struct false_t : std::false_type
   {};
   namespace detail
   {
      struct aggressive_unicorn_type; // Do not unleash
   }
   template <>
   struct false_t<detail::aggressive_unicorn_type> : std::true_type
   {};

   template <class... Args>
   inline constexpr bool false_v = false_t<Args...>::value;

   // from
   // https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector
   template <class, template <class...> class>
   inline constexpr bool is_specialization_v = false;

   template <template <class...> class T, class... Args>
   inline constexpr bool is_specialization_v<T<Args...>, T> = true;

   template <class T>
   struct member_value;

   template <class ClassType, class T>
   struct member_value<T ClassType::*>
   {
      using type = T;
   };

   // for std::function
   template <class T>
   struct function_traits;

   template <class R, class... Args>
   struct function_traits<std::function<R(Args...)>>
   {
      static constexpr size_t N = sizeof...(Args);
      using result_type = R;
      using arguments = std::tuple<Args...>;
   };

   // Member object and function pointer type traits
   template <class T>
   struct function_signature;

   template <class ClassType, class T>
   struct function_signature<T ClassType::*>
   {
      using type = T*;
   };

   template <class ClassType, class Result, class... Args>
   struct function_signature<Result (ClassType::*)(Args...)>
   {
      using type = Result (*)(void*, std::decay_t<Args>&&...);
   };

   template <class ClassType, class Result, class... Args>
   struct function_signature<Result (ClassType::*)(Args...) const>
   {
      using type = Result (*)(void*, std::decay_t<Args>&&...);
   };

   template <class T>
   struct std_function_signature;

   template <class ClassType, class Result, class... Args>
   struct std_function_signature<Result (ClassType::*)(Args...)>
   {
      using type = std::function<Result(Args...)>;
   };

   template <class ClassType, class Result, class... Args>
   struct std_function_signature<Result (ClassType::*)(Args...) const>
   {
      using type = std::function<Result(Args...)>;
   };

   template <class T>
   struct keep_non_const_ref
   {
      using type = std::conditional_t<!std::is_const_v<std::remove_reference_t<T>> && std::is_lvalue_reference_v<T>,
                                      std::add_lvalue_reference_t<std::decay_t<T>>, std::decay_t<T>>;
   };

   template <class T>
   using keep_non_const_ref_t = typename keep_non_const_ref<T>::type;

   template <class T>
   struct std_function_signature_decayed_keep_non_const_ref;

   template <class ClassType, class Result, class... Args>
   struct std_function_signature_decayed_keep_non_const_ref<Result (ClassType::*)(Args...)>
   {
      using type = std::function<Result(keep_non_const_ref_t<Args>...)>;
   };

   template <class ClassType, class Result, class... Args>
   struct std_function_signature_decayed_keep_non_const_ref<Result (ClassType::*)(Args...) const>
   {
      using type = std::function<Result(keep_non_const_ref_t<Args>...)>;
   };

   template <class T>
   struct return_type;

   template <class ClassType, class Result, class... Args>
   struct return_type<Result (ClassType::*)(Args...)>
   {
      using type = Result;
   };

   template <class ClassType, class Result, class... Args>
   struct return_type<Result (ClassType::*)(Args...) const>
   {
      using type = Result;
   };

   template <class T>
   struct inputs_as_tuple;

   template <class ClassType, class Result, class... Args>
   struct inputs_as_tuple<Result (ClassType::*)(Args...)>
   {
      using type = std::tuple<Args...>;
   };

   template <class ClassType, class Result, class... Args>
   struct inputs_as_tuple<Result (ClassType::*)(Args...) const>
   {
      using type = std::tuple<Args...>;
   };

   template <class T>
   struct parent_of_fn;

   template <class ClassType, class Result, class... Args>
   struct parent_of_fn<Result (ClassType::*)(Args...)>
   {
      using type = ClassType;
   };

   template <class ClassType, class Result, class... Args>
   struct parent_of_fn<Result (ClassType::*)(Args...) const>
   {
      using type = ClassType;
   };

   template <auto MemPtr, class T>
   struct arguments;

   template <auto MemPtr, class ClassType, class T>
   struct arguments<MemPtr, T ClassType::*>
   {
      using type = T*;
   };

   template <auto MemPtr, class T, class R, class... Args>
   struct arguments<MemPtr, R (T::*)(Args...)>
   {
      static constexpr auto op(void* ptr, std::decay_t<Args>&&... args)
         -> std::invoke_result_t<decltype(std::mem_fn(MemPtr)), T, Args...>
      {
         return (reinterpret_cast<T*>(ptr)->*MemPtr)(std::forward<Args>(args)...);
      }
   };

   template <auto MemPtr, class T, class R, class... Args>
   struct arguments<MemPtr, R (T::*)(Args...) const>
   {
      static constexpr auto op(void* ptr, std::decay_t<Args>&&... args)
         -> std::invoke_result_t<decltype(std::mem_fn(MemPtr)), T, Args...>
      {
         return (reinterpret_cast<T*>(ptr)->*MemPtr)(std::forward<Args>(args)...);
      }
   };

   template <auto MemPtr>
   inline constexpr auto get_argument()
   {
      using Type = std::decay_t<decltype(MemPtr)>;
      if constexpr (std::is_member_function_pointer_v<Type>) {
         return arguments<MemPtr, Type>::op;
      }
      else {
         return typename arguments<MemPtr, Type>::type{};
      }
   }

   template <class>
   struct lambda_traits : std::false_type
   {};

   template <class R, class T, class... Args>
   struct lambda_traits<R (T::*)(Args...) const> : std::true_type
   {
      using result_type = R;
      using arguments = std::tuple<Args...>;
      using object_type = T;
   };

   template <class R, class T, class... Args>
   struct lambda_traits<R (T::*)(Args...)> : std::true_type
   {
      using result_type = R;
      using arguments = std::tuple<Args...>;
      using object_type = T;
   };

   template <class T>
   using lambda_args_t = typename lambda_traits<decltype(&T::operator())>::arguments;

   template <class T>
   using lambda_result_t = typename lambda_traits<decltype(&T::operator())>::result_type;

   // checks if type is a lambda with all known arguments
   template <class T>
   concept is_lambda_concrete = requires { typename lambda_traits<decltype(&T::operator())>::result_type; };
}
