// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/reflection/get_name.hpp"
#include "glaze/reflection/to_tuple.hpp"

namespace glz
{
   namespace detail
   {
      template <fixed_string Name>
      struct named final
      {
         static constexpr std::string_view name = Name;
      };

      template <class T>
      extern const T external;

      template <auto Ptr>
      [[nodiscard]] consteval auto member_name_impl() -> std::string_view
      {
         // const auto name = std::string_view{std::source_location::current().function_name()};
         const std::string_view name = GLZ_PRETTY_FUNCTION;
#if defined(__clang__)
         const auto split = name.substr(0, name.find("}]"));
         return split.substr(split.find_last_of(".") + 1);
#elif defined(__GNUC__)
         const auto split = name.substr(0, name.find(")}"));
         return split.substr(split.find_last_of(":") + 1);
#elif defined(_MSC_VER)
         const auto split = name.substr(0, name.find_last_of("}"));
         return split.substr(split.find_last_of(">") + 1);
#endif
      }

      template <auto N>
      [[nodiscard]] consteval auto nth(auto... args)
      {
         return [&]<std::size_t... Ns>(std::index_sequence<Ns...>) {
            return [](decltype((void*)Ns)..., auto* nth, auto*...) { return *nth; }(&args...);
         }(std::make_index_sequence<N>{});
      }

      template <class T, auto N>
      [[nodiscard]] consteval auto member_name()
      {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
         constexpr auto name = member_name_impl<get_ptr<N>(external<T>)>();
#pragma clang diagnostic pop
#elif __GNUC__
         constexpr auto name = member_name_impl<get_ptr<N>(external<T>)>();
#else
         constexpr auto name = member_name_impl<get_ptr<N>(external<T>)>();
#endif
         return [&]<auto... Ns>(std::index_sequence<Ns...>) {
            return fixed_string<sizeof...(Ns)>{name[Ns]...};
         }(std::make_index_sequence<name.size()>{});
      }

      template <class T, size_t... I>
      [[nodiscard]] constexpr auto member_names_impl(std::index_sequence<I...>)
      {
         return std::make_tuple(named<member_name<T, I>()>{}...);
      }

      template <class T>
      [[nodiscard]] constexpr auto member_names()
      {
         return member_names_impl<T>(std::make_index_sequence<count_members<T>()>{});
      }

      template <class Tuple>
      using reflection_value_tuple_variant_t = typename tuple_ptr_variant<Tuple>::type;

      template <class T, bool use_hash_comparison, size_t... I>
      constexpr auto make_reflection_map_impl(std::index_sequence<I...>)
      {
         using V = decltype(to_tuple(std::declval<T>()));
         constexpr auto n = std::tuple_size_v<V>;
         [[maybe_unused]] constexpr auto members = member_names<T>();
         static_assert(count_members<T>() == n);

         using value_t = reflection_value_tuple_variant_t<V>;

         auto naive_or_normal_hash = [&] {
            if constexpr (n <= 20) {
               return glz::detail::naive_map<value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{get<I>(members).name, std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
            else {
               return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(
                  {std::pair<sv, value_t>{get<I>(members).name, std::add_pointer_t<std::tuple_element_t<I, V>>{}}...});
            }
         };

         return naive_or_normal_hash();
      }

      template <class T, bool use_hash_comparison = false>
      constexpr auto make_reflection_map()
         requires(!glaze_t<T> && !array_t<T> && std::is_aggregate_v<std::remove_cvref_t<T>>)
      {
         using V = decltype(to_tuple(std::declval<T>()));

         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<V>>{};
         return make_reflection_map_impl<std::decay_t<T>, use_hash_comparison>(indices);
      }
   }
}
