// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

namespace glz::detail
{
   // We create const and not-const versions for when our reflected struct is const or non-const qualified
   template <class Tuple>
   struct tuple_ptr;

   template <class... Ts>
   struct tuple_ptr<tuplet::tuple<Ts...>>
   {
      using type = tuplet::tuple<std::add_pointer_t<std::remove_reference_t<Ts>>...>;
   };

   template <class Tuple>
   struct tuple_ptr_const;

   template <class... Ts>
   struct tuple_ptr_const<tuplet::tuple<Ts...>>
   {
      using type = tuplet::tuple<std::add_pointer_t<std::add_const_t<std::remove_reference_t<Ts>>>...>;
   };

   // This is needed to hack a fix for MSVC evaluating wrong `if constexpr` branches
   template <class T>
      requires(!reflectable<T>)
   constexpr auto make_tuple_from_struct() noexcept
   {
      return glz::tuplet::tuple{};
   }

   // This needs to produce const qualified pointers so that we can write out const structs
   template <reflectable T>
   constexpr auto make_tuple_from_struct() noexcept
   {
      using V = decay_keep_volatile_t<decltype(to_tuple(std::declval<T>()))>;
      return typename tuple_ptr<V>::type{};
   }

   template <reflectable T>
   constexpr auto make_const_tuple_from_struct() noexcept
   {
      using V = decay_keep_volatile_t<decltype(to_tuple(std::declval<T>()))>;
      return typename tuple_ptr_const<V>::type{};
   }

   template <reflectable T, class TuplePtrs>
      requires(!std::is_const_v<TuplePtrs>)
   constexpr void populate_tuple_ptr(T&& value, TuplePtrs& tuple_of_ptrs) noexcept
   {
      // we have to populate the pointers in the reflection tuple from the structured binding
      auto t = to_tuple(std::forward<T>(value));
      [&]<size_t... I>(std::index_sequence<I...>) {
         ((get<I>(tuple_of_ptrs) = &get<I>(t)), ...);
      }(std::make_index_sequence<count_members<T>>{});
   }
}

namespace glz
{
   // Get indices of elements satisfying a predicate
   template <class Tuple, template <class> class Predicate>
   consteval auto filter_indices()
   {
      constexpr auto N = tuple_size_v<Tuple>;
      if constexpr (N == 0) {
         return std::array<size_t, 0>{};
      }
      else {
         return []<size_t... Is>(std::index_sequence<Is...>) constexpr {
            constexpr bool matches[] = {Predicate<glz::tuple_element_t<Is, Tuple>>::value...};
            constexpr size_t count = (matches[Is] + ...);

            std::array<size_t, count> indices{};
            size_t index = 0;
            ((void)((matches[Is] ? (indices[index++] = Is, true) : false)), ...);
            return indices;
         }(std::make_index_sequence<N>{});
      }
   }

   template <class T>
   concept is_object_key_type = std::convertible_to<T, std::string_view>;

   template <class T>
   using object_key_type = std::bool_constant<is_object_key_type<T>>;

   template <class T>
   using not_object_key_type = std::bool_constant<not is_object_key_type<T>>;

   namespace detail
   {
      // The purpose of this is to allocate a new string_view to only the portion of memory
      // that we are concerned with. This lets the compiler reduce the binary on
      // reflected names;
      template <size_t I, class V>
      struct get_name_alloc
      {
         static constexpr auto alias = get_name<get<I>(meta_v<V>)>();
         static constexpr auto value = join_v<alias>;
      };
   }

   template <class T, size_t I>
   consteval sv get_key_element()
   {
      using V = std::decay_t<T>;
      if constexpr (I == 0) {
         if constexpr (is_object_key_type<decltype(get<0>(meta_v<V>))>) {
            return get<0>(meta_v<V>);
         }
         else {
            return detail::get_name_alloc<0, V>::value;
            // return get_name<get<0>(meta_v<V>)>();
         }
      }
      else if constexpr (is_object_key_type<decltype(get<I - 1>(meta_v<V>))>) {
         return get<I - 1>(meta_v<V>);
      }
      else {
         return detail::get_name_alloc<I, V>::value;
         // return get_name<get<I>(meta_v<V>)>();
      }
   };

   template <class T>
   struct refl_info;

   // MSVC requires this template specialization for when the tuple size if zero,
   // otherwise MSVC tries to instantiate calls of get<0> in invalid branches
   template <class T>
      requires((detail::glaze_object_t<T> || detail::glaze_flags_t<T> || detail::glaze_enum_t<T>) &&
               (tuple_size_v<meta_t<T>> == 0))
   struct refl_info<T>
   {
      static constexpr auto N = 0;
      static constexpr auto values = tuplet::tuple{};
      static constexpr std::array<sv, 0> keys{};

