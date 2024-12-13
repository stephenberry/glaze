// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>
#include <utility>

#include "glaze/util/inline.hpp"
#include "glaze/util/utility.hpp"

namespace glz
{
   // Compile time iterate over I indices
   // We do not make this noexcept so that it can be used in exception contexts
   template <std::size_t N, class Func>
   constexpr void for_each(Func&& f)
   {
      if constexpr (N == 0) {
         return;
      }
      else {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (f(std::integral_constant<std::size_t, I>{}), ...);
         }(std::make_index_sequence<N>{});
      }
   }

   // Runtime short circuiting if function returns true, return false to continue evaluation
   template <std::size_t N, class Func>
   constexpr void for_each_short_circuit(Func&& f)
   {
      if constexpr (N == 0) {
         return;
      }
      else {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (f(std::integral_constant<std::size_t, I>{}) || ...);
         }(std::make_index_sequence<N>{});
      }
   }

   template <class Func, class Tuple>
   constexpr void for_each_apply(Func&& f, Tuple&& t)
   {
      constexpr size_t N = std::tuple_size_v<std::decay_t<Tuple>>;
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::get<I>(t)), ...);
      }(std::make_index_sequence<N>{});
   }
}

namespace glz
{
   template <size_t N>
   inline constexpr void visit(auto&& lambda, const size_t index)
   {
      if constexpr (N > 0) {
         static constexpr auto mem_ptrs = []<size_t... I>(std::index_sequence<I...>) constexpr {
            return std::array{&std::decay_t<decltype(lambda)>::template operator()<I>...};
         }(std::make_index_sequence<N>{});

         (lambda.*mem_ptrs[index])();
      }
   }
}

#define GLZ_PARENS ()

// binary expansion is much more compile time efficient than quaternary expansion
#define GLZ_EXPAND(...) GLZ_EXPAND32(__VA_ARGS__)
#define GLZ_EXPAND32(...) GLZ_EXPAND16(GLZ_EXPAND16(__VA_ARGS__))
#define GLZ_EXPAND16(...) GLZ_EXPAND8(GLZ_EXPAND8(__VA_ARGS__))
#define GLZ_EXPAND8(...) GLZ_EXPAND4(GLZ_EXPAND4(__VA_ARGS__))
#define GLZ_EXPAND4(...) GLZ_EXPAND2(GLZ_EXPAND2(__VA_ARGS__))
#define GLZ_EXPAND2(...) GLZ_EXPAND1(GLZ_EXPAND1(__VA_ARGS__))
#define GLZ_EXPAND1(...) __VA_ARGS__

#define GLZ_FOR_EACH(macro, ...) __VA_OPT__(GLZ_EXPAND(GLZ_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define GLZ_FOR_EACH_HELPER(macro, a, ...) \
   macro(a) __VA_OPT__(, ) __VA_OPT__(GLZ_FOR_EACH_AGAIN GLZ_PARENS(macro, __VA_ARGS__))
#define GLZ_FOR_EACH_AGAIN() GLZ_FOR_EACH_HELPER

namespace glz::detail
{
#define GLZ_EVERY(macro, ...) __VA_OPT__(GLZ_EXPAND(GLZ_EVERY_HELPER(macro, __VA_ARGS__)))
#define GLZ_EVERY_HELPER(macro, a, ...) macro(a) __VA_OPT__(GLZ_EVERY_AGAIN GLZ_PARENS(macro, __VA_ARGS__))
#define GLZ_EVERY_AGAIN() GLZ_EVERY_HELPER

#define GLZ_CASE(I)                    \
   case I: {                           \
      lambda.template operator()<I>(); \
      break;                           \
   }

#define GLZ_SWITCH(X, ...)                 \
   else if constexpr (N == X)              \
   {                                       \
      switch (index) {                     \
         GLZ_EVERY(GLZ_CASE, __VA_ARGS__); \
      default: {                           \
         unreachable();                    \
      }                                    \
      }                                    \
   }

#define GLZ_10 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
#define GLZ_20 GLZ_10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20
#define GLZ_30 GLZ_20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30
#define GLZ_40 GLZ_30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40
#define GLZ_50 GLZ_40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50
#define GLZ_60 GLZ_50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60

