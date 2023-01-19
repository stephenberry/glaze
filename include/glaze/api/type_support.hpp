// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <string_view>

#include "glaze/util/for_each.hpp"

namespace glz
{
#define specialize(type) template <> \
   struct meta<type> { \
      static constexpr std::string_view name = #type; \
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
   struct meta<T> {
      using V = std::remove_reference_t<T>;
      static constexpr std::string_view name = detail::join_v<name_v<V>, chars<"&">>;
   };

   template <class T>
   concept rvalue_reference = std::is_rvalue_reference_v<T>;

   template <rvalue_reference T>
   struct meta<T>
   {
      using V = std::remove_reference_t<T>;
      static constexpr std::string_view name = detail::join_v<name_v<V>, chars<"&&">>;
   };
   
   template <class T>
   concept constant = std::is_const_v<T>;
   
   template <constant T>
   struct meta<T> {
      using V = std::remove_const_t<T>;
      static constexpr std::string_view name = detail::join_v<chars<"const ">, name_v<V>>;
   };
   
   template <class T>
   concept pointer = std::is_pointer_v<T>;
   
   template <pointer T>
   struct meta<T> {
      using V = std::remove_pointer_t<T>;
      static constexpr std::string_view name = detail::join_v<name_v<V>, chars<"*">>;
   };

   template <typename Ret, typename Obj, class... Args>
   struct meta<Ret (Obj::*)(Args...)>
   {
   static constexpr std::string_view name =
         detail::join_v<name_v<Ret>, chars<" (">, name_v<Obj>, chars<"::*)(">, name_v<Args>..., chars<")">>;
   };
}
