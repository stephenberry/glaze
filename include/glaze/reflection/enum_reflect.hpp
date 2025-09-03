// Glaze Library
// For the license information refer to glaze.hpp

// Enum Reflection

// Original code refactored from: https://github.com/ZXShady/enchantum (MIT License)

#pragma once

#include <array>
#include <cassert>
#include <climits>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

inline constexpr auto enum_min_range = -256;
inline constexpr auto enum_max_range = 256;

namespace glz
{
   template <class T>
   concept is_enum = std::is_enum_v<T>;

   template <class T>
   concept signed_enum = is_enum<T> && std::signed_integral<std::underlying_type_t<T>>;

   template <class T>
   concept unsigned_enum = is_enum<T> && !signed_enum<T>;

   template <class T>
   concept scoped_enum = is_enum<T> && (!std::is_convertible_v<T, std::underlying_type_t<T>>);

   template <class T>
   concept unscoped_enum = is_enum<T> && !scoped_enum<T>;

   template <class E, class Underlying>
   concept enum_of_underlying = is_enum<E> && std::same_as<std::underlying_type_t<E>, Underlying>;

   template <class T>
   concept enum_fixed_underlying = is_enum<T> && requires { T{0}; };

   template <class T>
   struct enum_traits;

   template <signed_enum E>
   struct enum_traits<E>
   {
     private:
      using U = std::underlying_type_t<E>;
      using L = std::numeric_limits<U>;

     public:
      static constexpr std::size_t prefix_length = 0;

      static constexpr auto min = (L::min)() > enum_min_range ? (L::min)() : enum_min_range;
      static constexpr auto max = (L::max)() < enum_max_range ? (L::max)() : enum_max_range;
   };

   template <unsigned_enum E>
   struct enum_traits<E>
   {
     private:
      using T = std::underlying_type_t<E>;
      using L = std::numeric_limits<T>;

     public:
      static constexpr std::size_t prefix_length = 0;

      static constexpr auto min = []() {
         if constexpr (std::is_same_v<T, bool>)
            return false;
         else
            return (enum_min_range) < 0 ? 0 : (enum_min_range);
      }();
      static constexpr auto max = []() {
         if constexpr (std::is_same_v<T, bool>)
            return true;
         else
            return (L::max)() < (enum_max_range) ? (L::max)() : (enum_max_range);
      }();
   };
}

namespace glz::detail
{
   template <class E, class = void>
   inline constexpr std::size_t prefix_length_or_zero = 0;

   template <class E>
   inline constexpr auto prefix_length_or_zero<E, decltype((void)enum_traits<E>::prefix_length)> =
      std::size_t{enum_traits<E>::prefix_length};

   template <class Enum, auto Min, decltype(Min) Max>
   constexpr auto generate_arrays()
   {
      static_assert(Min < Max, "enum_traits::min must be less than enum_traits::max");
      std::array<Enum, (Max - Min) + 1> array;
      for (std::size_t i = 0; i < array.size(); ++i) {
#if defined(__clang__) && __clang_major__ >= 16
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#endif
         array[i] = static_cast<Enum>(static_cast<decltype(Min)>(i) + Min);
#if defined(__clang__) && __clang_major__ >= 16
#pragma clang diagnostic pop
#endif
      }
      return array;
   }
}

#if defined(__clang__)

#if __clang_major__ < 20
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#endif

namespace glz
{
   namespace detail
   {
#if __clang_major__ >= 20

      template <class T, auto V, class = void>
      inline constexpr bool is_valid_cast = false;

      template <class T, auto V>
      inline constexpr bool is_valid_cast<T, V, std::void_t<std::integral_constant<T, static_cast<T>(V)>>> = true;

