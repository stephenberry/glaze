// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

namespace glaze
{
   template <class T>
   concept is_tuple = is_specialization_v<T, std::tuple>;
   
   inline constexpr auto size_impl(auto&& t) {
       return std::tuple_size_v<std::decay_t<decltype(t)>>;
   }

   template <class T>
   inline constexpr size_t size_v = std::tuple_size_v<std::decay_t<T>>;
   
   namespace detail
   {
      template <size_t Offset, class Tuple, std::size_t...Is>
      auto tuple_split_impl(Tuple&& tuple, std::index_sequence<Is...>) {
         return std::make_tuple(std::get<Is * 2 + Offset>(tuple)...);
      }
   }
   
   template <class Tuple, std::size_t...Is>
   auto tuple_split(Tuple&& tuple) {
      static constexpr auto N = std::tuple_size_v<Tuple>;
      static constexpr auto is = std::make_index_sequence<N / 2>{};
      return std::make_pair(detail::tuple_split_impl<0>(tuple, is), detail::tuple_split_impl<1>(tuple, is));
   }
   
   
   // group builder code
   template <auto >
   struct value {
   };

   template <auto... Vals>
   struct value_sequence {
       using index_sequence = std::index_sequence<Vals...>;

       static constexpr auto to_array() {
           return std::to_array({Vals...});
       }
   };

   template <size_t...Is>
   constexpr auto make_value_sequence(std::index_sequence<Is...>)
   {
       return value_sequence<Is...>{};
   }

   template <auto... As, auto... Bs>
   constexpr value_sequence<As..., Bs...> operator+(value_sequence<As...>,
                                                   value_sequence<Bs...> )
   {
       return {};
   }

   template <auto& Tuple, auto Val, class F>
   constexpr auto filter_single(value<Val>, F pred) {
       if constexpr (pred(std::get<Val>(Tuple))) {
           return value_sequence<Val>{};
       }
       else {
           return value_sequence<>{};
       }
   }

   template <auto& Tuple, auto... Vals, class F>
   constexpr auto filter(value_sequence<Vals...>, F pred) {
       return (filter_single<Tuple>(value<Vals>{}, pred) + ...);
   }

   template <auto Val0, auto Val1>
   constexpr auto difference(value<Val0>, value<Val1>)
   {
       return Val1 - Val0;
   }

   template <size_t TotalSize, auto... Vals>
   constexpr auto group_sizes(value_sequence<Vals...>) {
       constexpr auto indices = std::to_array({Vals...});
       constexpr auto N = sizeof...(Vals);
       std::array<size_t, N> diffs;
       
       for (size_t i = 0; i < N - 1; ++i) {
           diffs[i] = indices[i + 1] - indices[i];
       }
       diffs[N - 1] = TotalSize - indices[N - 1];
       return diffs;
   }

   template <size_t Start, class Tuple, size_t... Is>
   constexpr auto make_group(Tuple&& t, std::index_sequence<Is...>)
   {
       return std::make_tuple(std::get<Start + Is>(t)...);
   }

   template <auto& GroupStartArr, auto& GroupSizeArr, class Tuple, size_t... GroupNumber>
   constexpr auto make_groups_impl(Tuple&& t, std::index_sequence<GroupNumber...>)
   {
       return std::make_tuple(make_group<std::get<GroupNumber>(GroupStartArr)>(t, std::make_index_sequence<std::get<GroupNumber>(GroupSizeArr)>{})...);
   }

   template <auto& Tuple>
   constexpr auto make_groups_helper()
   {
       constexpr auto N = std::tuple_size_v<std::decay_t<decltype(Tuple)>>;

       constexpr auto filtered = filter<Tuple>(
       make_value_sequence(std::make_index_sequence<N>()),
       [](auto&& v) constexpr {
           using V = std::decay_t<decltype(v)>;
           if constexpr (std::convertible_to<std::decay_t<V>, std::string_view>) {
               return true;
           }
           else {
               return false;
           }
       });

       constexpr auto sizes = group_sizes<N>(filtered);
       constexpr auto starts = filtered.to_array();

       return std::tuple{ starts, sizes };
   }

   template <auto& Tuple>
   struct group_builder
   {
       static constexpr auto h = make_groups_helper<Tuple>();
       static constexpr auto starts = std::get<0>(h);
       static constexpr auto sizes = std::get<1>(h);

       static constexpr auto op()
       {
           constexpr auto n_groups = starts.size();
           return make_groups_impl<starts, sizes>(Tuple, std::make_index_sequence<n_groups>{});
       }
   };
}
