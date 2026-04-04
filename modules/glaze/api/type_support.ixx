// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.api.type_support;

import std;

import glaze.core.meta;
import glaze.util.for_each;
import glaze.util.string_literal;

using std::int8_t;
using std::uint8_t;
using std::int16_t;
using std::uint16_t;
using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;

export namespace glz
{
#define specialize(type)                              \
   template <>                                        \
   struct meta<type>                                  \
   {                                                  \
      static constexpr std::string_view name = #type; \
   };

   specialize(bool) specialize(char) specialize(char16_t) specialize(char32_t) specialize(wchar_t) specialize(int8_t)
      specialize(uint8_t) specialize(int16_t) specialize(uint16_t) specialize(int32_t) specialize(uint32_t)
         specialize(int64_t) specialize(uint64_t) specialize(float) specialize(double)
#undef specialize

            template <std::same_as<long long> long_long_t>
      requires requires { !std::same_as<long long, int64_t>; }
   struct meta<long_long_t>
   {
      static_assert(sizeof(long long) == 8);
      static constexpr std::string_view name{"std::int64_t"};
   };
   static_assert(glz::name_v<int64_t> == glz::name_v<long long>);

   template <std::same_as<unsigned long long> unsigned_long_long_t>
      requires requires { !std::same_as<unsigned long long, uint64_t>; }
   struct meta<unsigned_long_long_t>
   {
      static_assert(sizeof(unsigned long long) == 8);
      static constexpr std::string_view name{"std::uint64_t"};
   };
   static_assert(glz::name_v<uint64_t> == glz::name_v<unsigned long long>);

   template <class T>
      requires(std::is_lvalue_reference_v<T>)
   struct meta<T>
   {
      using V = std::remove_reference_t<T>;
      static constexpr std::string_view name = join_v<name_v<V>, chars<"&">>;
   };

   template <class T>
      requires(std::is_rvalue_reference_v<T>)
   struct meta<T>
   {
      using V = std::remove_reference_t<T>;
      static constexpr std::string_view name = join_v<name_v<V>, chars<"&&">>;
   };

   template <class T>
      requires(std::is_const_v<T>)
   struct meta<T>
   {
      using V = std::remove_const_t<T>;
      static constexpr std::string_view name = join_v<chars<"const ">, name_v<V>>;
   };

   template <class T>
      requires(std::is_pointer_v<T>)
   struct meta<T>
   {
      using V = std::remove_pointer_t<T>;
      static constexpr std::string_view name = join_v<name_v<V>, chars<"*">>;
   };

   template <class Ret, class Obj, class... Args>
   struct meta<Ret (Obj::*)(Args...)>
   {
      static constexpr std::string_view name =
         join_v<name_v<Ret>, chars<" (">, name_v<Obj>, chars<"::*)(">, name_v<Args>..., chars<")">>;
   };
}
