#pragma once

#include "tags.hpp"

namespace glz::eetf
{

   namespace detail
   {

      // Primary template
      template <typename Tag>
      struct in_impl;

      // Specialization for `int...`
      template <eetf_tag N, eetf_tag... Vs>
      struct in_impl<std::integer_sequence<eetf_tag, N, Vs...>>
      {
         bool value{false};

         template <class T>
         constexpr in_impl(const T& val) : value{(val == N) || in_impl<std::integer_sequence<eetf_tag, Vs...>>(val).value}
         {}
      };

      template <eetf_tag N>
      struct in_impl<std::integer_sequence<eetf_tag, N>>
      {
         bool value{false};

         template <class T>
         constexpr in_impl(const T& val) : value{val == N}
         {}
      };

   } // namespace detail

   template <typename T>
   using in = detail::in_impl<T>;

   namespace cmp
   {

      template <template <class> class Op, eetf_tag... Vs, typename T>
      constexpr bool is(const T& val)
      {
         return Op<std::integer_sequence<eetf_tag, Vs...>>(val).value;
      }

   };

} // namespace erlterm
