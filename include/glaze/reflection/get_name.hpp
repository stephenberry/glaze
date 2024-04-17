// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// TODO: Use std::source_location when deprecating clang 14
// #include <source_location>
#include <string_view>

#include "glaze/reflection/to_tuple.hpp"
#include "glaze/util/string_literal.hpp"

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

   // using const char* simplifies the complier's output and should improve compile times
   template <auto Ptr>
   [[nodiscard]] consteval auto mangled_name()
   {
      // return std::source_location::current().function_name();
      return GLZ_PRETTY_FUNCTION;
   }

   template <class T>
   [[nodiscard]] consteval auto mangled_name()
   {
      // return std::source_location::current().function_name();
      return GLZ_PRETTY_FUNCTION;
   }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
   template <auto N, class T>
   constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#pragma clang diagnostic pop
#elif __GNUC__
   template <auto N, class T>
   constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#else
   template <auto N, class T>
   constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
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
      static constexpr std::string_view name = mangled_name<GLAZE_REFLECTOR>();
      static constexpr auto end = name.substr(name.find("GLAZE_REFLECTOR") + sizeof("GLAZE_REFLECTOR") - 1);
#if defined(__GNUC__) || defined(__clang__)
      static constexpr auto begin = std::string_view{"T = "};
#else
      static constexpr auto begin = std::string_view{"glz::detail::mangled_name<"};
#endif
   };
}

namespace glz
{
   template <auto N, class T>
   struct nameof_impl
   {
      static constexpr auto name = detail::get_name_impl<N, T>;
      static constexpr auto begin = name.find(detail::reflect_field::end);
      static constexpr auto tmp = name.substr(0, begin);
      static constexpr auto stripped = tmp.substr(tmp.find_last_of(detail::reflect_field::begin) + 1);
      // making static memory to stripped to help the compiler optimize away prettified function signature
      static constexpr std::string_view stripped_literal = join_v<stripped>;
   };

   template <auto N, class T>
   constexpr auto nameof = []() constexpr { return nameof_impl<N, T>::stripped_literal; }();

   template <class T>
   constexpr auto type_name = [] {
      constexpr std::string_view name = detail::mangled_name<T>();
      constexpr auto begin = name.find(detail::reflect_type::end);
      constexpr auto tmp = name.substr(0, begin);
#if defined(__GNUC__) || defined(__clang__)
      return tmp.substr(tmp.rfind(detail::reflect_type::begin) + detail::reflect_type::begin.size());
#else
      constexpr auto name_with_keyword =
         tmp.substr(tmp.rfind(detail::reflect_type::begin) + detail::reflect_type::begin.size());
      return name_with_keyword.substr(name_with_keyword.find(' ') + 1);
#endif
   }();

   template <class T, size_t... I>
   [[nodiscard]] constexpr auto member_names_impl(std::index_sequence<I...>)
   {
      return std::array{nameof<I, T>...};
   }

   template <class T>
   inline constexpr auto member_names =
      [] { return member_names_impl<T>(std::make_index_sequence<detail::count_members<T>>{}); }();
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

   template <class C, class R, class... Args>
   struct remove_member_pointer<R (C::*)(Args...)>
   {
      using type = C;
   };

   template <class T, auto P>
   consteval std::string_view get_name_msvc()
   {
      std::string_view str = GLZ_PRETTY_FUNCTION;
      str = str.substr(str.find("->") + 2);
      return str.substr(0, str.find(">"));
   }

   template <class T, auto P>
   consteval std::string_view func_name_msvc()
   {
      std::string_view str = GLZ_PRETTY_FUNCTION;
      str = str.substr(str.rfind(type_name<T>) + type_name<T>.size());
      str = str.substr(str.find("::") + 2);
      return str.substr(0, str.find("("));
   }

#if defined(__clang__)
   inline constexpr auto pretty_function_tail = "]";
#elif defined(__GNUC__) || defined(__GNUG__)
   inline constexpr auto pretty_function_tail = ";";
#elif defined(_MSC_VER)
#endif

   template <auto P>
      requires(std::is_member_pointer_v<decltype(P)>)
   consteval std::string_view get_name()
   {
#if defined(_MSC_VER) && !defined(__clang__)
      if constexpr (std::is_member_object_pointer_v<decltype(P)>) {
         using T = remove_member_pointer<std::decay_t<decltype(P)>>::type;
         constexpr auto p = P;
         return get_name_msvc<T, &(detail::external<T>.*p)>();
      }
      else {
         using T = remove_member_pointer<std::decay_t<decltype(P)>>::type;
         return func_name_msvc<T, P>();
      }
#else
      // TODO: Use std::source_location when deprecating clang 14
      // std::string_view str = std::source_location::current().function_name();
      std::string_view str = GLZ_PRETTY_FUNCTION;
      str = str.substr(str.find("&") + 1);
      str = str.substr(0, str.find(pretty_function_tail));
      return str.substr(str.rfind("::") + 2);
#endif
   }

   template <auto E>
      requires(std::is_enum_v<decltype(E)>)
   consteval auto get_name()
   {
#if defined(_MSC_VER) && !defined(__clang__)
      std::string_view str = GLZ_PRETTY_FUNCTION;
      str = str.substr(str.rfind("::") + 2);
      return str.substr(0, str.find('>'));
#else
      std::string_view str = GLZ_PRETTY_FUNCTION;
      str = str.substr(str.rfind("::") + 2);
      return str.substr(0, str.find(']'));
#endif
   }
}