   template <size_t N, class Lambda>
   GLZ_ALWAYS_INLINE constexpr void jump_table(Lambda&& lambda, size_t index)
   {
      if constexpr (N == 0) {
         return;
      }
      else if constexpr (N == 1) {
         lambda.template operator()<0>();
      }
      GLZ_SWITCH(2, 0, 1)
      GLZ_SWITCH(3, 0, 1, 2)
      GLZ_SWITCH(4, 0, 1, 2, 3)
      GLZ_SWITCH(5, 0, 1, 2, 3, 4)
      GLZ_SWITCH(6, 0, 1, 2, 3, 4, 5)
      GLZ_SWITCH(7, 0, 1, 2, 3, 4, 5, 6)
      GLZ_SWITCH(8, 0, 1, 2, 3, 4, 5, 6, 7)
      GLZ_SWITCH(9, 0, 1, 2, 3, 4, 5, 6, 7, 8)
      GLZ_SWITCH(10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)
      GLZ_SWITCH(11, GLZ_10)
      GLZ_SWITCH(12, GLZ_10, 11)
      GLZ_SWITCH(13, GLZ_10, 11, 12)
      GLZ_SWITCH(14, GLZ_10, 11, 12, 13)
      GLZ_SWITCH(15, GLZ_10, 11, 12, 13, 14)
      GLZ_SWITCH(16, GLZ_10, 11, 12, 13, 14, 15)
      GLZ_SWITCH(17, GLZ_10, 11, 12, 13, 14, 15, 16)
      GLZ_SWITCH(18, GLZ_10, 11, 12, 13, 14, 15, 16, 17)
      GLZ_SWITCH(19, GLZ_10, 11, 12, 13, 14, 15, 16, 17, 18)
      GLZ_SWITCH(20, GLZ_10, 11, 12, 13, 14, 15, 16, 17, 18, 19)
      GLZ_SWITCH(21, GLZ_20)
      GLZ_SWITCH(22, GLZ_20, 21)
      GLZ_SWITCH(23, GLZ_20, 21, 22)
      GLZ_SWITCH(24, GLZ_20, 21, 22, 23)
      GLZ_SWITCH(25, GLZ_20, 21, 22, 23, 24)
      GLZ_SWITCH(26, GLZ_20, 21, 22, 23, 24, 25)
      GLZ_SWITCH(27, GLZ_20, 21, 22, 23, 24, 25, 26)
      GLZ_SWITCH(28, GLZ_20, 21, 22, 23, 24, 25, 26, 27)
      GLZ_SWITCH(29, GLZ_20, 21, 22, 23, 24, 25, 26, 27, 28)
      GLZ_SWITCH(30, GLZ_20, 21, 22, 23, 24, 25, 26, 27, 28, 29)
      GLZ_SWITCH(31, GLZ_30)
      GLZ_SWITCH(32, GLZ_30, 31)
      GLZ_SWITCH(33, GLZ_30, 31, 32)
      GLZ_SWITCH(34, GLZ_30, 31, 32, 33)
      GLZ_SWITCH(35, GLZ_30, 31, 32, 33, 34)
      GLZ_SWITCH(36, GLZ_30, 31, 32, 33, 34, 35)
      GLZ_SWITCH(37, GLZ_30, 31, 32, 33, 34, 35, 36)
      GLZ_SWITCH(38, GLZ_30, 31, 32, 33, 34, 35, 36, 37)
      GLZ_SWITCH(39, GLZ_30, 31, 32, 33, 34, 35, 36, 37, 38)
      GLZ_SWITCH(40, GLZ_30, 31, 32, 33, 34, 35, 36, 37, 38, 39)
      GLZ_SWITCH(41, GLZ_40)
      GLZ_SWITCH(42, GLZ_40, 41)
      GLZ_SWITCH(43, GLZ_40, 41, 42)
      GLZ_SWITCH(44, GLZ_40, 41, 42, 43)
      GLZ_SWITCH(45, GLZ_40, 41, 42, 43, 44)
      GLZ_SWITCH(46, GLZ_40, 41, 42, 43, 44, 45)
      GLZ_SWITCH(47, GLZ_40, 41, 42, 43, 44, 45, 46)
      GLZ_SWITCH(48, GLZ_40, 41, 42, 43, 44, 45, 46, 47)
      GLZ_SWITCH(49, GLZ_40, 41, 42, 43, 44, 45, 46, 47, 48)
      GLZ_SWITCH(50, GLZ_40, 41, 42, 43, 44, 45, 46, 47, 48, 49)
      GLZ_SWITCH(51, GLZ_50)
      GLZ_SWITCH(52, GLZ_50, 51)
      GLZ_SWITCH(53, GLZ_50, 51, 52)
      GLZ_SWITCH(54, GLZ_50, 51, 52, 53)
      GLZ_SWITCH(55, GLZ_50, 51, 52, 53, 54)
      GLZ_SWITCH(56, GLZ_50, 51, 52, 53, 54, 55)
      GLZ_SWITCH(57, GLZ_50, 51, 52, 53, 54, 55, 56)
      GLZ_SWITCH(58, GLZ_50, 51, 52, 53, 54, 55, 56, 57)
      GLZ_SWITCH(59, GLZ_50, 51, 52, 53, 54, 55, 56, 57, 58)
      GLZ_SWITCH(60, GLZ_50, 51, 52, 53, 54, 55, 56, 57, 58, 59)
      GLZ_SWITCH(61, GLZ_60)
      GLZ_SWITCH(62, GLZ_60, 61)
      GLZ_SWITCH(63, GLZ_60, 61, 62)
      GLZ_SWITCH(64, GLZ_60, 61, 62, 63)
      else
      {
         for_each_short_circuit<N>([&](auto I) {
            if (index == I) {
               lambda.template operator()<I>();
               return true;
            }
            return false;
         });
      }
   }

#define GLZ_INVOKE(I) lambda.template operator()<I>();

#define GLZ_INVOKE_ALL(X, ...)            \
   else if constexpr (N == X)             \
   {                                      \
      GLZ_EVERY(GLZ_INVOKE, __VA_ARGS__); \
   }

