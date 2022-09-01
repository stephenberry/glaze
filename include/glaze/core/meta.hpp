// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>

namespace glz
{   
   template <class T>
   struct meta
   {};

   namespace detail
   {
      template <class T>
      concept local_meta_t = requires
      {
         T::glaze::value;
      };

      template <class T>
      concept global_meta_t = requires
      {
         meta<T>::value;
      };
   }

   struct empty
   {
      static constexpr std::tuple<> value{};
   };
   
   template <class T>
   inline constexpr auto meta_wrapper_v = [] {
      if constexpr (detail::local_meta_t<T>) {
         return T::glaze::value;
      }
      else if constexpr (detail::global_meta_t<T>) {
         return meta<T>::value;
      }
      else {
         return empty{};
      }
   }();
   
   template <class T>
   inline constexpr auto meta_v = meta_wrapper_v<std::decay_t<T>>.value;

   template <class T>
   using meta_t = std::decay_t<decltype(meta_v<T>)>;
   
   template <class T>
   using meta_wrapper_t = std::decay_t<decltype(meta_wrapper_v<std::decay_t<T>>)>;
   
   template <class T>
   concept named = requires {
      meta<T>::name;
   };
   
   template <named T>
   inline constexpr std::string_view name_v = [] {
      if constexpr (detail::local_meta_t<T>) {
         return T::glaze::name;
      }
      else {
         return meta<T>::name;
      }
   }();
   
   using version_t = std::array<uint32_t, 3>;
   
   template <class T>
   concept versioned = requires {
      meta<std::decay_t<T>>::version;
   };
   
   template <class T>
   inline constexpr version_t version = []() -> version_t {
      if constexpr (versioned<T>) {
         return meta<std::decay_t<T>>::version;
      }
      else {
         return { 0, 0, 1 };
      }
   }();
}