      template <class T, std::underlying_type_t<T> max_range = 1>
      constexpr auto valid_cast_range()
      {
         if constexpr (max_range >= 0) {
            if constexpr (max_range <= enum_max_range) {
               // this tests whether `static_cast`ing max_range is valid
               // because C style enums stupidly is like a bit field
               // `enum E { a,b,c,d = 3};` is like a bitfield `struct E { int val : 2;}`
               // which means giving E.val a larger than 2 bit value is UB so is it for enums
               // and gcc and msvc ignore this (for good)
               // while clang makes it a subsituation failure which we can check for
               // using std::inegral_constant makes sure this is a constant expression situation
               // for SFINAE to occur
               if constexpr (is_valid_cast<T, max_range>)
                  return valid_cast_range<T, max_range * 2>();
               else
                  return max_range - 1;
            }
            else {
               return max_range - 1;
            }
         }
         else {
            if constexpr (max_range >= enum_min_range) {
               // this tests whether `static_cast`ing max_range is valid
               // because C style enums stupidly is like a bit field
               // `enum E { a,b,c,d = 3};` is like a bitfield `struct E { int val : 2;}`
               // which means giving E.val a larger than 2 bit value is UB so is it for enums
               // and gcc and msvc ignore this (for good)
               // while clang makes it a subsituation failure which we can check for
               // using std::inegral_constant makes sure this is a constant expression situation
               // for SFINAE to occur
               if constexpr (is_valid_cast<T, max_range>)
                  return valid_cast_range<T, max_range * 2>();
               else
                  return max_range / 2;
            }
            else {
               return max_range / 2;
            }
         }
      }
#else
      template <class T, std::underlying_type_t<T> max_range = 1>
      constexpr auto valid_cast_range()
      {
         if constexpr (max_range >= 0)
            return enum_max_range;
         else
            return enum_min_range;
      }
#endif
   }

   template <unscoped_enum E>
      requires signed_enum<E> && (!enum_fixed_underlying<E>)
   struct enum_traits<E>
   {
      static constexpr auto max = detail::valid_cast_range<E>();
      static constexpr decltype(max) min = detail::valid_cast_range<E, -1>();
   };

   template <unscoped_enum E>
      requires unsigned_enum<E> && (!enum_fixed_underlying<E>)
   struct enum_traits<E>
   {
      static constexpr auto max = detail::valid_cast_range<E>();
      static constexpr decltype(max) min = 0;
   };

   namespace detail
   {

      template <class _>
      constexpr auto type_name_func() noexcept
      {
         // constexpr auto f() [with _ = Scoped]
         // return __PRETTY_FUNCTION__;
         constexpr auto funcname =
            std::string_view(__PRETTY_FUNCTION__ + (sizeof("auto glz::detail::type_name_func() [_ = ") - 1));
         // (sizeof("auto __cdecl glz::detail::type_name_func<") - 1)
         constexpr auto size = funcname.size() - (sizeof("]") - 1);
         std::array<char, size> ret;
         const auto* const funcname_data = funcname.data();
         for (std::size_t i = 0; i < size; ++i) ret[i] = funcname_data[i];
         return ret;
      }

      template <class T>
      inline constexpr auto type_name = type_name_func<T>();

      template <auto Enum>
      constexpr auto enum_in_array_name() noexcept
      {
         // constexpr auto f() [with auto _ = (
         // constexpr auto f() [Enum = (Scoped)0]
         std::string_view s = __PRETTY_FUNCTION__ + (sizeof("auto glz::detail::enum_in_array_name() [Enum = ") - 1);
         s.remove_suffix(sizeof("]") - 1);

         if constexpr (scoped_enum<decltype(Enum)>) {
            if (s[s.size() - 2] == ')') {
               s.remove_prefix(sizeof("(") - 1);
               s.remove_suffix(sizeof(")0") - 1);
               return s;
            }
            else {
               return s.substr(0, s.rfind("::"));
            }
         }
         else {
            if (s[s.size() - 2] == ')') {
               s.remove_prefix(sizeof("(") - 1);
               s.remove_suffix(sizeof(")0") - 1);
            }
            if (const auto pos = s.rfind("::"); pos != s.npos) return s.substr(0, pos);
            return std::string_view();
         }
      }

      template <auto... Vs>
      constexpr auto var_name() noexcept
      {
         // "auto glz::detail::var_name() [Vs = <(A)0, a, b, c, e, d, (A)6>]"
#define SZC(x) (sizeof(x) - 1)
         constexpr auto funcsig_off = SZC("auto glz::detail::var_name() [Vs = <");
         return std::string_view(__PRETTY_FUNCTION__ + funcsig_off, SZC(__PRETTY_FUNCTION__) - funcsig_off - SZC(">]"));
#undef SZC
      }

