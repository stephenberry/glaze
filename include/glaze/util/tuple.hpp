// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_view.hpp"

namespace glz
{
   template <class T>
   concept is_std_tuple = is_specialization_v<T, std::tuple>;

   inline constexpr auto size_impl(auto&& t) { return std::tuple_size_v<std::decay_t<decltype(t)>>; }

   template <class T>
   inline constexpr size_t size_v = std::tuple_size_v<std::decay_t<T>>;

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
      static constexpr auto N = std::tuple_size_v<Tuple>;
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
      constexpr auto n = std::tuple_size_v<Tuple>;
      std::array<size_t, n> indices{};
      size_t i = 0;
      for_each<n>([&](auto I) {
         using V = std::decay_t<std::tuple_element_t<I, Tuple>>;
         if constexpr (!(std::convertible_to<V, std::string_view> || is_schema_class<V>)) {
            indices[i++] = I - 1;
         }
      });
      return std::make_pair(indices, i);
   }

   namespace detail
   {
      template <class Func, class Tuple, std::size_t... Is>
      inline constexpr auto map_tuple(Func&& f, Tuple&& tuple, std::index_sequence<Is...>)
      {
         return tuplet::make_tuple(f(tuplet::get<Is>(tuple))...);
      }
   }

   template <class Func, class Tuple>
   inline constexpr auto map_tuple(Func&& f, Tuple&& tuple)
   {
      constexpr auto N = std::tuple_size_v<std::decay_t<Tuple>>;
      return detail::map_tuple(f, tuple, std::make_index_sequence<N>{});
   }

   template <class M>
   inline constexpr void check_member()
   {
      constexpr auto N = std::tuple_size_v<M>;

      static_assert(N == 0 || N > 1, "members need at least a name and a member pointer");
      static_assert(N < 4, "only member_ptr (or enum, or lambda), name, and comment are supported at the momment");

      if constexpr (N > 0)
         static_assert(sv_convertible<std::tuple_element_t<0, M>>, "first element should be the name");
      /*if constexpr (N > 1) {
       TODO: maybe someday add better error messaging that handles lambdas
         using E = std::tuple_element_t<1, M>;
         static_assert(std::is_member_pointer_v<E> || std::is_enum_v<E>>,
                       "second element should be the member pointer");
      }*/
      if constexpr (N > 2)
         static_assert(sv_convertible<std::tuple_element_t<2, M>>, "third element should be a string comment");
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
      auto get_elem = [&](auto i) {
         constexpr auto I = decltype(i)::value;
         using type = decltype(glz::tuplet::get<Start + I>(t));
         if constexpr (I == 0 || std::convertible_to<type, std::string_view>) {
            return std::string_view(glz::tuplet::get<Start + I>(t));
         }
         else {
            return glz::tuplet::get<Start + I>(t);
         }
      };
      auto r = glz::tuplet::make_copy_tuple(get_elem(std::integral_constant<size_t, Is>{})...);
      // check_member<decltype(r)>();
      return r;
   }

   template <auto& GroupStartArr, auto& GroupSizeArr, class Tuple, size_t... GroupNumber>
   constexpr auto make_groups_impl(Tuple&& t, std::index_sequence<GroupNumber...>)
   {
      return glz::tuplet::make_copy_tuple(make_group<get<GroupNumber>(GroupStartArr)>(
         t, std::make_index_sequence<std::get<GroupNumber>(GroupSizeArr)>{})...);
   }

   template <class Tuple>
   constexpr auto make_groups_helper()
   {
      constexpr auto N = std::tuple_size_v<Tuple>;

      constexpr auto filtered = filter<Tuple>();
      constexpr auto starts = shrink_index_array<filtered.second>(filtered.first);
      constexpr auto sizes = group_sizes(starts, N);

      return glz::tuplet::make_tuple(starts, sizes);
   }

   template <class Tuple>
   struct group_builder
   {
      static constexpr auto h = make_groups_helper<Tuple>();
      static constexpr auto starts = glz::tuplet::get<0>(h);
      static constexpr auto sizes = glz::tuplet::get<1>(h);

      static constexpr auto op(Tuple&& t)
      {
         constexpr auto n_groups = starts.size();
         return make_groups_impl<starts, sizes>(std::forward<Tuple>(t), std::make_index_sequence<n_groups>{});
      }
   };
}
