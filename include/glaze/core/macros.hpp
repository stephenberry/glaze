// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// utility macros

// https://www.scs.stanford.edu/~dm/blog/va-opt.html

#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)                                    \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...)                         \
  macro(a1)__VA_OPT__(,)                                                     \
  __VA_OPT__(FOR_EACH_AGAIN PARENS (macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

// Glaze specific macros

#define GLZ_X(a) #a, &T::a

#define GLZ_META(C, ...) template <> struct glz::meta<C> { \
using T = C; \
static constexpr auto value = object(FOR_EACH(GLZ_X, __VA_ARGS__)); }

#define GLZ_LOCAL_META(C, ...) struct glaze { \
using T = C; \
static constexpr auto value = glz::object(FOR_EACH(GLZ_X, __VA_ARGS__)); }