      template <auto Copy>
      inline constexpr auto static_storage_for = Copy;

      template <class E, class Pair, bool ShouldNullTerminate>
      constexpr auto reflect() noexcept
      {
         constexpr auto Min = enum_traits<E>::min;
         constexpr auto Max = enum_traits<E>::max;

         constexpr auto elements = []() {
            constexpr auto Array = detail::generate_arrays<E, Min, Max>();
            auto str = [Array]<std::size_t... Idx>(std::index_sequence<Idx...>) {
               return detail::var_name<Array[Idx]...>();
            }(std::make_index_sequence<Array.size()>());

            struct RetVal
            {
               std::array<Pair, decltype(Array){}.size()> pairs{};
               std::size_t total_string_length = 0;
               std::size_t valid_count = 0;
            } ret;

            std::size_t index = 0;
            constexpr auto enum_in_array_name = detail::enum_in_array_name<E{}>();
            constexpr auto enum_in_array_len = enum_in_array_name.size();

            // ((anonymous namespace)::A)0
            // (anonymous namespace)::a

            // this is needed to determine whether the above are cast expression if 2 braces are
            // next to eachother then it is a cast but only for anonymoused namespaced enums
            constexpr std::size_t index_check =
               !enum_in_array_name.empty() && enum_in_array_name.front() == '(' ? 1 : 0;
            while (index < Array.size()) {
               if (str[index_check] == '(') {
                  str.remove_prefix(sizeof("(") - 1 + enum_in_array_len + sizeof(")0") -
                                    1); // there is atleast 1 base 10 digit
                  // if(!str.empty())
                  //    std::cout << "after str \"" << str << '"' << '\n';
                  if (const auto commapos = str.find(','); commapos != str.npos) str.remove_prefix(commapos + 2);
                  // std::cout << "strsize \"" << str.size() << '"' << '\n';
               }
               else {
                  if constexpr (enum_in_array_len != 0) {
                     str.remove_prefix(enum_in_array_len + (sizeof("::") - 1));
                  }
                  if constexpr (detail::prefix_length_or_zero<E> != 0) {
                     str.remove_prefix(detail::prefix_length_or_zero<E>);
                  }
                  const auto commapos = str.find(',');

                  const auto name = str.substr(0, commapos);

                  ret.pairs[ret.valid_count] = Pair{Array[index], name};
                  ret.total_string_length += name.size() + ShouldNullTerminate;

                  if (commapos != str.npos) str.remove_prefix(commapos + 2); // skip comma and space
                  ++ret.valid_count;
               }
               ++index;
            }
            return ret;
         }();

         constexpr auto strings = [elements]() {
            std::array<char, elements.total_string_length> strings;
            for (std::size_t _i = 0, index = 0; _i < elements.valid_count; ++_i) {
               const auto& [_, s] = elements.pairs[_i];
               for (std::size_t i = 0; i < s.size(); ++i) strings[index++] = s[i];

               if constexpr (ShouldNullTerminate) strings[index++] = '\0';
            }
            return strings;
         }();

         std::array<Pair, elements.valid_count> ret;
         constexpr const auto* str = static_storage_for<strings>.data();
         for (std::size_t i = 0, string_index = 0; i < elements.valid_count; ++i) {
            const auto& [e, s] = elements.pairs[i];
            auto& [re, rs] = ret[i];
            re = e;

            rs = {str + string_index, str + string_index + s.size()};
            string_index += s.size() + ShouldNullTerminate;
         }
         return ret;
      }
   }
}

#if __clang_major__ < 20
#pragma clang diagnostic pop
#endif

#elif defined(__GNUC__) || defined(__GNUG__)

namespace glz
{

   namespace detail
   {

      template <class _>
      constexpr auto type_name_func() noexcept
      {
         // constexpr auto f() [with _ = Scoped]
         // return __PRETTY_FUNCTION__;
         constexpr auto funcname = std::string_view(
            __PRETTY_FUNCTION__ + (sizeof("constexpr auto glz::detail::type_name_func() [with _ = ") - 1));
         // (sizeof("auto __cdecl glz::detail::type_name_func<") - 1)
         constexpr auto size = funcname.size() - (sizeof("]") - 1);
         std::array<char, size> ret;
         auto* const ret_data = ret.data();
         const auto* const funcname_data = funcname.data();
         for (std::size_t i = 0; i < size; ++i) ret_data[i] = funcname_data[i];
         return ret;
      }

