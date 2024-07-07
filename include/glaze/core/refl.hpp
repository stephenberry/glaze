// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/reflection/reflect.hpp"

namespace glz
{
   // Get indices of elements satisfying a predicate
   template <class Tuple, template <class> class Predicate>
   consteval auto filter_indices() {
      return []<size_t... Is>(std::index_sequence<Is...>) constexpr {
         constexpr bool matches[] = {
            Predicate<glz::tuple_element_t<Is, Tuple>>::value...};
         constexpr size_t count = (matches[Is] + ...);
         
         std::array<size_t, count> indices{};
         size_t index = 0;
         ((void)((matches[Is] ? (indices[index++] = Is, true) : false)), ...);
         return indices;
      }(std::make_index_sequence<glz::tuple_size_v<Tuple>>{});
   }
   
   template <class T>
   concept is_object_key_type = std::convertible_to<T, std::string_view>;
   
   template <class T>
   using object_key_type = std::bool_constant<is_object_key_type<T>>;
   
   template <class T>
   using not_object_key_type = std::bool_constant<not is_object_key_type<T>>;
   
   template <class T, size_t I>
   consteval sv get_key_element() {
      using V = std::decay_t<T>;
      if constexpr (I == 0) {
         if constexpr (is_object_key_type<decltype(get<0>(meta_v<V>))>) {
            return get<0>(meta_v<V>);
         }
         else {
            return get_name<get<0>(meta_v<V>)>();
         }
      }
      else if constexpr (is_object_key_type<decltype(get<I - 1>(meta_v<V>))>) {
         return get<I - 1>(meta_v<V>);
      }
      else {
         return get_name<get<I>(meta_v<V>)>();
      }
   };
   
   template <class T>
   struct make_reflection_info;
   
   template <class T>
      requires detail::glaze_object_t<T>
   struct make_reflection_info<T>
   {
      using V = std::decay_t<T>;
      static constexpr auto value_indices = filter_indices<meta_t<V>, not_object_key_type>();
      
      static constexpr auto values = [] {
         return [&]<size_t... I>(std::index_sequence<I...>) { //
            return tuplet::tuple{ get<value_indices[I]>(meta_v<T>)... }; //
         }(std::make_index_sequence<value_indices.size()>{}); //
      }();
      
      static constexpr auto N = tuple_size_v<decltype(values)>;
      
      static constexpr auto keys = [] {
         std::array<sv, N> res{};
         [&]<size_t... I>(std::index_sequence<I...>) { //
            ((res[I] = get_key_element<T, value_indices[I]>()), ...);
         }(std::make_index_sequence<value_indices.size()>{});
         return res;
      }();
      
      template <size_t I>
      using type = detail::member_t<V, decltype(get<I>(values))>;
   };
   
   template <class T>
      requires detail::reflectable<T>
   struct make_reflection_info<T>
   {
      using V = std::decay_t<T>;
      using Tuple = decay_keep_volatile_t<decltype(detail::to_tuple(std::declval<T>()))>;
      
      static constexpr auto values = typename detail::tuple_ptr<Tuple>::type{};
      
      static constexpr auto keys = member_names<T>;
      static constexpr auto N = keys.size();
      
      template <size_t I>
      using type = detail::member_t<V, glz::tuple_element_t<I, Tuple>>;
   };
   
   template <class T>
   constexpr auto refl = make_reflection_info<T>{};
   
   template <class T, size_t I>
   using refl_t = make_reflection_info<T>::template type<I>;
   
   template <auto Opts, class T>
   struct object_info
   {
      using V = std::decay_t<T>;
      static constexpr auto info = make_reflection_info<T>();
      static constexpr auto N = info.N;
      
      // Allows us to remove a branch if the first item will always be written
      static constexpr bool first_will_be_written = [] {
         if constexpr (N > 0) {
            using V = std::remove_cvref_t<refl_t<T, 0>>;

            if constexpr (detail::null_t<V> && Opts.skip_null_members) {
               return false;
            }

            if constexpr (is_includer<V> || std::same_as<V, hidden> || std::same_as<V, skip>) {
               return false;
            }
            else {
               return true;
            }
         }
         else {
            return false;
         }
      }();

      static constexpr bool maybe_skipped = [] {
         if constexpr (N > 0) {
            bool found_maybe_skipped{};
            for_each_short_circuit<N>([&](auto I) {
               using V = std::remove_cvref_t<refl_t<T, I>>;

               if constexpr (Opts.skip_null_members && detail::null_t<V>) {
                  found_maybe_skipped = true;
                  return true; // early exit
               }

               if constexpr (is_includer<V> || std::same_as<V, hidden> || std::same_as<V, skip>) {
                  found_maybe_skipped = true;
                  return true; // early exit
               }
               return false; // continue
            });
            return found_maybe_skipped;
         }
         else {
            return false;
         }
      }();
   };
}

