// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <string_view>

namespace glz
{
   template <class T>
   concept has_nameof = requires {
      {
         nameof(T{})
      } -> std::convertible_to<std::string_view>;
   };

   template <class... Args>
   constexpr size_t number_of_args(Args&&...)
   {
      return sizeof...(Args);
   }
}

// Macros for GLZ_ENUM
#define GLZ_PARENS ()

#define GLZ_EXPAND(...) GLZ_EXPAND4(GLZ_EXPAND4(GLZ_EXPAND4(GLZ_EXPAND4(__VA_ARGS__))))
#define GLZ_EXPAND4(...) GLZ_EXPAND3(GLZ_EXPAND3(GLZ_EXPAND3(GLZ_EXPAND3(__VA_ARGS__))))
#define GLZ_EXPAND3(...) GLZ_EXPAND2(GLZ_EXPAND2(GLZ_EXPAND2(GLZ_EXPAND2(__VA_ARGS__))))
#define GLZ_EXPAND2(...) GLZ_EXPAND1(GLZ_EXPAND1(GLZ_EXPAND1(GLZ_EXPAND1(__VA_ARGS__))))
#define GLZ_EXPAND1(...) __VA_ARGS__

#define GLZ_FOR_EACH(macro, ...) __VA_OPT__(GLZ_EXPAND(GLZ_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define GLZ_FOR_EACH_HELPER(macro, a, ...) \
   macro(a) __VA_OPT__(, ) __VA_OPT__(GLZ_FOR_EACH_AGAIN GLZ_PARENS(macro, __VA_ARGS__))
#define GLZ_FOR_EACH_AGAIN() GLZ_FOR_EACH_HELPER

#define GLZ_STRINGIFY(a) \
   std::string_view { #a }

// For pairs of arguments
#define GLZ_FOR_EACH2(macro, ...) __VA_OPT__(GLZ_EXPAND(GLZ_FOR_EACH_HELPER2(macro, __VA_ARGS__)))
#define GLZ_FOR_EACH_HELPER2(macro, a1, a2, ...) \
   macro(a1, a2) __VA_OPT__(, GLZ_FOR_EACH_AGAIN2 GLZ_PARENS(macro, __VA_ARGS__))
#define GLZ_FOR_EACH_AGAIN2() GLZ_FOR_EACH_HELPER2

#define GLZ_EXTRACT_FIRST(x, y) x
#define GLZ_EXTRACT_SECOND(x, y) y

// Create an enum with the provided fields. A nameof reflection function made.
// Example: GLZ_ENUM(color, red, green blue)
// nameof(color::red) == "red"
#define GLZ_ENUM(EnumType, ...)                                                                              \
   enum struct EnumType : uint32_t { __VA_ARGS__ };                                                          \
   constexpr auto EnumType##_names = std::array{GLZ_FOR_EACH(GLZ_STRINGIFY, __VA_ARGS__)};                   \
   constexpr std::string_view nameof(EnumType value) noexcept { return (EnumType##_names)[size_t(value)]; }; \
   template <class T>                                                                                        \
      requires(std::same_as<std::decay_t<T>, EnumType>)                                                      \
   constexpr decltype(auto) enum_names(T&&) noexcept                                                         \
   {                                                                                                         \
      return (EnumType##_names);                                                                             \
   }

// Create an enum with the provided fields and names. A nameof reflection function is made.
// Example: GLZ_ENUM_MAP(color, "Red", red, "Green", green, "Blue", blue);
// nameof(color::red) == "Red"
#define GLZ_ENUM_MAP(EnumType, ...)                                                                          \
   enum struct EnumType : uint32_t { GLZ_FOR_EACH2(GLZ_EXTRACT_SECOND, __VA_ARGS__) };                       \
   constexpr auto EnumType##_names = std::array{GLZ_FOR_EACH2(GLZ_EXTRACT_FIRST, __VA_ARGS__)};              \
   constexpr std::string_view nameof(EnumType value) noexcept { return (EnumType##_names)[size_t(value)]; }; \
   template <class T>                                                                                        \
      requires(std::same_as<std::decay_t<T>, EnumType>)                                                      \
   constexpr decltype(auto) enum_names(T&&) noexcept                                                         \
   {                                                                                                         \
      return (EnumType##_names);                                                                             \
   }