      template <class T>
      inline constexpr auto type_name = type_name_func<T>();

      template <auto Enum>
      constexpr auto enum_in_array_name() noexcept
      {
         // constexpr auto f() [with auto _ = (
         // constexpr auto f() [with auto _ = (Scoped)0]
         std::string_view s =
            __PRETTY_FUNCTION__ + sizeof("constexpr auto glz::detail::enum_in_array_name() [with auto Enum = ") - 1;
         s.remove_suffix(sizeof("]") - 1);

         if constexpr (scoped_enum<decltype(Enum)>) {
            if (s.front() == '(') {
               s.remove_prefix(1);
               s.remove_suffix(sizeof(")0") - 1);
               return s;
            }
            else {
               return s.substr(0, s.rfind("::"));
            }
         }
         else {
            if (s.front() == '(') {
               s.remove_prefix(1);
               s.remove_suffix(sizeof(")0") - 1);
            }
            if (const auto pos = s.rfind("::"); pos != s.npos) return s.substr(0, pos);
            return std::string_view();
         }
      }

      template <class Enum>
      constexpr auto length_of_enum_in_template_array_if_casting() noexcept
      {
         if constexpr (scoped_enum<Enum>) {
            return detail::enum_in_array_name<Enum{}>().size();
         }
         else {
            constexpr auto s = enum_in_array_name<Enum{}>().size();
            constexpr auto& tyname = type_name<Enum>;
            constexpr auto str = std::string_view(tyname.data(), tyname.size());
            if (constexpr auto pos = str.rfind("::"); pos != str.npos) {
               return s + str.substr(pos).size();
            }
            else {
               return s + tyname.size();
            }
         }
      }

      template <auto... Vs>
      constexpr auto var_name() noexcept
      {
         // constexpr auto f() [with auto _ = std::array<E, 6>{std::__array_traits<E, 6>::_Type{a, b, c, e, d, (E)6}}]
#define SZC(x) (sizeof(x) - 1)
         constexpr std::size_t funcsig_off = SZC("constexpr auto glz::detail::var_name() [with auto ...Vs = {");
         return std::std::string_view(__PRETTY_FUNCTION__ + funcsig_off,
                                      SZC(__PRETTY_FUNCTION__) - funcsig_off - SZC("}]"));
      }

      template <auto Copy>
      inline constexpr auto static_storage_for = Copy;

      template <class E, class Pair, bool ShouldNullTerminate>
      constexpr auto reflect() noexcept
      {
         constexpr auto Min = enum_traits<E>::min;
         constexpr auto Max = enum_traits<E>::max;

         constexpr auto elements = []() {
            constexpr auto length_of_enum_in_template_array_casting =
               detail::length_of_enum_in_template_array_if_casting<E>();
            constexpr auto Array = detail::generate_arrays<E, Min, Max>();
            auto str = [Array]<std::size_t... Idx>(std::index_sequence<Idx...>) {
               return detail::var_name<Array[Idx]...>();
            }(std::make_index_sequence<Array.size()>());
            struct RetVal
            {
               std::array<Pair, Array.size()> pairs{};
               std::size_t total_string_length = 0;
               std::size_t valid_count = 0;
            } ret;
            std::size_t index = 0;
            constexpr auto enum_in_array_len = enum_in_array_name<E{}>().size();
            while (index < Array.size()) {
               if (str.front() == '(') {
                  str.remove_prefix(sizeof("(") - 1 + length_of_enum_in_template_array_casting + sizeof(")0") -
                                    1); // there is atleast 1 base 10 digit
                  // if(!str.empty())
                  //    std::cout << "after str \"" << str << '"' << '\n';

                  if (const auto commapos = str.find(','); commapos != str.npos) str.remove_prefix(commapos + 2);

                  // std::cout << "strsize \"" << str.size() << '"' << '\n';
               }
               else {
                  if constexpr (enum_in_array_len != 0) str.remove_prefix(enum_in_array_len + sizeof("::") - 1);
                  if constexpr (detail::prefix_length_or_zero<E> != 0) {
                     str.remove_prefix(detail::prefix_length_or_zero<E>);
                  }
                  const auto commapos = str.find(',');

                  const auto name = str.substr(0, commapos);

                  ret.pairs[ret.valid_count] = Pair{Array[index], name};
                  ret.total_string_length += name.size() + ShouldNullTerminate;

                  if (commapos != str.npos) str.remove_prefix(commapos + 2);
                  ++ret.valid_count;
               }
               ++index;
            }
            return ret;
         }();

         constexpr auto strings = [elements]() {
            std::array<char, elements.total_string_length> strings;
            for (std::size_t _i = 0, index = 0; _i < elements.valid_count; ++_i) {
               const auto& [_, s] = elements.pairs[_i];
               for (std::size_t i = 0; i < s.size(); ++i) strings[index++] = s[i];

               if constexpr (ShouldNullTerminate) strings[index++] = '\0';
            }
            return strings;
         }();

         std::array<Pair, elements.valid_count> ret;
         constexpr const auto* str = static_storage_for<strings>.data();
         for (std::size_t i = 0, string_index = 0; i < elements.valid_count; ++i) {
            const auto& [e, s] = elements.pairs[i];
            auto& [re, rs] = ret[i];
            re = e;

            rs = {str + string_index, str + string_index + s.size()};
            string_index += s.size() + ShouldNullTerminate;
         }
         return ret;
      }

   }

}
#elif defined(_MSC_VER)

