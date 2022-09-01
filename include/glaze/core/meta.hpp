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
         T::glaze::value;
      };
   }
   
   template <class T>
   struct meta
   {};
   
   // Do not decay T here for sake of name
   template <class T>
   inline constexpr auto meta_wrapper_v = [] {
      if constexpr (detail::local_meta_t<T>) {
         return T::glaze::value;
      }
      else {
         return meta<T>::value;
      }
   }();
   
   template <class T>
   inline constexpr auto meta_v = meta_wrapper_v<std::decay_t<T>>.value;

   template <class T>
   using meta_t = std::decay_t<decltype(meta_v<T>)>;
   
   template <class T>
   using meta_wrapper_t = std::decay_t<decltype(meta_wrapper_v<std::decay_t<T>>)>;
   
   /*template <class T>
   concept named = requires {
      name_t<T>::value;
   };
   
   template <named T>
   inline constexpr std::string_view name = meta_wrapper_v<T>.name;*/
}