   template <size_t N, class Lambda>
   GLZ_ALWAYS_INLINE constexpr void invoke_table(Lambda&& lambda)
   {
      if constexpr (N == 0) {
         return;
      }
      else if constexpr (N == 1) {
         static_cast<Lambda&&>(lambda).template operator()<0>();
      }
      GLZ_INVOKE_ALL(2, 0, 1)
      GLZ_INVOKE_ALL(3, 0, 1, 2)
      GLZ_INVOKE_ALL(4, 0, 1, 2, 3)
      GLZ_INVOKE_ALL(5, 0, 1, 2, 3, 4)
      GLZ_INVOKE_ALL(6, 0, 1, 2, 3, 4, 5)
      GLZ_INVOKE_ALL(7, 0, 1, 2, 3, 4, 5, 6)
      GLZ_INVOKE_ALL(8, 0, 1, 2, 3, 4, 5, 6, 7)
      GLZ_INVOKE_ALL(9, 0, 1, 2, 3, 4, 5, 6, 7, 8)
      GLZ_INVOKE_ALL(10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)
      GLZ_INVOKE_ALL(11, GLZ_10)
      GLZ_INVOKE_ALL(12, GLZ_10, 11)
      GLZ_INVOKE_ALL(13, GLZ_10, 11, 12)
      GLZ_INVOKE_ALL(14, GLZ_10, 11, 12, 13)
      GLZ_INVOKE_ALL(15, GLZ_10, 11, 12, 13, 14)
      GLZ_INVOKE_ALL(16, GLZ_10, 11, 12, 13, 14, 15)
      GLZ_INVOKE_ALL(17, GLZ_10, 11, 12, 13, 14, 15, 16)
      GLZ_INVOKE_ALL(18, GLZ_10, 11, 12, 13, 14, 15, 16, 17)
      GLZ_INVOKE_ALL(19, GLZ_10, 11, 12, 13, 14, 15, 16, 17, 18)
      GLZ_INVOKE_ALL(20, GLZ_10, 11, 12, 13, 14, 15, 16, 17, 18, 19)
      GLZ_INVOKE_ALL(21, GLZ_20)
      GLZ_INVOKE_ALL(22, GLZ_20, 21)
      GLZ_INVOKE_ALL(23, GLZ_20, 21, 22)
      GLZ_INVOKE_ALL(24, GLZ_20, 21, 22, 23)
      GLZ_INVOKE_ALL(25, GLZ_20, 21, 22, 23, 24)
      GLZ_INVOKE_ALL(26, GLZ_20, 21, 22, 23, 24, 25)
      GLZ_INVOKE_ALL(27, GLZ_20, 21, 22, 23, 24, 25, 26)
      GLZ_INVOKE_ALL(28, GLZ_20, 21, 22, 23, 24, 25, 26, 27)
      GLZ_INVOKE_ALL(29, GLZ_20, 21, 22, 23, 24, 25, 26, 27, 28)
      GLZ_INVOKE_ALL(30, GLZ_20, 21, 22, 23, 24, 25, 26, 27, 28, 29)
      GLZ_INVOKE_ALL(31, GLZ_30)
      GLZ_INVOKE_ALL(32, GLZ_30, 31)
      GLZ_INVOKE_ALL(33, GLZ_30, 31, 32)
      GLZ_INVOKE_ALL(34, GLZ_30, 31, 32, 33)
      GLZ_INVOKE_ALL(35, GLZ_30, 31, 32, 33, 34)
      GLZ_INVOKE_ALL(36, GLZ_30, 31, 32, 33, 34, 35)
      GLZ_INVOKE_ALL(37, GLZ_30, 31, 32, 33, 34, 35, 36)
      GLZ_INVOKE_ALL(38, GLZ_30, 31, 32, 33, 34, 35, 36, 37)
      GLZ_INVOKE_ALL(39, GLZ_30, 31, 32, 33, 34, 35, 36, 37, 38)
      GLZ_INVOKE_ALL(40, GLZ_30, 31, 32, 33, 34, 35, 36, 37, 38, 39)
      GLZ_INVOKE_ALL(41, GLZ_40)
      GLZ_INVOKE_ALL(42, GLZ_40, 41)
      GLZ_INVOKE_ALL(43, GLZ_40, 41, 42)
      GLZ_INVOKE_ALL(44, GLZ_40, 41, 42, 43)
      GLZ_INVOKE_ALL(45, GLZ_40, 41, 42, 43, 44)
      GLZ_INVOKE_ALL(46, GLZ_40, 41, 42, 43, 44, 45)
      GLZ_INVOKE_ALL(47, GLZ_40, 41, 42, 43, 44, 45, 46)
      GLZ_INVOKE_ALL(48, GLZ_40, 41, 42, 43, 44, 45, 46, 47)
      GLZ_INVOKE_ALL(49, GLZ_40, 41, 42, 43, 44, 45, 46, 47, 48)
      GLZ_INVOKE_ALL(50, GLZ_40, 41, 42, 43, 44, 45, 46, 47, 48, 49)
      GLZ_INVOKE_ALL(51, GLZ_50)
      GLZ_INVOKE_ALL(52, GLZ_50, 51)
      GLZ_INVOKE_ALL(53, GLZ_50, 51, 52)
      GLZ_INVOKE_ALL(54, GLZ_50, 51, 52, 53)
      GLZ_INVOKE_ALL(55, GLZ_50, 51, 52, 53, 54)
      GLZ_INVOKE_ALL(56, GLZ_50, 51, 52, 53, 54, 55)
      GLZ_INVOKE_ALL(57, GLZ_50, 51, 52, 53, 54, 55, 56)
      GLZ_INVOKE_ALL(58, GLZ_50, 51, 52, 53, 54, 55, 56, 57)
      GLZ_INVOKE_ALL(59, GLZ_50, 51, 52, 53, 54, 55, 56, 57, 58)
      GLZ_INVOKE_ALL(60, GLZ_50, 51, 52, 53, 54, 55, 56, 57, 58, 59)
      GLZ_INVOKE_ALL(61, GLZ_60)
      GLZ_INVOKE_ALL(62, GLZ_60, 61)
      GLZ_INVOKE_ALL(63, GLZ_60, 61, 62)
      GLZ_INVOKE_ALL(64, GLZ_60, 61, 62, 63)
      else
      {
         [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            (lambda.template operator()<I>(), ...);
         }(std::make_index_sequence<N>{});
      }
   }

#undef GLZ_10
#undef GLZ_20
#undef GLZ_30
#undef GLZ_40
#undef GLZ_50
#undef GLZ_60

#undef GLZ_CASE
#undef GLZ_SWITCH

#undef GLZ_INVOKE
#undef GLZ_INVOKE_ALL

#undef GLZ_EVERY_AGAIN
#undef GLZ_EVERY_HELPER
#undef GLZ_EVERY
}