namespace glz
{
   namespace detail
   {
#define SZC(x) (sizeof(x) - 1)
      template <class>
      constexpr auto type_name_func_size() noexcept
      {
         // (sizeof("auto __cdecl glz::detail::type_name_func<") - 1)
         return SZC(__FUNCSIG__) - SZC("auto __cdecl glz::detail::type_name_func_size<enum ") - SZC(">(void) noexcept");
      }

      template <auto Enum>
      constexpr auto enum_in_array_name() noexcept
      {
         std::string_view s = __FUNCSIG__ + sizeof("auto __cdecl glz::detail::enum_in_array_name<") - 1;
         s.remove_suffix(sizeof(">(void) noexcept") - 1);

         if constexpr (scoped_enum<decltype(Enum)>) {
            if (s.front() == '(') {
               s.remove_prefix(sizeof("(enum ") - 1);
               s.remove_suffix(sizeof(")0x0") - 1);
               return s;
            }
            return s.substr(0, s.rfind("::"));
         }
         else {
            if (s.front() == '(') {
               s.remove_prefix(sizeof("(enum ") - 1);
               s.remove_suffix(sizeof(")0x0") - 1);
            }
            if (const auto pos = s.rfind("::"); pos != s.npos) return s.substr(0, pos);
            return std::string_view();
         }
      }

      template <auto Array>
      constexpr auto var_name() noexcept
      {
         // auto __cdecl f<class std::array<enum `anonymous namespace'::UnscopedAnon,32>{enum
         // `anonymous-namespace'::UnscopedAnon

         using T = typename decltype(Array)::value_type;
#define SZC(x) (sizeof(x) - 1)
         std::size_t funcsig_off = SZC("auto __cdecl glz::detail::var_name<class std::array<enum ");
         constexpr auto type_name_len = glz::detail::type_name_func_size<T>();
         funcsig_off += type_name_len + SZC(",");
         constexpr auto Size = Array.size();
         // clang-format off
         funcsig_off += Size < 10 ? 1
         : Size < 100 ? 2
         : Size < 1000 ? 3
         : Size < 10000 ? 4
         : Size < 100000 ? 5
         : Size < 1000000 ? 6
         : Size < 10000000 ? 7
         : Size < 100000000 ? 8
         : Size < 1000000000 ? 9
         : 10;
         // clang-format on
         funcsig_off += SZC(">{enum ") + type_name_len;
         return std::string_view(__FUNCSIG__ + funcsig_off,
                                 SZC(__FUNCSIG__) - funcsig_off - (sizeof("}>(void) noexcept") - 1));
      }
#undef SZC

      template <auto Copy>
      inline constexpr auto static_storage_for = Copy;

