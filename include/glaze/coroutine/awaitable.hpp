// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <concepts>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace glz
{
   template <class type, class... types>
   concept in_types = (std::same_as<type, types> || ...);

   /**
    * This concept declares a type that is required to meet the c++20 coroutine operator co_await()
    * retun type.  It requires the following three member functions:
    *      await_ready() -> bool
    *      await_suspend(std::coroutine_handle<>) -> void|bool|std::coroutine_handle<>
    *      await_resume() -> decltype(auto)
    *          Where the return type on await_resume is the requested return of the awaitable.
    */
   template <class type>
   concept awaiter = requires(type t, std::coroutine_handle<> c) {
                        {
                           t.await_ready()
                        } -> std::same_as<bool>;
                        {
                           t.await_suspend(c)
                        } -> in_types<void, bool, std::coroutine_handle<>>;
                        {
                           t.await_resume()
                        };
                     };

   template <class type>
   concept member_co_await_awaitable = requires(type t) {
                                          {
                                             t.operator co_await()
                                          } -> awaiter;
                                       };

   template <class type>
   concept global_co_await_awaitable = requires(type t) {
                                          {
                                             operator co_await(t)
                                          } -> awaiter;
                                       };

   /**
    * This concept declares a type that can be operator co_await()'ed and returns an awaiter_type.
    */
   template <class type>
   concept awaitable = member_co_await_awaitable<type> || global_co_await_awaitable<type> || awaiter<type>;

   template <class type>
   concept awaiter_void = awaiter<type> && requires(type t) {
                                              {
                                                 t.await_resume()
                                              } -> std::same_as<void>;
                                           };

   template <class type>
   concept member_co_await_awaitable_void = requires(type t) {
                                               {
                                                  t.operator co_await()
                                               } -> awaiter_void;
                                            };

   template <class type>
   concept global_co_await_awaitable_void = requires(type t) {
                                               {
                                                  operator co_await(t)
                                               } -> awaiter_void;
                                            };

   template <class type>
   concept awaitable_void =
      member_co_await_awaitable_void<type> || global_co_await_awaitable_void<type> || awaiter_void<type>;

   template <awaitable awaitable, class = void>
   struct awaitable_traits
   {};

   template <awaitable awaitable>
   static auto get_awaiter(awaitable&& value)
   {
      if constexpr (member_co_await_awaitable<awaitable>)
         return std::forward<awaitable>(value).operator co_await();
      else if constexpr (global_co_await_awaitable<awaitable>)
         return operator co_await(std::forward<awaitable>(value));
      else if constexpr (awaiter<awaitable>) {
         return std::forward<awaitable>(value);
      }
   }

   template <awaitable awaitable>
   struct awaitable_traits<awaitable>
   {
      using type = decltype(get_awaiter(std::declval<awaitable>()));
      using return_type = decltype(std::declval<type>().await_resume());
   };
}
