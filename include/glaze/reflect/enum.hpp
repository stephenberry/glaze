#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>
#if __has_include(<source_location>)
#include <source_location>
#endif

// magic_enum style reflection based on willwray/enum_traits
// TODO: Depracate/Remove when we get proper static reflection
// Known Limitations:
//   * Nonstandard and likely only works on tested compilers
//     * Tested on gcc10-12, clang13-15, and msvc2019-2022
//   * Only tests 2048 values in the enum
//   * My be problematic with enums in templates
namespace glz::detail
{

#if defined(__FUNCSIG__)
#define GLAZE_FUNCTION_NAME_NONSTD __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
#define GLAZE_FUNCTION_NAME_NONSTD __PRETTY_FUNCTION__
#elif defined(__cpp_lib_source_location)
#define GLAZE_FUNCTION_NAME_NONSTD std::source_location::current().function_name()
#else
#define GLAZE_FUNCTION_NAME_NONSTD ""
#endif

   namespace impl
   {
      // anonymous namespace is used to prevent gcc from dropping the namespace on the
      // type name in certain instances
      namespace
      {
         template <class T>
         consteval std::string_view wrapped_type_name()
         {
            return GLAZE_FUNCTION_NAME_NONSTD;
         }
      }  // namespace

      consteval std::size_t wrapped_type_name_prefix_length() { return wrapped_type_name<void>().find("void"); }

      consteval std::size_t wrapped_type_name_suffix_length()
      {
         return wrapped_type_name<void>().size() - wrapped_type_name_prefix_length() - 4;
      }
   }  // namespace impl

   template <class T>
   consteval std::string_view type_name()
   {
      constexpr auto wrapped_name = impl::wrapped_type_name<T>();
      constexpr auto prefix_length = impl::wrapped_type_name_prefix_length();
      constexpr auto suffix_length = impl::wrapped_type_name_suffix_length();
      constexpr auto type_name_length = wrapped_name.size() - prefix_length - suffix_length;
      constexpr auto name = wrapped_name.substr(prefix_length, type_name_length);
      if constexpr (name.starts_with("enum ")) {
         return name.substr(5, name.size() - 5);
      }
      else {
         return name;
      }
   }

   namespace impl
   {
      enum class Foo { Bar };

      template <auto... V>
      consteval auto raw_enum_names()
      {
         // Non a string_view since with long strings some compilers have issues
         return GLAZE_FUNCTION_NAME_NONSTD;
      }

      template <auto... V>
      constexpr std::string_view wrapped_enum_name()
      {
         return {raw_enum_names<V...>()};
      }

      consteval std::size_t wrapped_enum_prefix_length()
      {
         return wrapped_enum_name<Foo::Bar>().find(type_name<Foo>());
      }

      template <class T>
      consteval size_t enum_val_prefix_length()
      {
         constexpr auto enum_name = type_name<T>();
         if constexpr (!std::is_convertible_v<T, std::underlying_type_t<T>>) {
            // scoped enum
            return enum_name.size() + 2;
         }
         else {
            // unscoped enum
            return enum_name.rfind(':') + 1;
         }
      }

      consteval std::size_t wrapped_enum_suffix_length()
      {
         return wrapped_enum_name<Foo::Bar>().size() - wrapped_enum_prefix_length() - enum_val_prefix_length<Foo>() - 3;
      }
   }  // namespace impl

   template <auto V>
      requires std::is_enum_v<decltype(V)>
   consteval std::string_view enum_name()
   {
      constexpr auto wrapped_name = impl::wrapped_enum_name<V>();
      constexpr auto prefix_length = impl::wrapped_enum_prefix_length() + impl::enum_val_prefix_length<decltype(V)>();
      constexpr auto suffix_length = impl::wrapped_enum_suffix_length();
      constexpr auto name_length = wrapped_name.length() - prefix_length - suffix_length;
      return wrapped_name.substr(prefix_length, name_length);
   }

