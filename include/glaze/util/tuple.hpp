// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

#include "glaze/reflection/get_name.hpp"
#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   template <class T>
   concept is_std_tuple = is_specialization_v<T, std::tuple>;

   inline constexpr auto size_impl(auto&& t) { return glz::tuple_size_v<std::decay_t<decltype(t)>>; }

   template <class T>
   inline constexpr size_t size_v = glz::tuple_size_v<std::decay_t<T>>;

   namespace detail
   {
      template <size_t Offset, class Tuple, std::size_t... Is>
      auto tuple_split_impl(Tuple&& tuple, std::index_sequence<Is...>)
      {
         return std::make_tuple(std::get<Is * 2 + Offset>(tuple)...);
      }
   }

   template <class Tuple, std::size_t... Is>
   auto tuple_split(Tuple&& tuple)
   {
      static constexpr auto N = glz::tuple_size_v<Tuple>;
      static constexpr auto is = std::make_index_sequence<N / 2>{};
      return std::make_pair(detail::tuple_split_impl<0>(tuple, is), detail::tuple_split_impl<1>(tuple, is));
   }

   // group builder code
   template <size_t N>
   constexpr auto shrink_index_array(auto&& arr)
   {
      std::array<size_t, N> res{};
      for (size_t i = 0; i < N; ++i) {
         res[i] = arr[i];
      }
      return res;
   }

   template <class Type>
   concept is_schema_class = requires {
      requires std::is_class_v<Type>;
      requires Type::schema_attributes;
   };

   template <class Tuple>
   constexpr auto filter()
   {
      constexpr auto N = glz::tuple_size_v<Tuple>;
      std::array<uint64_t, N> indices{};
      size_t i = 0;
      for_each<N>([&](auto I) {
         using V = std::decay_t<glz::tuple_element_t<I, Tuple>>;
         if constexpr (std::is_member_pointer_v<V>) {
            if constexpr (I == 0) {
               indices[i++] = 0;
            }
            else if constexpr (std::convertible_to<glz::tuple_element_t<I - 1, Tuple>, std::string_view>) {
               // If the previous element in the tuple is convertible to a std::string_view, then we treat it as the key
               indices[i++] = I - 1;
            }
            else {
               indices[i++] = I;
            }
         }
         else if constexpr (std::is_enum_v<V>) {
            if constexpr (I == 0) {
               indices[i++] = 0;
            }
            else if constexpr (std::convertible_to<glz::tuple_element_t<I - 1, Tuple>, std::string_view>) {
               // If the previous element in the tuple is convertible to a std::string_view, then we treat it as the key
               indices[i++] = I - 1;
            }
            else {
               indices[i++] = I;
            }
         }
         else if constexpr (!(std::convertible_to<V, std::string_view> || is_schema_class<V> ||
                              std::same_as<V, comment>)) {
            indices[i++] = I - 1;
         }
      });
      return std::make_pair(indices, i); // indices for groups and the number of groups
   }

   namespace detail
   {
      template <class Func, class Tuple, std::size_t... Is>
      inline constexpr auto map_tuple(Func&& f, Tuple&& tuple, std::index_sequence<Is...>)
      {
         return tuplet::make_tuple(f(get<Is>(tuple))...);
      }
   }

   template <class Func, class Tuple>
   inline constexpr auto map_tuple(Func&& f, Tuple&& tuple)
   {
      constexpr auto N = glz::tuple_size_v<std::decay_t<Tuple>>;
      return detail::map_tuple(f, tuple, std::make_index_sequence<N>{});
   }

   template <size_t n_groups>
   constexpr auto group_sizes(const std::array<size_t, n_groups>& indices, size_t n_total)
   {
      std::array<size_t, n_groups> diffs;

      for (size_t i = 0; i < n_groups - 1; ++i) {
         diffs[i] = indices[i + 1] - indices[i];
      }
      diffs[n_groups - 1] = n_total - indices[n_groups - 1];
      return diffs;
   }

   template <size_t Start, class Tuple, size_t... Is>
   constexpr auto make_group(Tuple&& t, std::index_sequence<Is...>)
   {
      auto get_elem = [&](auto I) {
         constexpr auto Index = Start + I;
         using type = decltype(glz::get<Index>(t));
         using T = std::decay_t<type>;
         if constexpr (std::convertible_to<type, std::string_view>) {
            return std::string_view(glz::get<Index>(t));
         }
         else if constexpr (std::same_as<T, comment>) {
            return glz::get<Index>(t).value;
         }
         else {
            return glz::get<Index>(t);
         }
      };
      return glz::tuplet::tuple{get_elem(std::integral_constant<size_t, Is>{})...};
   }

   template <auto& GroupStartArr, auto& GroupSizeArr, class Tuple, size_t... GroupNumber>
   constexpr auto make_groups_impl(Tuple&& t, std::index_sequence<GroupNumber...>)
   {
      return glz::tuplet::make_tuple(make_group<get<GroupNumber>(GroupStartArr)>(
         t, std::make_index_sequence<std::get<GroupNumber>(GroupSizeArr)>{})...);
   }

   template <class Tuple>
   constexpr auto make_groups_helper()
   {
      constexpr auto N = glz::tuple_size_v<Tuple>;

      constexpr auto filtered = filter<Tuple>();
      constexpr auto starts = shrink_index_array<filtered.second>(filtered.first);
      constexpr auto sizes = group_sizes(starts, N);

      return glz::tuplet::make_tuple(starts, sizes);
   }

   template <class Tuple>
   struct group_builder
   {
      static constexpr auto h = make_groups_helper<Tuple>();
      static constexpr auto starts = glz::get<0>(h);
      static constexpr auto sizes = glz::get<1>(h);

      static constexpr auto op(Tuple&& t)
      {
         constexpr auto n_groups = starts.size();
         return make_groups_impl<starts, sizes>(std::forward<Tuple>(t), std::make_index_sequence<n_groups>{});
      }
   };
}
