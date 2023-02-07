// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{   
   template <class T>
   struct meta
   {};

   namespace detail
   {
      template <class T>
      concept local_construct_t = requires
      {
         T::glaze::construct;
      };
      
      template <class T>
      concept global_construct_t = requires
      {
         meta<T>::construct;
      };
      
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
      static constexpr glz::tuplet::tuple<> value{};
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
   inline constexpr auto meta_construct_v = [] {
      if constexpr (detail::local_construct_t<T>) {
         return T::glaze::construct;
      }
      else if constexpr (detail::global_construct_t<T>) {
         return meta<T>::construct;
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
   }
   || requires {
      T::glaze::name;
   };
   
   template <class T, bool fail_on_unknown = false>
   inline constexpr std::string_view name_v = [] {

      if constexpr (named<T>) {
         if constexpr (detail::local_meta_t<T>) {
            if constexpr (fail_on_unknown) {
               static_assert(T::glaze::name.find("glz::unknown") == std::string_view::npos, "name_v used on unnamed type");
            }
            return T::glaze::name;
         }
         else {
            if constexpr (fail_on_unknown) {
               static_assert(meta<T>::name.find("glz::unknown") == std::string_view::npos, "name_v used on unnamed type");
            }
            return meta<T>::name;
         }
      }
      else if constexpr (std::is_void_v<T>) {
         return "void";
      }
      else if constexpr (fail_on_unknown) {
         static_assert(false_v<T>, "name_v used on unnamed type");
      }
      else {
         return "glz::unknown";
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
