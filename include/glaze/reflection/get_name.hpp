// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// TODO: Use std::source_location when deprecating clang 14
// #include <source_location>
#include <string_view>

#include "glaze/reflection/to_tuple.hpp"
#include "glaze/tuplet/tuple.hpp"

#if defined(__clang__) || defined(__GNUC__)
#define GLZ_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define GLZ_PRETTY_FUNCTION __FUNCSIG__
#endif

// For struct fields
namespace glz::detail
{
   template <class T>
   extern const T external;

   template <auto Ptr>
   [[nodiscard]] consteval std::string_view get_mangled_name()
   {
      // return std::source_location::current().function_name();
      return GLZ_PRETTY_FUNCTION;
   }

   template <class T>
   [[nodiscard]] consteval std::string_view get_mangled_name()
   {
      // return std::source_location::current().function_name();
      return GLZ_PRETTY_FUNCTION;
   }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
   template <auto N, class T>
   constexpr auto get_name_impl = get_mangled_name<get_ptr<N>(external<T>)>();
#pragma clang diagnostic pop
#elif __GNUC__
   template <auto N, class T>
   constexpr auto get_name_impl = get_mangled_name<get_ptr<N>(external<T>)>();
#else
   template <auto N, class T>
   constexpr auto get_name_impl = get_mangled_name<get_ptr<N>(external<T>)>();
#endif

   struct GLAZE_REFLECTOR
   {
      int GLAZE_FIELD;
   };

   struct reflect_field
   {
      static constexpr auto name = get_name_impl<0, GLAZE_REFLECTOR>;
      static constexpr auto end = name.substr(name.find("GLAZE_FIELD") + sizeof("GLAZE_FIELD") - 1);
      static constexpr auto begin = name[name.find("GLAZE_FIELD") - 1];
   };

   struct reflect_type
   {
      static constexpr auto name = get_mangled_name<GLAZE_REFLECTOR>();
      static constexpr auto end = name.substr(name.find("GLAZE_REFLECTOR") + sizeof("GLAZE_REFLECTOR") - 1);
      static constexpr auto begin = "T = ";
   };
}

namespace glz
{
   template <auto N, class T>
   static constexpr auto nameof = [] {
      constexpr auto name = detail::get_name_impl<N, T>;
      constexpr auto begin = name.find(detail::reflect_field::end);
      constexpr auto tmp = name.substr(0, begin);
      return tmp.substr(tmp.find_last_of(detail::reflect_field::begin) + 1);
   }();

   template <class T>
   static constexpr auto type_name = [] {
      constexpr auto name = detail::get_mangled_name<T>();
      constexpr auto begin = name.find(detail::reflect_type::end);
      constexpr auto tmp = name.substr(0, begin);
      return tmp.substr(tmp.find_last_of(detail::reflect_type::begin) + 1);
   }();

   template <class T, size_t... I>
   [[nodiscard]] constexpr auto member_names_impl(std::index_sequence<I...>)
   {
      return tuplet::make_tuple(nameof<I, T>...);
   }

   template <class T>
   [[nodiscard]] constexpr auto member_names()
   {
      return member_names_impl<T>(std::make_index_sequence<detail::count_members<T>()>{});
   }
}

// For member object pointers
namespace glz
{
   template <class T>
   struct remove_member_pointer
   {
      using type = T;
   };

   template <class C, class T>
   struct remove_member_pointer<T C::*>
   {
      using type = C;
   };

   template <class T, auto P>
   consteval std::string_view get_name_msvc() noexcept
   {
      std::string_view str = GLZ_PRETTY_FUNCTION;

      size_t i = str.find("->");
      str = str.substr(i + 2);
      i = str.find(">");
      str = str.substr(0, i);

      return str;
   }

#if defined(__clang__)
   inline constexpr auto pretty_function_tail = "]";
#elif defined(__GNUC__) || defined(__GNUG__)
   inline constexpr auto pretty_function_tail = ";";
#elif defined(_MSC_VER)
#endif

   template <auto P>
      requires(std::is_member_object_pointer_v<decltype(P)>)
   consteval std::string_view get_name() noexcept
   {
#if defined(_MSC_VER)
      using T = remove_member_pointer<std::decay_t<decltype(P)>>::type;
      constexpr auto p = P;
      return get_name_msvc<T, &(detail::external<T>.*p)>();
#else
      // TODO: Use std::source_location when deprecating clang 14
      // std::string_view str = std::source_location::current().function_name();
      std::string_view str = GLZ_PRETTY_FUNCTION;

      size_t i = str.find("&");
      str = str.substr(i + 1);
      i = str.find("::");
      str = str.substr(i + 2);
      i = str.find(pretty_function_tail);
      str = str.substr(0, i);

      return str;
#endif
   }
}