   namespace impl
   {
      template <class T, int64_t Start, std::size_t... Is>
      consteval auto get_valid_enums(std::index_sequence<Is...>)
      {
         constexpr auto n = sizeof...(Is);
         std::array<T, n> valid_enums{};
         auto count = 0;
         auto it = raw_enum_names<T(Start + int(Is))...>();
         it += wrapped_enum_prefix_length();
         constexpr auto offset = enum_val_prefix_length<T>();
         auto validate_and_consume_prefix = [&](auto i) {
            bool valid = (*it != '(');
#if defined(_MSC_VER)
            if (!valid) it += 5;
#endif
            it += offset;
            // bool valid =*it > '9' || *it < '0';
            if (valid) {
               valid_enums[count++] = T(i + Start);
            }
         };
         validate_and_consume_prefix(0);
         for (size_t i = 1; i < n; ++i) {
            while (*it++ != ',')
               ;
#if !defined(_MSC_VER)
            ++it;
#endif
            validate_and_consume_prefix(i);
         }
         return std::pair{valid_enums, count};
      }

      template <class T, int64_t Start, std::size_t ChunkSize>
      consteval auto enum_index_array()
      {
         constexpr auto valid_enums = get_valid_enums<T, Start>(std::make_index_sequence<ChunkSize>{});
         constexpr auto num_enums = valid_enums.second;
         std::array<T, num_enums> res{};
         if constexpr (num_enums > 0) {
            for (size_t i = 0; i < num_enums; ++i) {
               res[i] = valid_enums.first[i];
            }
         }
         return res;
      }

      template <class T, std::size_t... Ns>
      consteval auto concat_arrays(const std::array<T, Ns>&... arrays)
      {
         std::array<T, (Ns + ...)> res;
         std::size_t index{};
         ((std::copy_n(arrays.begin(), Ns, res.begin() + index), index += Ns), ...);
         return res;
      }

      template <class T, int64_t Start, std::size_t ChunkSize, std::size_t... Is>
      consteval auto get_valid_enums_chunked(std::index_sequence<Is...>)
      {
         return concat_arrays(enum_index_array<T, Start + int64_t(ChunkSize * Is), ChunkSize>()...);
      }
   }  // namespace impl

   template <class T>
   // We cant check all posible values unless the underlying type is one byte.
   // Will check up to chunks * chunk_size values (2048)
   consteval auto enum_array()
   {
      using U = std::underlying_type_t<T>;
      constexpr size_t chunk_size = sizeof(U) == 1 ? 256 : 1024;
      constexpr size_t chunks = sizeof(U) == 1 ? 1 : 2;
      constexpr int64_t start = std::is_signed_v<U> ? -int64_t(chunk_size * chunks / 2) : 0;
      return impl::get_valid_enums_chunked<T, start, chunk_size>(std::make_index_sequence<chunks>{});
   }

#undef GLAZE_FUNCTION_NAME_NONSTD

}  // namespace glz::detail

#include "glaze/core/common.hpp"

namespace glz::detail
{
   template <class T, auto Vals, std::size_t... Is>
   consteval auto auto_enum(std::index_sequence<Is...>)
   {
      return glz::tuplet::make_tuple(glz::tuplet::make_tuple(enum_name<Vals[Is]>(), Vals[Is])...);
   }
   template <class T>
   consteval auto auto_enum()
   {
      constexpr auto vals = enum_array<T>();
      return auto_enum<T, vals>(std::make_index_sequence<vals.size()>{});
   }
   template <class T>
   concept is_enum = std::is_enum_v<T>;
}

template <glz::detail::is_enum E>
struct glz::meta<E>
{
   static constexpr sv name =
      glz::detail::type_name<E>();  // Should be consistant across gcc/MSVC/clang unless enum is in a template
   static constexpr auto value = glz::detail::Enum{glz::detail::auto_enum<E>()};
};
