// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

namespace glz
{
   template<class T> struct false_t : std::false_type {};
   namespace detail {
       struct aggressive_unicorn_type; // Do not unleash
   }
   template<> struct false_t<detail::aggressive_unicorn_type> : std::true_type {};
   
   template <class T>
   inline constexpr bool false_v = false_t<T>::value;
   
   // from
   // https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector
   template <class, template<class...> class>
   inline constexpr bool is_specialization_v = false;
   
   template <template<class...> class T, class... Args>
   inline constexpr bool is_specialization_v<T<Args...>, T> = true;
   
   // Member object and function pointer type traits
   template <class T>
   struct function_signature;
   
   template <class ClassType, class T>
   struct function_signature<T ClassType::*>
   {
      using type = T*;
   };
   
   template <class ClassType, class Result, class... Args>
   struct function_signature<Result(ClassType::*)(Args...)>
   {
      using type = Result(*)(void*, Args...);
   };
   
   template <class T>
   struct std_function_signature;
   
   template <class ClassType, class Result, class... Args>
   struct std_function_signature<Result(ClassType::*)(Args...)>
   {
      using type = std::function<Result(Args...)>;
   };
   
   template <class T>
   struct return_type;
   
   template <class ClassType, class Result, class... Args>
   struct return_type<Result(ClassType::*)(Args...)>
   {
      using type = Result;
   };
   
   template <class T>
   struct inputs_as_tuple;
   
   template <class ClassType, class Result, class... Args>
   struct inputs_as_tuple<Result(ClassType::*)(Args...)>
   {
      using type = std::tuple<Args...>;
   };
   
   template <class T>
   struct parent_of_fn;
   
   template <class ClassType, class Result, class... Args>
   struct parent_of_fn<Result(ClassType::*)(Args...)>
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
   struct arguments<MemPtr, R(T::*)(Args...)>
   {
      static constexpr decltype(auto) op(void* ptr, Args&&... args) {
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
}
