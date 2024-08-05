// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

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
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::integral_constant<std::size_t, I>{}), ...);
      }(std::make_index_sequence<N>{});
   }

   template <std::size_t N, class Func>
   GLZ_FLATTEN constexpr void for_each_flatten(Func&& f)
   {
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::integral_constant<std::size_t, I>{}), ...);
      }(std::make_index_sequence<N>{});
   }

   // Runtime short circuiting if function returns true, return false to continue evaluation
   template <std::size_t N, class Func>
   constexpr void for_each_short_circuit(Func&& f)
   {
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::integral_constant<std::size_t, I>{}) || ...);
      }(std::make_index_sequence<N>{});
   }

   template <class Func, class Tuple>
   constexpr void for_each_apply(Func&& f, Tuple&& t)
   {
      constexpr size_t N = glz::tuple_size_v<std::decay_t<Tuple>>;
      [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
         (f(std::get<I>(t)), ...);
      }(std::make_index_sequence<N>{});
   }
}

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

namespace glz::detail
{
#define GLZ_EVERY(macro, ...) __VA_OPT__(GLZ_EXPAND(GLZ_EVERY_HELPER(macro, __VA_ARGS__)))
#define GLZ_EVERY_HELPER(macro, a, ...) macro(a) __VA_OPT__(GLZ_EVERY_AGAIN GLZ_PARENS(macro, __VA_ARGS__))
#define GLZ_EVERY_AGAIN() GLZ_EVERY_HELPER

#define GLZ1(I)                                                \
   case I: {                                                   \
      static_cast<Lambda&&>(lambda).template operator()<I>();  \
      break;                                                   \
   }

#define GLZ_SWITCH(X, ...)             \
   else if constexpr (N == X)          \
   {                                   \
      switch (index) {                 \
         GLZ_EVERY(GLZ1, __VA_ARGS__); \
      default: {                       \
         unreachable();                \
      }                                \
      }                                \
   }

#define GLZ_10 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
#define GLZ_20 GLZ_10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20
#define GLZ_30 GLZ_20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30
#define GLZ_40 GLZ_30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40
#define GLZ_50 GLZ_40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50
#define GLZ_60 GLZ_50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60

      template <size_t N, class Lambda>
      GLZ_ALWAYS_INLINE constexpr void jump_table(Lambda&& lambda, size_t index) noexcept
      {
         if constexpr (N == 1) {
            static_cast<Lambda&&>(lambda).template operator()<0>();
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

#undef GLZ_10
#undef GLZ_20
#undef GLZ_30
#undef GLZ_40
#undef GLZ_50
#undef GLZ_60

#undef GLZ1
#undef GLZ_SWITCH
#undef GLZ_EVERY_AGAIN
#undef GLZ_EVERY_HELPER
#undef GLZ_EVERY
}