      template <size_t I>
      using type = std::nullptr_t;
   };

   template <class T>
      requires(detail::glaze_object_t<T> || detail::glaze_flags_t<T> || detail::glaze_enum_t<T>)
   struct refl_info<T>
   {
      using V = std::remove_cvref_t<T>;
      static constexpr auto value_indices = filter_indices<meta_t<V>, not_object_key_type>();

      static constexpr auto values = [] {
         return [&]<size_t... I>(std::index_sequence<I...>) { //
            return tuplet::tuple{get<value_indices[I]>(meta_v<T>)...}; //
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
      using elem = decltype(get<I>(values));

      template <size_t I>
      using type = detail::member_t<V, decltype(get<I>(values))>;
   };

   template <class T>
      requires(detail::glaze_array_t<T>)
   struct refl_info<T>
   {
      using V = std::remove_cvref_t<T>;

      static constexpr auto values = meta_v<V>;

      static constexpr auto N = tuple_size_v<decltype(values)>;

      template <size_t I>
      using elem = decltype(get<I>(values));

      template <size_t I>
      using type = detail::member_t<V, decltype(get<I>(values))>;
   };

   template <class T>
      requires detail::reflectable<T>
   struct refl_info<T>
   {
      using V = std::remove_cvref_t<T>;
      using tuple = decay_keep_volatile_t<decltype(detail::to_tuple(std::declval<T>()))>;

      // static constexpr auto values = typename detail::tuple_ptr<tuple>::type{};

      static constexpr auto keys = member_names<V>;
      static constexpr auto N = keys.size();

      template <size_t I>
      using elem = decltype(get<I>(std::declval<tuple>()));

      template <size_t I>
      using type = detail::member_t<V, decltype(get<I>(std::declval<tuple>()))>;
   };

   template <class T>
      requires detail::readable_map_t<T>
   struct refl_info<T>
   {
      static constexpr auto N = 0;
   };

   template <class T>
   constexpr auto refl = refl_info<T>{};

   template <class T, size_t I>
   using elem_t = refl_info<T>::template elem<I>;

   template <class T, size_t I>
   using refl_t = refl_info<T>::template type<I>;

   // MSVC requires this specialization, otherwise it will try to instatiate dead `if constexpr` branches for N == 0
   template <opts Opts, class T>
   struct object_info;

   template <opts Opts, class T>
      requires(refl<T>.N == 0)
   struct object_info<Opts, T>
   {
      static constexpr bool first_will_be_written = false;
      static constexpr bool maybe_skipped = false;
   };

   template <opts Opts, class T>
      requires(refl<T>.N > 0)
   struct object_info<Opts, T>
   {
      static constexpr auto N = refl<T>.N;

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
         if constexpr (N > 0 && Opts.skip_null_members) {
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

namespace glz::detail
{
   template <size_t I, class T>
   constexpr auto key_name_v = [] {
      if constexpr (reflectable<T>) {
         return get<I>(member_names<T>);
      }
      else {
         return refl<T>.keys[I];
      }
   }();

   template <class T, auto Opts>
   constexpr auto required_fields()
   {
      constexpr auto N = refl<T>.N;

      bit_array<N> fields{};
      if constexpr (Opts.error_on_missing_keys) {
         for_each<N>([&](auto I) constexpr {
            fields[I] = !bool(Opts.skip_null_members) || !null_t<std::decay_t<refl_t<T, I>>>;
         });
      }
      return fields;
   }
}

namespace glz::detail
{
   // from
   // https://stackoverflow.com/questions/55941964/how-to-filter-duplicate-types-from-tuple-c
   template <class T, class... Ts>
   struct unique
   {
      using type = T;
   };

   template <template <class...> class T, class... Ts, class U, class... Us>
   struct unique<T<Ts...>, U, Us...>
      : std::conditional_t<(std::is_same_v<U, Ts> || ...), unique<T<Ts...>, Us...>, unique<T<Ts..., U>, Us...>>
   {};

   template <class... Ts>
   struct unique_variant : unique<std::variant<>, Ts...>
   {};

   template <class T>
   struct tuple_ptr_variant;

   template <class... Ts>
   struct tuple_ptr_variant<glz::tuplet::tuple<Ts...>> : unique<std::variant<>, std::add_pointer_t<Ts>...>
   {};

   template <class... Ts>
   struct tuple_ptr_variant<std::tuple<Ts...>> : unique<std::variant<>, std::add_pointer_t<Ts>...>
   {};

   template <class... Ts>
   struct tuple_ptr_variant<std::pair<Ts...>> : unique<std::variant<>, std::add_pointer_t<Ts>...>
   {};

   template <class T, class = std::make_index_sequence<refl<T>.N>>
   struct member_tuple_type;

   template <class T, size_t... I>
   struct member_tuple_type<T, std::index_sequence<I...>>
   {
      using type = std::conditional_t<sizeof...(I) == 0, tuplet::tuple<>,
                                      tuplet::tuple<std::remove_cvref_t<member_t<T, refl_t<T, I>>>...>>;
   };

   template <class T>
   using member_tuple_t = typename member_tuple_type<T>::type;

   template <class T, class = std::make_index_sequence<refl<T>.N>>
   struct value_variant;

   template <class T, size_t... I>
   struct value_variant<T, std::index_sequence<I...>>
   {
      using type = typename unique_variant<std::remove_cvref_t<elem_t<T, I>>...>::type;
   };

   template <class T>
   using value_variant_t = typename value_variant<T>::type;

   template <class T>
   inline constexpr auto make_array()
   {
      return []<size_t... I>(std::index_sequence<I...>) {
         using value_t = value_variant_t<T>;
         return std::array<value_t, refl<T>.N>{get<I>(refl<T>.values)...};
      }(std::make_index_sequence<refl<T>.N>{});
   }

   template <class Tuple, std::size_t... Is>
   inline constexpr auto tuple_runtime_getter(std::index_sequence<Is...>)
   {
      using value_t = typename tuple_ptr_variant<Tuple>::type;
      using tuple_ref = std::add_lvalue_reference_t<Tuple>;
      using getter_t = value_t (*)(tuple_ref);
      return std::array<getter_t, glz::tuple_size_v<Tuple>>{+[](tuple_ref t) -> value_t {
         if constexpr (is_std_tuple<Tuple>) {
            return &std::get<Is>(t);
         }
         else {
            return &glz::get<Is>(t);
         }
      }...};
   }

   template <class Tuple>
   inline auto get_runtime(Tuple&& t, const size_t index)
   {
      using T = std::decay_t<Tuple>;
      static constexpr auto indices = std::make_index_sequence<glz::tuple_size_v<T>>{};
      static constexpr auto runtime_getter = tuple_runtime_getter<T>(indices);
      return runtime_getter[index](t);
   }
}

namespace glz::detail
{
   template <class T, size_t I>
   constexpr auto key_value = pair<sv, value_variant_t<T>>{refl<T>.keys[I], get<I>(refl<T>.values)};

   template <class T, size_t I>
   constexpr sv key_v = refl<T>.keys[I];

   template <class T, bool use_hash_comparison, size_t... I>
   constexpr auto make_map_impl(std::index_sequence<I...>)
   {
      using value_t = value_variant_t<T>;
      constexpr auto n = refl<T>.N;

      if constexpr (n == 0) {
         return nullptr; // Hack to fix MSVC
         // static_assert(false_v<T>, "Empty object map is illogical. Handle empty upstream.");
      }
      else if constexpr (n == 1) {
         return micro_map1<value_t, key_v<T, I>...>{key_value<T, I>...};
      }
      else if constexpr (n == 2) {
         return micro_map2<value_t, key_v<T, I>...>{key_value<T, I>...};
      }
      else if constexpr (n < 64) // don't even attempt a first character hash if we have too many keys
      {
         constexpr auto& keys = refl<T>.keys;
         constexpr auto front_desc = single_char_hash<n>(keys);

         if constexpr (front_desc.valid) {
            return make_single_char_map<value_t, front_desc>({key_value<T, I>...});
         }
         else {
            constexpr single_char_hash_opts rear_hash{.is_front_hash = false};
            constexpr auto back_desc = single_char_hash<n, rear_hash>(keys);

            if constexpr (back_desc.valid) {
               return make_single_char_map<value_t, back_desc>({key_value<T, I>...});
            }
            else {
               constexpr single_char_hash_opts sum_hash{.is_front_hash = true, .is_sum_hash = true};
               constexpr auto sum_desc = single_char_hash<n, sum_hash>(keys);

               if constexpr (sum_desc.valid) {
                  return make_single_char_map<value_t, sum_desc>({key_value<T, I>...});
               }
               else {
                  if constexpr (n <= naive_map_max_size) {
                     constexpr auto naive_desc = naive_map_hash<use_hash_comparison, n>(keys);
                     return glz::detail::make_naive_map<value_t, naive_desc>(std::array{key_value<T, I>...});
                  }
                  else {
                     return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(
                        std::array{key_value<T, I>...});
                  }
               }
            }
         }
      }
      else {
         return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(std::array{key_value<T, I>...});
      }
   }

   template <class T, bool use_hash_comparison = false>
      requires(!reflectable<T>)
   constexpr auto make_map()
   {
      constexpr auto indices = std::make_index_sequence<refl<T>.N>{};
      return make_map_impl<decay_keep_volatile_t<T>, use_hash_comparison>(indices);
   }

   template <class T>
   constexpr auto make_key_int_map()
   {
      constexpr auto N = refl<T>.N;
      return [&]<size_t... I>(std::index_sequence<I...>) {
         return normal_map<sv, size_t, refl<T>.N>(pair<sv, size_t>{refl<T>.keys[I], I}...);
      }(std::make_index_sequence<N>{});
   }
}

namespace glz
{
   template <auto Enum>
      requires(std::is_enum_v<decltype(Enum)>)
   constexpr sv enum_name_v = []() -> std::string_view {
      using T = std::decay_t<decltype(Enum)>;

      if constexpr (detail::glaze_t<T>) {
         using U = std::underlying_type_t<T>;
         return refl<T>.keys[static_cast<U>(Enum)];
      }
      else {
         static_assert(false_v<decltype(Enum)>, "Enum requires glaze metadata for name");
      }
   }();
}

namespace glz::detail
{
   template <class T>
   constexpr auto make_enum_to_string_map()
   {
      constexpr auto N = refl<T>.N;
      return [&]<size_t... I>(std::index_sequence<I...>) {
         using key_t = std::underlying_type_t<T>;
         return normal_map<key_t, sv, N>(std::array<pair<key_t, sv>, N>{
            pair<key_t, sv>{static_cast<key_t>(get<I>(refl<T>.values)), refl<T>.keys[I]}...});
      }(std::make_index_sequence<N>{});
   }

   // TODO: This faster approach can be used if the enum has an integer type base and sequential numbering
   template <class T>
   constexpr auto make_enum_to_string_array() noexcept
   {
      return []<size_t... I>(std::index_sequence<I...>) {
         return std::array<sv, sizeof...(I)>{refl<T>.keys[I]...};
      }(std::make_index_sequence<refl<T>.N>{});
   }

   template <class T>
   constexpr auto make_string_to_enum_map() noexcept
   {
      constexpr auto N = refl<T>.N;
      return [&]<size_t... I>(std::index_sequence<I...>) {
         return normal_map<sv, T, N>(
            std::array<pair<sv, T>, N>{pair<sv, T>{refl<T>.keys[I], T(get<I>(refl<T>.values))}...});
      }(std::make_index_sequence<N>{});
   }

   // get a std::string_view from an enum value
   template <class T>
      requires(detail::glaze_t<T> && std::is_enum_v<std::decay_t<T>>)
   constexpr auto get_enum_name(T&& enum_value)
   {
      using V = std::decay_t<T>;
      using U = std::underlying_type_t<V>;
      constexpr auto arr = glz::detail::make_enum_to_string_array<V>();
      return arr[static_cast<U>(enum_value)];
   }

   template <detail::glaze_flags_t T>
   consteval auto byte_length() noexcept
   {
      constexpr auto N = refl<T>.N;

      if constexpr (N % 8 == 0) {
         return N / 8;
      }
      else {
         return (N / 8) + 1;
      }
   }
}

#include <initializer_list>

#include "glaze/core/common.hpp"
#include "glaze/reflection/get_name.hpp"
#include "glaze/reflection/to_tuple.hpp"

namespace glz
{
   // Use a dummy struct for make_reflectable so that we don't conflict with any user defined constructors
   struct dummy final
   {};

   // If you want to make an empty struct or a struct with constructors visible in reflected structs,
   // add the folllwing constructor to your type:
   // my_struct(glz::make_reflectable) {}
   using make_reflectable = std::initializer_list<dummy>;

   namespace detail
   {
      template <class Tuple>
      using reflection_value_tuple_variant_t = typename tuple_ptr_variant<Tuple>::type;

      template <class T, size_t I>
      struct named_member
      {
         static constexpr sv value = get<I>(member_names<T>);
      };

      template <class T, bool use_hash_comparison, size_t... I>
      constexpr auto make_reflection_map_impl(std::index_sequence<I...>)
      {
         using V = decltype(to_tuple(std::declval<T>()));
         constexpr auto n = glz::tuple_size_v<V>;
         constexpr auto members = member_names<T>;
         static_assert(members.size() == n);

         using value_t = reflection_value_tuple_variant_t<V>;

         if constexpr (n == 0) {
            return nullptr; // Hack to fix MSVC
            // static_assert(false_v<T>, "Empty object map is illogical. Handle empty upstream.");
         }
         else if constexpr (n == 1) {
            return micro_map1<value_t, named_member<T, I>::value...>{
               pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n == 2) {
            return micro_map2<value_t, named_member<T, I>::value...>{
               pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n < 64) // don't even attempt a first character hash if we have too many keys
         {
            constexpr auto& keys = member_names<T>;
            constexpr auto front_desc = single_char_hash<n>(keys);

            if constexpr (front_desc.valid) {
               return make_single_char_map<value_t, front_desc>(
                  {{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
            }
            else {
               constexpr single_char_hash_opts rear_hash{.is_front_hash = false};
               constexpr auto back_desc = single_char_hash<n, rear_hash>(keys);

               if constexpr (back_desc.valid) {
                  return make_single_char_map<value_t, back_desc>(
                     {{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
               }
               else {
                  constexpr single_char_hash_opts sum_hash{.is_front_hash = true, .is_sum_hash = true};
                  constexpr auto sum_desc = single_char_hash<n, sum_hash>(keys);

                  if constexpr (sum_desc.valid) {
                     return make_single_char_map<value_t, sum_desc>(
                        {{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
                  }
                  else {
                     if constexpr (n <= naive_map_max_size) {
                        constexpr auto naive_desc = naive_map_hash<use_hash_comparison, n>(keys);
                        return glz::detail::make_naive_map<value_t, naive_desc>(std::array{
                           pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
                     }
                     else {
                        return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(std::array{
                           pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
                     }
                  }
               }
            }
         }
         else {
            return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>(
               std::array{pair<sv, value_t>{get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
         }
      }

      template <reflectable T, bool use_hash_comparison = false>
      constexpr auto make_map()
         requires(!glaze_t<T> && !array_t<T> && std::is_aggregate_v<std::remove_cvref_t<T>>)
      {
         constexpr auto indices = std::make_index_sequence<count_members<T>>{};
         return make_reflection_map_impl<decay_keep_volatile_t<T>, use_hash_comparison>(indices);
      }

      template <reflectable T>
      GLZ_ALWAYS_INLINE constexpr void populate_map(T&& value, auto& cmap) noexcept
      {
         // we have to populate the pointers in the reflection map from the structured binding
         auto t = to_tuple(std::forward<T>(value));
         [&]<size_t... I>(std::index_sequence<I...>) {
            ((get<std::add_pointer_t<decay_keep_volatile_t<decltype(get<I>(t))>>>(get<I>(cmap.items).second) =
                 &get<I>(t)),
             ...);
         }(std::make_index_sequence<count_members<T>>{});
      }
   }
}

namespace glz::detail
{
   // TODO: This is returning the total keys and not the max keys for a particular variant object
   template <class T, size_t N>
   constexpr size_t get_max_keys = [] {
      size_t res{};
      for_each<N>([&](auto I) {
         using V = std::decay_t<std::variant_alternative_t<I, T>>;
         if constexpr (glaze_object_t<V> || reflectable<V>) {
            res += refl<V>.N;
         }
      });
      return res;
   }();

   template <class T>
   constexpr auto get_combined_keys_from_variant()
   {
      constexpr auto N = std::variant_size_v<T>;

      std::array<sv, get_max_keys<T, N>> keys{};
      // This intermediate pointer is necessary for GCC 13 (otherwise segfaults with reflection logic)
      auto* data_ptr = &keys;
      size_t index = 0;
      for_each<N>([&](auto I) {
         using V = std::decay_t<std::variant_alternative_t<I, T>>;
         if constexpr (glaze_object_t<V> || reflectable<V>) {
            for_each<refl<V>.N>([&](auto J) { (*data_ptr)[index++] = refl<V>.keys[J]; });
         }
      });

      std::sort(keys.begin(), keys.end());
      const auto end = std::unique(keys.begin(), keys.end());
      const auto size = std::distance(keys.begin(), end);

      return pair{keys, size};
   }

   template <class T, size_t... I>
   consteval auto make_variant_deduction_base_map(std::index_sequence<I...>, auto&& keys)
   {
      using V = bit_array<std::variant_size_v<T>>;
      return normal_map<sv, V, sizeof...(I)>(
         std::array<pair<sv, V>, sizeof...(I)>{pair<sv, V>{sv(std::get<I>(keys)), V{}}...});
   }

   template <class T>
   constexpr auto make_variant_deduction_map()
   {
      constexpr auto key_size_pair = get_combined_keys_from_variant<T>();

      auto deduction_map =
         make_variant_deduction_base_map<T>(std::make_index_sequence<key_size_pair.second>{}, key_size_pair.first);

      constexpr auto N = std::variant_size_v<T>;
      for_each<N>([&](auto I) {
         using V = decay_keep_volatile_t<std::variant_alternative_t<I, T>>;
         if constexpr (glaze_object_t<V> || reflectable<V>) {
            for_each<refl<V>.N>([&](auto J) { deduction_map.find(refl<V>.keys[J])->second[I] = true; });
         }
      });

      return deduction_map;
   }
}

namespace glz::detail
{
   // Array of random 64bit primes to avoid needing a generator
   // If you have a 10% chance of success, 128 tries has 1 : 719'380 odds of failure
   // formula: 1 / (1 - .10)^128
   // If you have a 15% chance of success, 128 tries has 1 : 1'082'374'392 odds of failure
   constexpr std::array<uint64_t, 128> primes_64 = {
      12835920395396008793ull, 15149911783463666029ull, 15211026597907833541ull, 14523965596842631817ull,
      16449355892475772073ull, 15002762636229733759ull, 12275448295353509891ull, 16826285440568349437ull,
      17433093378066653197ull, 10902769355249605843ull,
      // 10
      13434269760430048511ull, 11322871945166463571ull, 9764742595129026499ull, 13799666429485716229ull,
      14861204462552525359ull, 17599486090324515493ull, 10266842847898195667ull, 13468209895759219897ull,
      16289274021814922521ull, 17204791465022878523ull,
      // 20
      17650915497556268801ull, 9455725851336774341ull, 9961868820920778071ull, 18289017266131008167ull,
      16309921878298474091ull, 11652007405601517343ull, 17496906368504743207ull, 13339901080756288547ull,
      10018112158103183191ull, 14981853847663275059ull,
      // 30
      15024425770511821387ull, 10063189458099824779ull, 11839419318547724641ull, 12830508825440804579ull,
      18433073685722024387ull, 17299753196133391897ull, 15396274865347173053ull, 13749499465490568733ull,
      13860497513065316459ull, 11692484392508072123ull,
      // 40
      9438685344574842347ull, 14400973074801304571ull, 15991854874494707081ull, 12114831452910579989ull,
      13998562683662696077ull, 10851982863396796529ull, 10344539733475859441ull, 10045431121852560691ull,
      12186301656248163491ull, 10339340957932801343ull,
      // 50
      12602808298244447957ull, 9729322814141112953ull, 16362682788060189359ull, 14286005063617200559ull,
      10711312188467965759ull, 17189878401094383577ull, 14020292519311722517ull, 14345076507873302501ull,
      12604083410008146689ull, 13199781912416808047ull,
      // 60
      12315285542260457987ull, 9594690105092455043ull, 18319948908022195243ull, 16595976138570481823ull,
      12195274638706298323ull, 17646127793987855047ull, 16841966643138849923ull, 13507873747212661603ull,
      13643501673321416333ull, 12363547163076811279ull,
      // 70
      14884690672546139069ull, 16390599211964954609ull, 13389084014248465909ull, 9833412254409620477ull,
      13398707790907619797ull, 18368581595698861159ull, 10384836406878147059ull, 9453231669972318281ull,
      16669513057697818169ull, 14697852657232879237ull,
      // 80
      15363091209268144241ull, 15275124567630446861ull, 17481357028816037011ull, 12534821792295694231ull,
      11054303650520058359ull, 12403322865831490261ull, 16889743306920098983ull, 16047403249754946029ull,
      16775982981238529423ull, 12421181800577384167ull,
      // 90
      9280311361047996317ull, 12160582891762080001ull, 17367939315478490407ull, 9267482598791369377ull,
      17806662591996060709ull, 11533875385674872861ull, 16816712667689633689ull, 16223588510861656927ull,
      13482535659370429831ull, 11801382448764686461ull,
      // 100
      18177646705946605939ull, 12608957628213527767ull, 17302819561726521227ull, 10609776890453861677ull,
      16615779974080376533ull, 14779379909309032351ull, 14673741062489338759ull, 11634351264868983527ull,
      13874971253878591537ull, 17332275173558136899ull,
      // 110
      11257300840627829851ull, 13341443249676872783ull, 11607230252125083613ull, 12141161410409106457ull,
      13282325402819801929ull, 17990774082148480601ull, 15496227723025164119ull, 15956314649183876369ull,
      9834926670622290883ull, 16435035491439552943ull,
      // 120
      14806063675962222449ull, 18113370406266068597ull, 9968958022058734229ull, 14864844583184822773ull,
      11170341707334307859ull, 10502344452987841991ull, 16247553219265957739ull, 14933089789266129581ull};

   constexpr uint64_t bitmix(uint64_t h, const uint64_t seed) noexcept
   {
      h *= seed;
      return h ^ std::rotr(h, 49);
   };

   template <size_t N>
   using bucket_value_t = std::conditional_t < N<256, uint8_t, uint16_t>;

   // The larger the underlying bucket the more we avoid collisions with invalid keys.
   // This improves performance of rejecting invalid keys because we don't have to do
   // string comparisons in these cases.
   // However, there are obvious memory costs with increasing the bucket size.

   enum struct hash_type {
      invalid,
      unique_index,
      front_16,
   };

   consteval size_t bucket_size(hash_type type, size_t N)
   {
      using enum hash_type;
      switch (type) {
      case invalid: {
         return 0;
      }
      case unique_index: {
         return 256;
      }
      case front_16: {
         return (N == 1) ? 1 : std::bit_ceil(N * N) / 2;
      }
      default: {
         return 0;
      }
      }
   }

   struct keys_info_t
   {
      size_t N{};
      hash_type type{};
      size_t min_length = (std::numeric_limits<size_t>::max)();
      size_t max_length{};
      // uint8_t min_diff = (std::numeric_limits<uint8_t>::max)();
      uint64_t seed{};
      size_t unique_index = (std::numeric_limits<size_t>::max)();
      bool sized_hash = false;
   };

   // For hash algorithm a value of the seed indicates an invalid hash

   // A value of N in the bucket indicates an invalid hash
   template <class T, size_t Slots>
   struct hash_info_t
   {
      hash_type type{};

      static constexpr auto N = refl<T>.N;
      using V = bucket_value_t<N>;
      static constexpr auto invalid = static_cast<V>(N);

      std::array<V, Slots> table{}; // hashes to switch-case indices
      size_t max_length{};
      uint64_t seed{};
      size_t unique_index = (std::numeric_limits<size_t>::max)();
      bool sized_hash = false;
   };

   constexpr std::optional<size_t> find_unique_index(const auto& strings)
   {
      namespace ranges = std::ranges;

      const auto N = strings.size();

      if (N == 0) {
         return {};
      }

      size_t min_length = (std::numeric_limits<size_t>::max)();
      for (auto& s : strings) {
         const auto n = s.size();
         if (n < min_length) {
            min_length = n;
         }
      }

      if (min_length == 0) {
         return {};
      }

      std::vector<std::vector<uint8_t>> cols(min_length);

      for (size_t i = 0; i < N; ++i) {
         const auto& s = strings[i];
         // for each character in the string
         for (size_t c = 0; c < min_length; ++c) {
            cols[c].emplace_back(s[c]);
         }
      }

      // sort colums so that we can determine
      // if the column is unique
      size_t best_index{};
      size_t best_count{};
      for (size_t i = 0; i < min_length; ++i) {
         auto& col = cols[i];
         ranges::sort(col);
         if (auto it = ranges::adjacent_find(col); it == col.end()) {
            // no duplicates found
            best_count = col.size();
            best_index = i;
            break;
         }
      }

      if (best_count == 0) {
         return {};
      }

      return best_index;
   }

   constexpr std::optional<size_t> find_unique_sized_index(const auto& strings)
   {
      namespace ranges = std::ranges;

      const auto N = strings.size();

      if (N == 0) {
         return {};
      }

      size_t min_length = (std::numeric_limits<size_t>::max)();
      for (auto& s : strings) {
         if (s.contains('"')) {
            return {}; // Sized hashing requires looking for terminating quote
         }

         const auto n = s.size();
         if (n < min_length) {
            min_length = n;
         }
      }

      if (min_length == 0) {
         return {};
      }

      std::vector<std::vector<uint16_t>> cols(min_length);

      for (size_t i = 0; i < N; ++i) {
         const auto& s = strings[i];
         // for each character in the string
         for (size_t c = 0; c < min_length; ++c) {
            const auto k = uint16_t(s[c]) | (uint16_t(s.size()) << 8);
            cols[c].emplace_back(k);
         }
      }

      // sort colums so that we can determine
      // if the column is unique
      size_t best_index{};
      size_t best_count{};
      for (size_t i = 0; i < min_length; ++i) {
         auto& col = cols[i];
         ranges::sort(col);
         if (auto it = ranges::adjacent_find(col); it == col.end()) {
            // no duplicates found
            best_count = col.size();
            best_index = i;
            break;
         }
      }

      if (best_count == 0) {
         return {};
      }

      return best_index;
   }

   // TODO: Add N == 1 optimization
   template <size_t N>
   constexpr auto make_keys_info(const std::array<sv, N>& keys)
   {
      namespace ranges = std::ranges;

      keys_info_t info{N};

      if (N == 0) {
         return info;
      }

      for (size_t i = 0; i < N; ++i) {
         const auto n = keys[i].size();
         if (n < info.min_length) {
            info.min_length = n;
         }
         if (n > info.max_length) {
            info.max_length = n;
         }
      }

      using enum hash_type;

      if (const auto uindex = find_unique_index(keys)) {
         info.type = unique_index;
         info.unique_index = uindex.value();
         return info;
      }

      auto& seed = info.seed;
      constexpr uint64_t invalid_seed = 0;

      if (const auto uindex = find_unique_sized_index(keys)) {
         info.type = unique_index;
         info.unique_index = uindex.value();
         info.sized_hash = true;

         auto sized_unique_hash = [&] {
            std::array<size_t, N> bucket_index{};
            constexpr auto bsize = bucket_size(unique_index, N);

            for (size_t i = 0; i < primes_64.size(); ++i) {
               seed = primes_64[i];
               size_t index = 0;
               for (const auto& key : keys) {
                  const auto hash = bitmix(uint16_t(key[info.unique_index]) | (uint16_t(key.size()) << 8), seed);
                  if (hash == seed) {
                     break;
                  }
                  const auto bucket = hash % bsize;
                  if (contains(std::span{bucket_index.data(), index}, bucket)) {
                     break;
                  }
                  bucket_index[index] = bucket;
                  ++index;
               }

               if (index == N) {
                  // make sure the seed does not collide with any hashes
                  const auto bucket = seed % bsize;
                  if (not contains(std::span{bucket_index.data(), N}, bucket)) {
                     return; // found working seed
                  }
               }
            }

            seed = invalid_seed;
         };

         sized_unique_hash();
         if (seed != invalid_seed) {
            return info;
         }
      }

      if (info.min_length > 1 && N <= 32) {
         // check for uniqueness
         std::array<uint16_t, N> k;
         for (size_t i = 0; i < N; ++i) {
            k[i] = uint16_t(keys[i][0]) | (uint16_t(keys[i][1]) << 8);
         }

         ranges::sort(k);

         bool valid = true;
         for (size_t i = 0; i < N - 1; ++i) {
            const auto diff = k[i + 1] - k[i];
            if (diff == 0) {
               valid = false;
               break;
            }
         }

         if (valid) {
            auto front_16_hash = [&] {
               std::array<size_t, N> bucket_index{};
               constexpr auto bsize = bucket_size(front_16, N);

               for (size_t i = 0; i < primes_64.size(); ++i) {
                  seed = primes_64[i];
                  size_t index = 0;
                  for (const auto& key : keys) {
                     const auto hash = bitmix(uint16_t(key[0]) | (uint16_t(key[1]) << 8), seed);
                     if (hash == seed) {
                        break;
                     }
                     const auto bucket = hash % bsize;
                     if (contains(std::span{bucket_index.data(), index}, bucket)) {
                        break;
                     }
                     bucket_index[index] = bucket;
                     ++index;
                  }

                  if (index == N) {
                     // make sure the seed does not collide with any hashes
                     const auto bucket = seed % bsize;
                     if (not contains(std::span{bucket_index.data(), N}, bucket)) {
                        return; // found working seed
                     }
                  }
               }

               seed = invalid_seed;
            };

            front_16_hash();
            if (seed != invalid_seed) {
               info.type = front_16;
               return info;
            }
         }
      }

      return info;
   }

   template <class T>
   constexpr auto keys_info = make_keys_info(refl<T>.keys);

   template <class T>
   constexpr auto hash_info = [] {
      if constexpr ((glaze_object_t<T> || reflectable<T>)&&(refl<T>.N > 0)) {
         constexpr auto& k_info = keys_info<T>;
         constexpr auto type = k_info.type;
         constexpr auto N = refl<T>.N;
         constexpr auto& keys = refl<T>.keys;

         using enum hash_type;
         if constexpr (type == unique_index && N < 256) {
            hash_info_t<T, bucket_size(unique_index, N)> info{.type = unique_index, .seed = k_info.seed};
            info.max_length = k_info.max_length;
            info.table.fill(N);
            info.unique_index = k_info.unique_index;

            if constexpr (k_info.sized_hash) {
               info.sized_hash = true;
               constexpr auto bsize = bucket_size(unique_index, N);
               for (uint8_t i = 0; i < N; ++i) {
                  const auto x = uint16_t(keys[i][k_info.unique_index]) | (uint16_t(keys[i].size()) << 8);
                  const auto h = bitmix(x, info.seed) % bsize;
                  info.table[h] = i;
               }
            }
            else {
               for (uint8_t i = 0; i < N; ++i) {
                  const auto h = uint8_t(keys[i][k_info.unique_index]);
                  info.table[h] = i;
               }
            }
            return info;
         }
         else if constexpr (type == front_16) {
            constexpr auto bsize = bucket_size(front_16, N);
            hash_info_t<T, bsize> info{.type = front_16, .seed = k_info.seed};
            info.max_length = k_info.max_length;
            info.table.fill(N);

            for (uint8_t i = 0; i < N; ++i) {
               const auto h = bitmix(uint16_t(keys[i][0]) | (uint16_t(keys[i][1]) << 8), info.seed) % bsize;
               info.table[h] = i;
            }

            return info;
         }
         else {
            // invalid
            return hash_info_t<T, 0>{};
         }
      }
      else {
         return hash_info_t<T, 0>{};
      }
   }();
}
