// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/variant.hpp"
#include "glaze/util/for_each.hpp"

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
      
      template <class T>
      concept glaze_t = requires
      {
         meta<std::decay_t<T>>::value;
      }
      || local_meta_t<std::decay_t<T>>;
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

   template <class T>
   concept tagged = requires
   {
      meta<T>::tag;
   }
   || requires { T::glaze::tag; };

   template <class T>
   concept ided = requires
   {
      meta<T>::ids;
   }
   || requires { T::glaze::ids; };

   template <class T>
   inline constexpr std::string_view tag_v = [] {
      if constexpr (tagged<T>) {
         if constexpr (detail::local_meta_t<T>) {
            return T::glaze::tag;
         }
         else {
            return meta<T>::tag;
         }
      }
      else {
         return "";
      }
   }();

   template <is_variant T>
   inline constexpr auto ids_v = [] {
      if constexpr (ided<T>) {
         if constexpr (detail::local_meta_t<T>) {
            return T::glaze::ids;
         }
         else {
            return meta<T>::ids;
         }
      }
      else {
         constexpr auto N = std::variant_size_v<T>;
         std::array<std::string_view, N> ids{};
         for_each<N>([&](auto I) { ids[I] = glz::name_v<std::decay_t<std::variant_alternative_t<I, T>>>; });
         return ids;
      }
   }();
   
   template <auto Enum> requires (std::is_enum_v<decltype(Enum)>)
   inline constexpr std::string_view enum_name_v = []() -> std::string_view {
      
      using T = std::decay_t<decltype(Enum)>;
      
      if constexpr (detail::glaze_t<T>) {
         using U = std::underlying_type_t<T>;
         return glz::tuplet::get<0>(glz::tuplet::get<static_cast<U>(Enum)>(meta_v<T>));
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
