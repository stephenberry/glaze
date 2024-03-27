// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>

#include "glaze/reflection/get_name.hpp"
#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   template <class T>
   struct meta
   {};

   template <>
   struct meta<std::string_view>
   {
      static constexpr std::string_view name = "std::string_view";
   };

   template <class T>
   struct json_schema
   {};

   namespace detail
   {
      template <class T>
      struct Array
      {
         T value;
      };

      template <class T>
      Array(T) -> Array<T>;

      template <class T>
      struct Object
      {
         T value;
      };

      template <class T>
      Object(T) -> Object<T>;

      template <class T>
      struct Enum
      {
         T value;
      };

      template <class T>
      Enum(T) -> Enum<T>;

      template <class T>
      struct Flags
      {
         T value;
      };

      template <class T>
      Flags(T) -> Flags<T>;

      template <class T>
      concept local_construct_t = requires { T::glaze::construct; };

      template <class T>
      concept global_construct_t = requires { meta<T>::construct; };

      template <class T>
      concept local_meta_t = requires { T::glaze::value; };

      template <class T>
      concept global_meta_t = requires { meta<T>::value; };

      template <class T>
      concept glaze_t = requires { meta<std::decay_t<T>>::value; } || local_meta_t<std::decay_t<T>>;

      template <class T>
      concept has_unknown_writer = requires { meta<T>::unknown_write; } || requires { T::glaze::unknown_write; };

      template <class T>
      concept has_unknown_reader = requires { meta<T>::unknown_read; } || requires { T::glaze::unknown_read; };

      template <class T>
      concept local_json_schema_t = requires { typename std::decay_t<T>::glaze_json_schema; };

      template <class T>
      concept global_json_schema_t = requires { is_specialization_v<std::decay_t<T>, json_schema>; };

      template <class T>
      concept json_schema_t = local_json_schema_t<T> || global_json_schema_t<T>;
   }

   struct empty
   {
      static constexpr glz::tuplet::tuple<> value{};
   };

   template <class T>
   inline constexpr decltype(auto) meta_wrapper_v = [] {
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
   inline constexpr auto meta_v = meta_wrapper_v<decay_keep_volatile_t<T>>.value;

   template <class T>
   using meta_t = decay_keep_volatile_t<decltype(meta_v<T>)>;

   template <class T>
   using meta_wrapper_t = decay_keep_volatile_t<decltype(meta_wrapper_v<std::decay_t<T>>)>;

   template <class T>
   struct remove_meta_wrapper
   {
      using type = T;
   };
   template <detail::glaze_t T>
   struct remove_meta_wrapper<T>
   {
      using type = std::remove_pointer_t<std::remove_const_t<meta_wrapper_t<T>>>;
   };
   template <class T>
   using remove_meta_wrapper_t = typename remove_meta_wrapper<T>::type;

   template <class T>
   inline constexpr auto meta_unknown_write_v = [] {
      if constexpr (detail::local_meta_t<T>) {
         return T::glaze::unknown_write;
      }
      else if constexpr (detail::global_meta_t<T>) {
         return meta<T>::unknown_write;
      }
      else {
         return empty{};
      }
   }();

   template <class T>
   using meta_unknown_write_t = std::decay_t<decltype(meta_unknown_write_v<std::decay_t<T>>)>;

   template <class T>
   inline constexpr auto meta_unknown_read_v = [] {
      if constexpr (detail::local_meta_t<T>) {
         return T::glaze::unknown_read;
      }
      else if constexpr (detail::global_meta_t<T>) {
         return meta<T>::unknown_read;
      }
      else {
         return empty{};
      }
   }();

   template <class T>
   using meta_unknown_read_t = std::decay_t<decltype(meta_unknown_read_v<std::decay_t<T>>)>;

   template <class T>
   concept named = requires { meta<T>::name; } || requires { T::glaze::name; };

   template <class T>
   inline constexpr std::string_view name_v = [] {
      if constexpr (named<T>) {
         if constexpr (requires { T::glaze::name; }) {
            return T::glaze::name;
         }
         else {
            return meta<T>::name;
         }
      }
      else if constexpr (std::is_void_v<T>) {
         return "void";
      }
      else {
         return type_name<T>;
      }
   }();

   template <class T>
   concept tagged = requires { meta<std::decay_t<T>>::tag; } || requires { std::decay_t<T>::glaze::tag; };

   template <class T>
   concept ided = requires { meta<std::decay_t<T>>::ids; } || requires { std::decay_t<T>::glaze::ids; };

   template <class T>
   inline constexpr std::string_view tag_v = [] {
      if constexpr (tagged<T>) {
         if constexpr (detail::local_meta_t<T>) {
            return std::decay_t<T>::glaze::tag;
         }
         else {
            return meta<std::decay_t<T>>::tag;
         }
      }
      else {
         return "";
      }
   }();

   namespace detail
   {
      template <class T, size_t N>
      inline constexpr std::array<std::string_view, N> convert_ids_to_array_of_sv(const std::array<T, N>& arr)
      {
         std::array<std::string_view, N> result;
         for (size_t i = 0; i < N; ++i) {
            result[i] = arr[i];
         }
         return result;
      }
   }

   template <is_variant T>
   inline constexpr auto ids_v = [] {
      if constexpr (ided<T>) {
         if constexpr (detail::local_meta_t<T>) {
            return detail::convert_ids_to_array_of_sv(std::decay_t<T>::glaze::ids);
         }
         else {
            return detail::convert_ids_to_array_of_sv(meta<std::decay_t<T>>::ids);
         }
      }
      else {
         constexpr auto N = std::variant_size_v<T>;
         std::array<std::string_view, N> ids{};
         for_each<N>([&](auto I) { ids[I] = glz::name_v<std::decay_t<std::variant_alternative_t<I, T>>>; });
         return ids;
      }
   }();

   using version_t = std::array<uint32_t, 3>;

   template <class T>
   concept versioned = requires { meta<std::decay_t<T>>::version; };

   template <class T>
   inline constexpr version_t version = []() -> version_t {
      if constexpr (versioned<T>) {
         return meta<std::decay_t<T>>::version;
      }
      else {
         return {0, 0, 1};
      }
   }();

   template <class T>
   inline constexpr auto json_schema_v = [] {
      if constexpr (detail::local_json_schema_t<T>) {
         return typename std::decay_t<T>::glaze_json_schema{};
      }
      else if constexpr (detail::global_json_schema_t<T>) {
         return json_schema<std::decay_t<T>>{};
      }
   }();

   template <class T>
   using json_schema_type = std::decay_t<decltype(json_schema_v<T>)>;

   // Allows developers to add `static constexpr auto custom_read = true;` to their glz::meta to prevent ambiguous
   // partial specialization for custom parsers
   template <class T>
   concept custom_read = requires { meta<T>::custom_read == true; };

   template <class T>
   concept custom_write = requires { meta<T>::custom_write == true; };

   template <class T>
   concept partial_read = requires { meta<T>::partial_read == true; };

   template <typename T>
   concept specialized_with_custom_write = requires {
      meta<T>::custom_write;
      requires(meta<T>::custom_write == true);
   };

   template <typename T>
   concept specialized_with_custom_read = requires {
      meta<T>::custom_write;
      requires(meta<T>::custom_read == true);
   };
}
