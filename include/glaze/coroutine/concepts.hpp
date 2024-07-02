// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <concepts>
#include <ranges>

#include "glaze/coroutine/awaitable.hpp"

namespace glz
{
   /**
    * Concept to require that the range contains a specific type of value.
    */
   template <class T, class V>
   concept range_of = std::ranges::range<T> && std::is_same_v<V, std::ranges::range_value_t<T>>;

   /**
    * Concept to require that a sized range contains a specific type of value.
    */
   template <class T, class V>
   concept sized_range_of = std::ranges::sized_range<T> && std::is_same_v<V, std::ranges::range_value_t<T>>;

   template <class T>
   concept executor = requires(T t, std::coroutine_handle<> c) {
                         {
                            t.schedule()
                         } -> awaiter;
                         {
                            t.yield()
                         } -> awaiter;
                         {
                            t.resume(c)
                         } -> std::same_as<bool>;
                      };

   template <class T>
   concept io_exceutor = executor<T>;/* and requires(T t, std::coroutine_handle<> c, net::file_handle_t fd, glz::poll_op op,
                                                     std::chrono::milliseconds timeout) {
                                               {
                                                  t.poll(fd, op, timeout)
                                               } -> std::same_as<glz::task<poll_status>>;
                                            };*/
   
   template <class T>
   concept const_buffer = requires(const T t)
   {
       { t.empty() } -> std::same_as<bool>;
       { t.data() } -> std::same_as<const char*>;
       { t.size() } -> std::same_as<size_t>;
   };

   template <class T>
   concept mutable_buffer = requires(T t)
   {
       { t.empty() } -> std::same_as<bool>;
       { t.data() } -> std::same_as<char*>;
       { t.size() } -> std::same_as<size_t>;
   };
}
