// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

namespace glz
{
   namespace detail
   {
      template <class T>
      concept local_meta_t = requires
      {
         std::decay_t<T>::glaze::value;
      };
   }
   
   template <class T>
   struct meta
   {};
   
   template <class T>
   inline constexpr auto meta_wrapper_v = [] {
      using V = std::decay_t<T>;
      if constexpr (detail::local_meta_t<V>) {
         return V::glaze::value;
      }
      else {
         return meta<V>::value;
      }
   }();
   
   template <class T>
   inline constexpr auto meta_v = meta_wrapper_v<T>.value;

   template <class T>
   using meta_t = std::decay_t<decltype(meta_v<T>)>;
   
   template <class T>
   using meta_wrapper_t = std::decay_t<decltype(meta_wrapper_v<T>)>;
}