      template <class E, class Pair, auto Array, bool ShouldNullTerminate>
      constexpr auto get_elements()
      {
         constexpr auto type_name_len = detail::type_name_func_size<E>();

         auto str = var_name<Array>();
         struct RetVal
         {
            std::array<Pair, Array.size()> pairs{};
            std::size_t total_string_length = 0;
            std::size_t valid_count = 0;
         } ret;
         std::size_t index = 0;
         constexpr auto enum_in_array_len = detail::enum_in_array_name<E{}>().size();
         while (index < Array.size()) {
            if (str.front() == '(') {
               str.remove_prefix(sizeof("(enum ") - 1 + type_name_len + sizeof(")0x0") -
                                 1); // there is atleast 1 base 16 hex digit

               if (const auto commapos = str.find(','); commapos != str.npos) str.remove_prefix(commapos + 1);
            }
            else {
               if constexpr (enum_in_array_len != 0) str.remove_prefix(enum_in_array_len + sizeof("::") - 1);

               if constexpr (detail::prefix_length_or_zero<E> != 0) str.remove_prefix(detail::prefix_length_or_zero<E>);

               const auto commapos = str.find(',');

               const auto name = str.substr(0, commapos);

               ret.pairs[ret.valid_count] = Pair{Array[index], name};
               ret.total_string_length += name.size() + ShouldNullTerminate;

               if (commapos != str.npos) str.remove_prefix(commapos + 1);
               ++ret.valid_count;
            }
            ++index;
         }
         return ret;
      }

      template <class E, class Pair, bool ShouldNullTerminate>
      constexpr auto reflect() noexcept
      {
         constexpr auto Min = enum_traits<E>::min;
         constexpr auto Max = enum_traits<E>::max;

         constexpr auto elements =
            detail::get_elements<E, Pair, detail::generate_arrays<E, Min, Max>(), ShouldNullTerminate>();

         constexpr auto strings = [elements]() {
            std::array<char, elements.total_string_length> strings;
            for (std::size_t _i = 0, index = 0; _i < elements.valid_count; ++_i) {
               const auto& [_, s] = elements.pairs[_i];
               for (std::size_t i = 0; i < s.size(); ++i) strings[index++] = s[i];

               if constexpr (ShouldNullTerminate) strings[index++] = '\0';
            }
            return strings;
         }();

         std::array<Pair, elements.valid_count> ret;
         constexpr const auto* str = static_storage_for<strings>.data();
         for (std::size_t i = 0, string_index = 0; i < elements.valid_count; ++i) {
            const auto& [e, s] = elements.pairs[i];
            auto& [re, rs] = ret[i];
            re = e;

            rs = {str + string_index, str + string_index + s.size()};
            string_index += s.size() + ShouldNullTerminate;
         }

         return ret;
      }
   }

}

#endif

namespace glz
{

#ifdef __cpp_lib_to_underlying
   using ::std::to_underlying;
#else
   template <Enum E>
   [[nodiscard]] constexpr auto to_underlying(const E e) noexcept
   {
      return static_cast<std::underlying_type_t<E>>(e);
   }
#endif

   template <is_enum E, class Pair = std::pair<E, std::string_view>, bool ShouldNullTerminate = true>
   inline constexpr auto enums = detail::reflect<std::remove_cv_t<E>, Pair, ShouldNullTerminate>();

   template <is_enum E>
   inline constexpr auto values = []() {
      constexpr auto& values = enums<E>;
      std::array<E, values.size()> ret;
      for (std::size_t i = 0; i < ret.size(); ++i) ret[i] = values[i].first;
      return ret;
   }();

   template <is_enum E, class String = std::string_view, bool NullTerminated = true>
   inline constexpr auto names = []() {
      constexpr auto& values = enums<E, std::pair<E, String>, NullTerminated>;
      std::array<String, values.size()> ret;
      for (std::size_t i = 0; i < ret.size(); ++i) ret[i] = values[i].second;
      return ret;
   }();

   template <is_enum E>
   inline constexpr auto min = enums<E>.front().first;

   template <is_enum E>
   inline constexpr auto max = enums<E>.back().first;

   template <is_enum E>
   inline constexpr std::size_t count = enums<E>.size();
}

namespace glz
{
   template <class>
   inline constexpr bool is_contiguous = false;

