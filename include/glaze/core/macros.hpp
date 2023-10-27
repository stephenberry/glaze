// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// utility macros

// https://www.scs.stanford.edu/~dm/blog/va-opt.html

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

// Glaze specific macros

#define GLZ_X(a) #a, &GLZ_T::a
#define GLZ_QUOTED_X(a) #a, glz::quoted<&GLZ_T::a>
#define GLZ_QUOTED_NUM_X(a) #a, glz::quoted_num<&GLZ_T::a>

#define GLZ_META(C, ...)                                                      \
   template <>                                                                \
   struct glz::meta<C>                                                        \
   {                                                                          \
      using GLZ_T = C;                                                        \
      [[maybe_unused]] static constexpr std::string_view name = #C;           \
      static constexpr auto value = object(GLZ_FOR_EACH(GLZ_X, __VA_ARGS__)); \
   }

#define GLZ_LOCAL_META(C, ...)                                                     \
   struct glaze                                                                    \
   {                                                                               \
      using GLZ_T = C;                                                             \
      [[maybe_unused]] static constexpr std::string_view name = #C;                \
      static constexpr auto value = glz::object(GLZ_FOR_EACH(GLZ_X, __VA_ARGS__)); \
   }
