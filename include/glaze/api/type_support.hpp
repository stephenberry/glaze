// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <string_view>

namespace glaze
{
   template <class = void, std::size_t...Is>
   constexpr auto indexer(std::index_sequence<Is...>) {
     return [](auto&& f) -> decltype(auto) {
       return decltype(f)(f)( std::integral_constant<std::size_t, Is>{}... );
     };
   }
   
   // takes a number N
   // returns a function object that, when passed a function object f
   // passes it compile-time values from 0 to N-1 inclusive.
   template <size_t N>
   constexpr auto indexer() {
     return indexer(std::make_index_sequence<N>{});
   }
   
   template <size_t N, class Func>
   constexpr auto for_each(Func&& f) {
      return indexer<N>()([&](auto&&...i){
         (f(i), ...);
      });
   }
   
   template<class T> struct false_v : std::false_type {};
   namespace detail {
       struct aggressive_unicorn_type; // Do not unleash
   }
   template<> struct false_v<detail::aggressive_unicorn_type> : std::true_type {};
   
#define specialize(type) template <> \
   struct name_t<type> { \
      static constexpr std::string_view value = #type; \
   };
   
   specialize(bool)
   specialize(char) specialize(char16_t) specialize(char32_t)
   specialize(wchar_t)
   specialize(int8_t) specialize(uint8_t)
   specialize(int16_t) specialize(uint16_t)
   specialize(int32_t) specialize(uint32_t)
   specialize(int64_t) specialize(uint64_t)
   specialize(float) specialize(double)
   specialize(std::string_view)
#undef specialize
   
   template <class T>
   concept lvalue_reference = std::is_lvalue_reference_v<T>;
   
   template <lvalue_reference T>
   struct name_t<T> {
      using V = std::remove_reference_t<T>;
      static constexpr std::string_view value = detail::join_v<name<V>, chars<"&">>;
   };
   
   template <class T>
   concept constant = std::is_const_v<T>;
   
   template <constant T>
   struct name_t<T> {
      using V = std::remove_const_t<T>;
      static constexpr std::string_view value = detail::join_v<chars<"const ">, name<V>>;
   };
   
   template <class T>
   concept pointer = std::is_pointer_v<T>;
   
   template <pointer T>
   struct name_t<T> {
      using V = std::remove_pointer_t<T>;
      static constexpr std::string_view value = detail::join_v<name<V>, chars<"*">>;
   };
}