   template <is_enum E>
   inline constexpr bool is_contiguous<E> = []() {
      using T = std::underlying_type_t<E>;
      if constexpr (std::is_same_v<T, bool>) {
         return true;
      }
      else {
         constexpr auto& values = enums<E>;
         for (std::size_t i = 0; i < values.size() - 1; ++i)
            if (T(values[i].first) + 1 != T(values[i + 1].first)) return false;
         return true;
      }
   }();

   template <class E>
   concept ContiguousEnum = is_enum<E> && is_contiguous<E>;

   template <is_enum E>
   [[nodiscard]] constexpr bool contains(const E value) noexcept
   {
      for (const auto v : values<E>) {
         if (v == value) {
            return true;
         }
      }
      return false;
   }

   template <is_enum E>
   [[nodiscard]] constexpr bool contains(const std::underlying_type_t<E> value) noexcept
   {
      return glz::contains(static_cast<E>(value));
   }

   template <is_enum E>
   [[nodiscard]] constexpr bool contains(const std::string_view name) noexcept
   {
      for (const auto& s : names<E>)
         if (s == name) return true;
      return false;
   }

   template <is_enum E, std::predicate<std::string_view, std::string_view> BinaryPredicate>
   [[nodiscard]] constexpr bool contains(const std::string_view name, const BinaryPredicate binary_predicate) noexcept
   {
      for (const auto& s : names<E>)
         if (binary_predicate(name, s)) return true;
      return false;
   }

   template <ContiguousEnum E>
   [[nodiscard]] constexpr bool contains(const E value) noexcept
   {
      using T = std::underlying_type_t<E>;
      return T(value) <= T(max<E>) && T(value) >= T(min<E>);
   }

   namespace detail
   {
      template <class E>
      struct index_to_enum_functor
      {
         [[nodiscard]] constexpr std::optional<E> operator()(const std::size_t index) const noexcept
         {
            std::optional<E> ret;
            if (index < values<E>.size()) ret.emplace(values<E>[index]);
            return ret;
         }
      };

      struct enum_to_index_functor
      {
         template <is_enum E>
         [[nodiscard]] constexpr std::optional<std::size_t> operator()(const E e) const noexcept
         {
            if constexpr (ContiguousEnum<E>) {
               using T = std::underlying_type_t<E>;
               if (glz::contains(e)) return std::optional<std::size_t>(std::size_t(T(e) - T(min<E>)));
            }
            else {
               for (std::size_t i = 0; i < values<E>.size(); ++i)
                  if (values<E>[i] == e) return std::optional<std::size_t>(i);
            }
            return std::optional<std::size_t>();
         }
      };

      template <is_enum E>
      struct cast_functor
      {
         [[nodiscard]] constexpr std::optional<E> operator()(const std::underlying_type_t<E> value) const noexcept
         {
            std::optional<E> a; // rvo not that it really matters
            if (!glz::contains<E>(value)) return a;
            a.emplace(static_cast<E>(value));
            return a;
         }

         [[nodiscard]] constexpr std::optional<E> operator()(const std::string_view name) const noexcept
         {
            std::optional<E> a; // rvo not that it really matters
            for (const auto& [e, s] : enums<E>) {
               if (s == name) {
                  a.emplace(e);
                  return a;
               }
            }
            return a; // nullopt
         }

         template <std::predicate<std::string_view, std::string_view> BinaryPred>
         [[nodiscard]] constexpr std::optional<E> operator()(const std::string_view name,
                                                             const BinaryPred binary_predicate) const noexcept
         {
            std::optional<E> a; // rvo not that it really matters
            for (const auto& [e, s] : enums<E>) {
               if (binary_predicate(name, s)) {
                  a.emplace(e);
                  return a;
               }
            }
            return a;
         }
      };

   }

   template <is_enum E>
   inline constexpr detail::index_to_enum_functor<E> index_to_enum{};

   inline constexpr detail::enum_to_index_functor enum_to_index{};

   template <is_enum E>
   inline constexpr detail::cast_functor<E> cast_enum{};

   inline constexpr auto enum_name = []<is_enum E>(const E value) noexcept -> std::string_view {
      if (const auto i = glz::enum_to_index(value)) {
         return names<E>[*i];
      }
      return {};
   };
}
