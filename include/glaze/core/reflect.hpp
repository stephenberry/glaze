// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/beve/header.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/primes_64.hpp"

#ifdef _MSC_VER
// Turn off MSVC warning for unreferenced formal parameter, which is referenced in a constexpr branch
#pragma warning(push)
#pragma warning(disable : 4100 4189)
#endif

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
   struct reflect;

   // MSVC requires this template specialization for when the tuple size if zero,
   // otherwise MSVC tries to instantiate calls of get<0> in invalid branches
   template <class T>
      requires((detail::glaze_object_t<T> || detail::glaze_flags_t<T> || detail::glaze_enum_t<T>) &&
               (tuple_size_v<meta_t<T>> == 0))
   struct reflect<T>
   {
      static constexpr auto size = 0;
      static constexpr auto values = tuplet::tuple{};
      static constexpr std::array<sv, 0> keys{};

      template <size_t I>
      using type = std::nullptr_t;
   };

   template <class T>
      requires(!detail::meta_keys<T> &&
               (detail::glaze_object_t<T> || detail::glaze_flags_t<T> || detail::glaze_enum_t<T>) &&
               (tuple_size_v<meta_t<T>> != 0))
   struct reflect<T>
   {
      using V = std::remove_cvref_t<T>;
      static constexpr auto value_indices = filter_indices<meta_t<V>, not_object_key_type>();

      static constexpr auto values = [] {
         return [&]<size_t... I>(std::index_sequence<I...>) { //
            return tuplet::tuple{get<value_indices[I]>(meta_v<T>)...}; //
         }(std::make_index_sequence<value_indices.size()>{}); //
      }();

      static constexpr auto size = tuple_size_v<decltype(values)>;

      static constexpr auto keys = [] {
         std::array<sv, size> res{};
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

   namespace detail
   {
      template <class T, size_t N>
      inline constexpr auto c_style_to_sv(const std::array<T, N>& arr)
      {
         std::array<sv, N> ret{};
         for (size_t i = 0; i < N; ++i) {
            ret[i] = arr[i];
         }
         return ret;
      }
   }

   template <class T>
      requires(detail::meta_keys<T> && detail::glaze_t<T>)
   struct reflect<T>
   {
      using V = std::remove_cvref_t<T>;
      static constexpr auto size = tuple_size_v<meta_keys_t<T>>;

      static constexpr auto values = meta_wrapper_v<T>;

      static constexpr auto keys = detail::c_style_to_sv(meta_keys_v<T>);

      template <size_t I>
      using elem = decltype(get<I>(values));

      template <size_t I>
      using type = detail::member_t<V, decltype(get<I>(values))>;
   };

   template <class T>
      requires(detail::is_memory_object<T>)
   struct reflect<T> : reflect<memory_type<T>>
   {};

   template <class T>
      requires(detail::glaze_array_t<T>)
   struct reflect<T>
   {
      using V = std::remove_cvref_t<T>;

      static constexpr auto values = meta_v<V>;

      static constexpr auto size = tuple_size_v<decltype(values)>;

      template <size_t I>
      using elem = decltype(get<I>(values));

      template <size_t I>
      using type = detail::member_t<V, decltype(get<I>(values))>;
   };

   template <class T>
      requires detail::reflectable<T>
   struct reflect<T>
   {
      using V = std::remove_cvref_t<T>;
      using tuple = decay_keep_volatile_t<decltype(detail::to_tuple(std::declval<T>()))>;

      // static constexpr auto values = typename detail::tuple_ptr<tuple>::type{};

      static constexpr auto keys = member_names<V>;
      static constexpr auto size = keys.size();

      template <size_t I>
      using elem = decltype(get<I>(std::declval<tuple>()));

      template <size_t I>
      using type = detail::member_t<V, decltype(get<I>(std::declval<tuple>()))>;
   };

   template <class T>
      requires detail::readable_map_t<T>
   struct reflect<T>
   {
      static constexpr auto size = 0;
   };

   template <class T, size_t I>
   using elem_t = reflect<T>::template elem<I>;

   template <class T, size_t I>
   using refl_t = reflect<T>::template type<I>;

   // MSVC requires this specialization, otherwise it will try to instatiate dead `if constexpr` branches for N == 0
   template <opts Opts, class T>
   struct object_info;

   template <opts Opts, class T>
      requires(reflect<T>::size == 0)
   struct object_info<Opts, T>
   {
      static constexpr bool first_will_be_written = false;
      static constexpr bool maybe_skipped = false;
   };

   template <opts Opts, class T>
      requires(reflect<T>::size > 0)
   struct object_info<Opts, T>
   {
      static constexpr auto N = reflect<T>::size;

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
         return reflect<T>::keys[I];
      }
   }();

   template <class T, auto Opts>
   constexpr auto required_fields()
   {
      constexpr auto N = reflect<T>::size;

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

   template <class T, class = std::make_index_sequence<reflect<T>::size>>
   struct member_tuple_type;

   template <class T, size_t... I>
   struct member_tuple_type<T, std::index_sequence<I...>>
   {
      using type = std::conditional_t<sizeof...(I) == 0, tuplet::tuple<>,
                                      tuplet::tuple<std::remove_cvref_t<member_t<T, refl_t<T, I>>>...>>;
   };

   template <class T>
   using member_tuple_t = typename member_tuple_type<T>::type;

   template <class T, class = std::make_index_sequence<reflect<T>::size>>
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
         return std::array<value_t, reflect<T>::size>{get<I>(reflect<T>::values)...};
      }(std::make_index_sequence<reflect<T>::size>{});
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
   constexpr auto key_value = pair<sv, value_variant_t<T>>{reflect<T>::keys[I], get<I>(reflect<T>::values)};

   template <class T, size_t I>
   constexpr sv key_v = reflect<T>::keys[I];

   template <class T, bool use_hash_comparison, size_t... I>
   constexpr auto make_map_impl(std::index_sequence<I...>)
   {
      using value_t = value_variant_t<T>;
      constexpr auto n = reflect<T>::size;

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
         constexpr auto& keys = reflect<T>::keys;
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
      constexpr auto indices = std::make_index_sequence<reflect<T>::size>{};
      return make_map_impl<decay_keep_volatile_t<T>, use_hash_comparison>(indices);
   }

   template <class T>
   constexpr auto make_key_int_map()
   {
      constexpr auto N = reflect<T>::size;
      return [&]<size_t... I>(std::index_sequence<I...>) {
         return normal_map<sv, size_t, reflect<T>::size>(pair<sv, size_t>{reflect<T>::keys[I], I}...);
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
         return reflect<T>::keys[static_cast<U>(Enum)];
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
      constexpr auto N = reflect<T>::size;
      return [&]<size_t... I>(std::index_sequence<I...>) {
         using key_t = std::underlying_type_t<T>;
         return normal_map<key_t, sv, N>(std::array<pair<key_t, sv>, N>{
            pair<key_t, sv>{static_cast<key_t>(glz::get<I>(reflect<T>::values)), reflect<T>::keys[I]}...});
      }(std::make_index_sequence<N>{});
   }

   // TODO: This faster approach can be used if the enum has an integer type base and sequential numbering
   template <class T>
   constexpr auto make_enum_to_string_array() noexcept
   {
      return []<size_t... I>(std::index_sequence<I...>) {
         return std::array<sv, sizeof...(I)>{reflect<T>::keys[I]...};
      }(std::make_index_sequence<reflect<T>::size>{});
   }

   template <class T>
   constexpr auto make_string_to_enum_map() noexcept
   {
      constexpr auto N = reflect<T>::size;
      return [&]<size_t... I>(std::index_sequence<I...>) {
         return normal_map<sv, T, N>(
            std::array<pair<sv, T>, N>{pair<sv, T>{reflect<T>::keys[I], T(glz::get<I>(reflect<T>::values))}...});
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
      constexpr auto N = reflect<T>::size;

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
         static constexpr sv value = glz::get<I>(member_names<T>);
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
               pair<sv, value_t>{glz::get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n == 2) {
            return micro_map2<value_t, named_member<T, I>::value...>{
               pair<sv, value_t>{glz::get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...};
         }
         else if constexpr (n < 64) // don't even attempt a first character hash if we have too many keys
         {
            constexpr auto& keys = member_names<T>;
            constexpr auto front_desc = single_char_hash<n>(keys);

            if constexpr (front_desc.valid) {
               return make_single_char_map<value_t, front_desc>(
                  {{glz::get<I>(members), std::add_pointer_t<glz::tuple_element_t<I, V>>{}}...});
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
            res += reflect<V>::size;
         }
         else if constexpr (is_memory_object<V>) {
            res += reflect<memory_type<V>>::size;
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
         if constexpr (glaze_object_t<V> || reflectable<V> || is_memory_object<V>) {
            using X = std::conditional_t<is_memory_object<V>, memory_type<V>, V>;
            for (size_t i = 0; i < reflect<X>::size; ++i) {
               (*data_ptr)[index++] = reflect<X>::keys[i];
            }
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
         if constexpr (glaze_object_t<V> || reflectable<V> || is_memory_object<V>) {
            using X = std::conditional_t<is_memory_object<V>, memory_type<V>, V>;
            for_each<reflect<X>::size>([&](auto J) { deduction_map.find(reflect<X>::keys[J])->second[I] = true; });
         }
      });

      return deduction_map;
   }
}

namespace glz::detail
{
   GLZ_ALWAYS_INLINE constexpr uint64_t bitmix(uint64_t h, const uint64_t seed) noexcept
   {
      h *= seed;
      return h ^ std::rotr(h, 49);
   };

   // Use when hashing large chunks of characters that are likely very similar
   GLZ_ALWAYS_INLINE constexpr uint64_t rich_bitmix(uint64_t h, const uint64_t seed) noexcept
   {
      h ^= h >> 23;
      h *= 0x2127599bf4325c37ULL;
      h ^= seed;
      h *= 0x880355f21e6d1965ULL;
      h ^= h >> 47;
      return h;
   }

   template <size_t N>
   using bucket_value_t = std::conditional_t < N<256, uint8_t, uint16_t>;

   // The larger the underlying bucket the more we avoid collisions with invalid keys.
   // This improves performance of rejecting invalid keys because we don't have to do
   // string comparisons in these cases.
   // However, there are obvious memory costs with increasing the bucket size.

   enum struct hash_type { //
      invalid, // hashing failed
      unique_index, // A unique character index is used
      front_hash, // Hash on the front bytes of the key
      single_element, // Map is a single element
      mod4, // c % 4
      xor_mod4, // (c ^ c0) % 4
      minus_mod4, // (c - c0) % 4
      three_element_unique_index,
      unique_per_length, // Hash on a unique character index and the length of the key
      full_flat // Full key hash with a single table
   };

   // For N == 3 and N == 4 it is cheap to check mod4, xor_mod4, and minus_mod4 hashes.
   // Consecuative values like "x", "y", "z" for keys work with minus_mod4

   struct unique_per_length_t
   {
      bool valid{};
      std::array<uint8_t, 256> unique_index{};
   };

   inline constexpr unique_per_length_t unique_per_length_info(const auto& input_strings)
   {
      // TODO: MSVC fixed the related compiler bug, but GitHub Actions has not caught up yet
#if !defined(_MSC_VER)
      const auto N = input_strings.size();
      if (N == 0) {
         return {};
      }

      std::vector<std::string_view> strings{};
      for (size_t i = 0; i < N; ++i) {
         strings.emplace_back(input_strings[i]);
      }

      std::ranges::sort(strings, [](const auto& a, const auto& b) { return a.size() < b.size(); });

      if (strings.front().empty() || strings.back().size() >= 255) {
         return {};
      }

      unique_per_length_t info{.valid = true};
      info.unique_index.fill(255);

      // Process each unique length
      for (size_t len = strings.front().size(); len <= strings.back().size(); ++len) {
         auto range_begin = std::lower_bound(strings.begin(), strings.end(), len,
                                             [](const auto& s, size_t l) { return s.length() < l; });

         auto range_end =
            std::upper_bound(range_begin, strings.end(), len, [](size_t l, const auto& s) { return l < s.length(); });

         auto range = std::ranges::subrange(range_begin, range_end);

         if (range.begin() == range.end()) continue;

         // Find the first unique character for this length
         bool found = false;
         for (size_t pos = 0; pos < len; ++pos) {
            std::array<int, 256> char_count = {};

            // Count occurrences of each character at this position
            for (auto it = range.begin(); it != range.end(); ++it) {
               ++char_count[uint8_t((*it)[pos])];
            }

            bool collision = false;
            for (const auto count : char_count) {
               if (count > 1) {
                  collision = true;
                  break;
               }
            }
            if (not collision) {
               info.unique_index[len] = uint8_t(pos);
               found = true;
               break;
            }
         }
         if (not found) {
            info.valid = false;
            return info;
         }
      }

      return info;
#else
      return {};
#endif
   }

   template <class T>
   inline constexpr auto per_length_info = unique_per_length_info(reflect<T>::keys);

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
      case front_hash: {
         return (N == 1) ? 1 : std::bit_ceil(N * N) / 2;
      }
      case single_element: {
         return 0;
      }
      case mod4: {
         return 0;
      }
      case xor_mod4: {
         return 0;
      }
      case minus_mod4: {
         return 0;
      }
      case three_element_unique_index: {
         return 0;
      }
      case unique_per_length: {
         return (N == 1) ? 1 : std::bit_ceil(N * N) / 2;
      }
      case full_flat: {
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
      size_t front_hash_bytes{};
   };

   // For hash algorithm a value of the seed indicates an invalid hash

   // A value of N in the bucket indicates an invalid hash
   template <class T, size_t Slots>
   struct hash_info_t
   {
      hash_type type{};

      static constexpr auto N = reflect<T>::size;
      using V = bucket_value_t<N>;
      static constexpr auto invalid = static_cast<V>(N);

      std::array<V, Slots> table{}; // hashes to switch-case indices
      size_t min_length = (std::numeric_limits<size_t>::max)();
      size_t max_length{};
      uint64_t seed{};
      size_t unique_index = (std::numeric_limits<size_t>::max)();
      bool sized_hash = false;
      size_t front_hash_bytes{};
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

      for (const auto& s : strings) {
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

   // For full hashing we perform a rich_bitmix on the tail end
   // This is because most cases that require full hashes are because
   // the tail end is the only unique part

   // Do not call this at runtime, it is assumes the key lies within min_length and max_length
   inline constexpr uint64_t full_hash_impl(const sv key, const uint64_t seed, const auto min_length,
                                            const auto max_length) noexcept
   {
      if (max_length < 8) {
         return bitmix(to_uint64_n_below_8(key.data(), key.size()), seed);
      }
      else if (min_length > 7) {
         const auto n = key.size();
         uint64_t h = seed;
         const auto* data = key.data();
         const auto* end7 = data + n - 7;
         for (auto d0 = data; d0 < end7; d0 += 8) {
            h = bitmix(to_uint64(d0), h);
         }
         // Handle potential tail. We know we have at least 8
         return rich_bitmix(to_uint64(data + n - 8), h);
      }
      else {
         const auto n = key.size();
         const auto* data = key.data();

         if (n < 8) {
            return bitmix(to_uint64_n_below_8(data, n), seed);
         }

         uint64_t h = seed;
         const auto* end7 = data + n - 7;
         for (auto d0 = data; d0 < end7; d0 += 8) {
            h = bitmix(to_uint64(d0), h);
         }
         // Handle potential tail. We know we have at least 8
         return rich_bitmix(to_uint64(data + n - 8), h);
      }
   }

   // runtime full hash algorithm
   template <uint64_t min_length, uint64_t max_length, uint64_t seed>
   inline constexpr uint64_t full_hash(const auto* it, const size_t n) noexcept
   {
      if constexpr (max_length < 8) {
         if (n > 7) {
            return seed;
         }
         return bitmix(to_uint64_n_below_8(it, n), seed);
      }
      else if constexpr (min_length > 7) {
         if (n < 8) {
            return seed;
         }
         uint64_t h = seed;
         const auto* end7 = it + n - 7;
         for (auto d0 = it; d0 < end7; d0 += 8) {
            h = bitmix(to_uint64(d0), h);
         }
         // Handle potential tail. We know we have at least 8
         return rich_bitmix(to_uint64(it + n - 8), h);
      }
      else {
         if (n < 8) {
            return bitmix(to_uint64_n_below_8(it, n), seed);
         }

         uint64_t h = seed;
         const auto* end7 = it + n - 7;
         for (auto d0 = it; d0 < end7; d0 += 8) {
            h = bitmix(to_uint64(d0), h);
         }
         // Handle potential tail. We know we have at least 8
         return rich_bitmix(to_uint64(it + n - 8), h);
      }
   }

   template <std::integral ChunkType, size_t N>
   constexpr bool front_bytes_hash_info(const std::array<sv, N>& keys, keys_info_t& info) noexcept
   {
      if (info.min_length < sizeof(ChunkType)) {
         return false;
      }

      // check for uniqueness
      std::array<ChunkType, N> k;
      for (size_t i = 0; i < N; ++i) {
         if constexpr (std::same_as<ChunkType, uint16_t>) {
            k[i] = uint16_t(keys[i][0]) | (uint16_t(keys[i][1]) << 8);
         }
         else if constexpr (std::same_as<ChunkType, uint32_t>) {
            k[i] = uint32_t(keys[i][0]) //
                   | (uint32_t(keys[i][1]) << 8) //
                   | (uint32_t(keys[i][2]) << 16) //
                   | (uint32_t(keys[i][3]) << 24);
         }
         else if constexpr (std::same_as<ChunkType, uint64_t>) {
            k[i] = uint64_t(keys[i][0]) //
                   | (uint64_t(keys[i][1]) << 8) //
                   | (uint64_t(keys[i][2]) << 16) //
                   | (uint64_t(keys[i][3]) << 24) //
                   | (uint64_t(keys[i][4]) << 32) //
                   | (uint64_t(keys[i][5]) << 40) //
                   | (uint64_t(keys[i][6]) << 48) //
                   | (uint64_t(keys[i][7]) << 56);
         }
         else {
            static_assert(false_v<ChunkType>);
         }
      }

      std::ranges::sort(k);

      for (size_t i = 0; i < N - 1; ++i) {
         const auto diff = k[i + 1] - k[i];
         if (diff == 0) {
            return false;
         }
      }

      using enum hash_type;
      constexpr uint64_t invalid_seed = 0;
      auto& seed = info.seed;
      auto hash_alg = [&] {
         std::array<size_t, N> bucket_index{};
         constexpr auto bsize = bucket_size(front_hash, N);

         for (size_t i = 0; i < primes_64.size(); ++i) {
            seed = primes_64[i];
            size_t index = 0;
            for (const auto& key : keys) {
               const auto hash = [&]() -> size_t {
                  if constexpr (std::same_as<ChunkType, uint16_t>) {
                     return bitmix(uint16_t(key[0]) | (uint16_t(key[1]) << 8), seed);
                  }
                  else if constexpr (std::same_as<ChunkType, uint32_t>) {
                     return bitmix(uint32_t(key[0]) //
                                      | (uint32_t(key[1]) << 8) //
                                      | (uint32_t(key[2]) << 16) //
                                      | (uint32_t(key[3]) << 24),
                                   seed);
                  }
                  else if constexpr (std::same_as<ChunkType, uint64_t>) {
                     return rich_bitmix(uint64_t(key[0]) //
                                           | (uint64_t(key[1]) << 8) //
                                           | (uint64_t(key[2]) << 16) //
                                           | (uint64_t(key[3]) << 24) //
                                           | (uint64_t(key[4]) << 32) //
                                           | (uint64_t(key[5]) << 40) //
                                           | (uint64_t(key[6]) << 48) //
                                           | (uint64_t(key[7]) << 56),
                                        seed);
                  }
                  else {
                     static_assert(false_v<ChunkType>);
                  }
               }();
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

      hash_alg();
      if (seed != invalid_seed) {
         info.type = front_hash;
         info.front_hash_bytes = sizeof(ChunkType);
         return true;
      }

      return false;
   }

   // The sequence of hashing algorithms written here determines the selection preference
   template <size_t N>
   constexpr auto make_keys_info(const std::array<sv, N>& keys)
   {
      namespace ranges = std::ranges;

      keys_info_t info{N};

      if constexpr (N == 0) {
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

      if constexpr (N == 1) {
         info.type = single_element;
         return info;
      }

      // N == 2 is optimized within other hashing methods

      if constexpr (N == 3 || N == 4) {
         if (info.min_length > 0) {
            bool valid = true;
            for (size_t i = 0; i < N; ++i) {
               if (keys[i][0] % 4 != uint8_t(i)) {
                  valid = false;
               }
            }
            if (valid) {
               info.type = mod4;
               return info;
            }

            const auto c0 = keys[0][0];

            valid = true;
            for (size_t i = 0; i < N; ++i) {
               if ((keys[i][0] ^ c0) % 4 != uint8_t(i)) {
                  valid = false;
               }
            }
            if (valid) {
               info.type = xor_mod4;
               return info;
            }

            valid = true;
            for (size_t i = 0; i < N; ++i) {
               if ((keys[i][0] - c0) % 4 != uint8_t(i)) {
                  valid = false;
               }
            }
            if (valid) {
               info.type = minus_mod4;
               return info;
            }
         }
      }

      auto& seed = info.seed;
      constexpr uint64_t invalid_seed = 0;

      if (const auto uindex = find_unique_index(keys)) {
         info.unique_index = uindex.value();

         if constexpr (N == 3) {
            // An xor of the first unique character with itself will result in 0 (our desired index)
            // We use a hash algorithm that will produce zero if zero is given, so we can avoid a branch
            // We need a seed produces hashes of [1, 2] for the 2nd and 3rd keys

            const auto u = info.unique_index;
            const auto first = uint8_t(keys[0][u]);
            const auto mix1 = uint8_t(keys[1][u]) ^ first;
            const auto mix2 = uint8_t(keys[2][u]) ^ first;

            for (size_t i = 0; i < primes_64.size(); ++i) {
               seed = primes_64[i];
               uint8_t h1 = (mix1 * seed) % 4;
               uint8_t h2 = (mix2 * seed) % 4;

               if (h1 == 1 && h2 == 2) {
                  info.type = three_element_unique_index;
                  return info;
               }
            }
            // Otherwise we failed to find a seed and we'll use a normal unique_index hash
         }

         info.type = unique_index;
         return info;
      }

      if (front_bytes_hash_info<uint16_t>(keys, info)) {
         return info;
      }
      else if (front_bytes_hash_info<uint32_t>(keys, info)) {
         return info;
      }
      else if (front_bytes_hash_info<uint64_t>(keys, info)) {
         return info;
      }

      if (const auto uindex = find_unique_sized_index(keys)) {
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
            info.type = unique_index;
            return info;
         }
      }

      // TODO: MSVC fixed the related compiler bug, but GitHub Actions has not caught up yet
#if !defined(_MSC_VER)
      // TODO: Use meta-programming to cache this value
      const auto per_length_data = unique_per_length_info(keys);
      if (per_length_data.valid) {
         auto sized_unique_hash = [&] {
            std::array<size_t, N> bucket_index{};
            constexpr auto bsize = bucket_size(unique_per_length, N);

            for (size_t i = 0; i < primes_64.size(); ++i) {
               seed = primes_64[i];
               size_t index = 0;
               for (const auto& key : keys) {
                  const auto n = uint8_t(key.size());
                  const auto hash = bitmix(uint16_t(key[per_length_data.unique_index[n]]) | (uint16_t(n) << 8), seed);
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
            info.type = unique_per_length;
            return info;
         }
      }
#endif

      // full_flat
      {
         auto full_flat_hash = [&] {
            std::array<size_t, N> bucket_index{};
            constexpr auto bsize = bucket_size(full_flat, N);

            for (size_t i = 0; i < primes_64.size(); ++i) {
               seed = primes_64[i];
               size_t index = 0;
               for (const auto& key : keys) {
                  const auto hash = full_hash_impl(key, seed, info.min_length, info.max_length);
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

         full_flat_hash();
         if (seed != invalid_seed) {
            info.type = full_flat;
            return info;
         }
      }

      return info;
   }

   template <class T>
   constexpr auto keys_info = make_keys_info(reflect<T>::keys);

   template <class T>
   constexpr auto hash_info = [] {
      if constexpr ((glaze_object_t<T> || reflectable<T> ||
                     ((std::is_enum_v<std::remove_cvref_t<T>> && meta_keys<T>) || glaze_enum_t<T>)) &&
                    (reflect<T>::size > 0)) {
         constexpr auto& k_info = keys_info<T>;
         constexpr auto& type = k_info.type;
         constexpr auto& N = reflect<T>::size;
         constexpr auto& keys = reflect<T>::keys;

         using enum hash_type;
         if constexpr (type == single_element) {
            hash_info_t<T, bucket_size(single_element, N)> info{.type = single_element};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            return info;
         }
         else if constexpr (type == mod4) {
            hash_info_t<T, bucket_size(mod4, N)> info{.type = mod4};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            return info;
         }
         else if constexpr (type == xor_mod4) {
            hash_info_t<T, bucket_size(xor_mod4, N)> info{.type = xor_mod4};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            return info;
         }
         else if constexpr (type == minus_mod4) {
            hash_info_t<T, bucket_size(minus_mod4, N)> info{.type = minus_mod4};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            return info;
         }
         else if constexpr (type == three_element_unique_index) {
            hash_info_t<T, bucket_size(three_element_unique_index, N)> info{.type = three_element_unique_index};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            info.seed = k_info.seed;
            info.unique_index = k_info.unique_index;
            return info;
         }
         else if constexpr (type == front_hash) {
            constexpr auto bsize = bucket_size(front_hash, N);
            hash_info_t<T, bsize> info{.type = front_hash, .seed = k_info.seed};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            info.front_hash_bytes = k_info.front_hash_bytes;
            info.table.fill(uint8_t(N));

            for (uint8_t i = 0; i < N; ++i) {
               auto& key = keys[i];
               const auto h = [&]() -> size_t {
                  if (info.front_hash_bytes == sizeof(uint16_t)) {
                     return bitmix(uint16_t(key[0]) | (uint16_t(key[1]) << 8), info.seed) % bsize;
                  }
                  else if (info.front_hash_bytes == sizeof(uint32_t)) {
                     return bitmix(uint32_t(key[0]) //
                                      | (uint32_t(key[1]) << 8) //
                                      | (uint32_t(key[2]) << 16) //
                                      | (uint32_t(key[3]) << 24),
                                   info.seed) %
                            bsize;
                  }
                  else if (info.front_hash_bytes == sizeof(uint64_t)) {
                     return rich_bitmix(uint64_t(key[0]) //
                                           | (uint64_t(key[1]) << 8) //
                                           | (uint64_t(key[2]) << 16) //
                                           | (uint64_t(key[3]) << 24) //
                                           | (uint64_t(key[4]) << 32) //
                                           | (uint64_t(key[5]) << 40) //
                                           | (uint64_t(key[6]) << 48) //
                                           | (uint64_t(key[7]) << 56),
                                        info.seed) %
                            bsize;
                  }
                  else {
                     return 0; // MSVC has a compiler bug that prevents us from returning N, but this is unreachable
                  }
               }();
               info.table[h] = i;
            }

            return info;
         }
         else if constexpr (type == unique_index && N < 256) {
            hash_info_t<T, bucket_size(unique_index, N)> info{.type = unique_index, .seed = k_info.seed};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            info.table.fill(bucket_value_t<N>(N));
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
         // TODO: MSVC fixed the related compiler bug, but GitHub Actions has not caught up yet
#if !defined(_MSC_VER)
         else if constexpr (type == unique_per_length) {
            hash_info_t<T, bucket_size(unique_per_length, N)> info{.type = unique_per_length, .seed = k_info.seed};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            info.table.fill(uint8_t(N));
            info.sized_hash = true;
            constexpr auto bsize = bucket_size(unique_per_length, N);
            constexpr auto& data = per_length_info<T>;
            for (uint8_t i = 0; i < N; ++i) {
               const auto n = keys[i].size();
               const auto x = uint16_t(keys[i][data.unique_index[n]]) | (uint16_t(n) << 8);
               const auto h = bitmix(x, info.seed) % bsize;
               info.table[h] = i;
            }
            return info;
         }
#endif
         else if constexpr (type == full_flat) {
            hash_info_t<T, bucket_size(full_flat, N)> info{.type = full_flat, .seed = k_info.seed};
            info.min_length = k_info.min_length;
            info.max_length = k_info.max_length;
            info.table.fill(uint8_t(N));
            constexpr auto bsize = bucket_size(full_flat, N);
            for (uint8_t i = 0; i < N; ++i) {
               const auto h = full_hash_impl(keys[i], info.seed, info.min_length, info.max_length) % bsize;
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

   template <size_t min_length>
   GLZ_ALWAYS_INLINE constexpr const void* quote_memchr(auto&& it, auto&& end) noexcept
   {
      if (std::is_constant_evaluated()) {
         const auto count = size_t(end - it);
         for (std::size_t i = 0; i < count; ++i) {
            if (it[i] == '"') {
               return it + i;
            }
         }
         return nullptr;
      }
      else {
         if constexpr (min_length >= 4) {
            // Skipping makes the bifurcation worth it
            const auto* start = it + min_length;
            if (start >= end) [[unlikely]] {
               return nullptr;
            }
            else [[likely]] {
               return std::memchr(start, '"', size_t(end - start));
            }
         }
         else {
            return std::memchr(it, '"', size_t(end - it));
         }
      }
   }

   template <uint32_t Format, class T, auto HashInfo, hash_type Type>
   struct decode_hash;

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::single_element>
   {
      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& /*it*/, auto&& /*end*/) noexcept { return 0; }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::mod4>
   {
      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& /*end*/) noexcept { return uint8_t(*it) % 4; }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::xor_mod4>
   {
      static constexpr auto first_key_char = reflect<T>::keys[0][0];

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& /*end*/) noexcept
      {
         return (uint8_t(*it) ^ first_key_char) % 4;
      }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::minus_mod4>
   {
      static constexpr auto first_key_char = reflect<T>::keys[0][0];

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& /*end*/) noexcept
      {
         return (uint8_t(*it) - first_key_char) % 4;
      }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::unique_index>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::unique_index, N);
      static constexpr auto uindex = HashInfo.unique_index;

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end) noexcept
      {
         if constexpr (HashInfo.sized_hash) {
            const auto* c = quote_memchr<HashInfo.min_length>(it, end);
            if (c) [[likely]] {
               const auto n = size_t(static_cast<std::decay_t<decltype(it)>>(c) - it);
               if (n == 0 || n > HashInfo.max_length) {
                  return N; // error
               }

               const auto h = bitmix(uint16_t(it[HashInfo.unique_index]) | (uint16_t(n) << 8), HashInfo.seed);
               return HashInfo.table[h % bsize];
            }
            else [[unlikely]] {
               return N;
            }
         }
         else {
            if constexpr (N == 2) {
               if constexpr (uindex > 0) {
                  if ((it + uindex) >= end) [[unlikely]] {
                     return N; // error
                  }
               }
               // Avoids using a hash table
               if (std::is_constant_evaluated()) {
                  constexpr auto first_key_char = reflect<T>::keys[0][uindex];
                  return size_t(bool(it[uindex] ^ first_key_char));
               }
               else {
                  static constexpr auto first_key_char = reflect<T>::keys[0][uindex];
                  return size_t(bool(it[uindex] ^ first_key_char));
               }
            }
            else {
               if constexpr (uindex > 0) {
                  if ((it + uindex) >= end) [[unlikely]] {
                     return N; // error
                  }
               }
               return HashInfo.table[uint8_t(it[uindex])];
            }
         }
      }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::three_element_unique_index>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto uindex = HashInfo.unique_index;

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end) noexcept
      {
         if constexpr (uindex > 0) {
            if ((it + uindex) >= end) [[unlikely]] {
               return N; // error
            }
         }
         // Avoids using a hash table
         if (std::is_constant_evaluated()) {
            constexpr auto first_key_char = reflect<T>::keys[0][uindex];
            return (uint8_t(it[uindex] ^ first_key_char) * HashInfo.seed) % 4;
         }
         else {
            static constexpr auto first_key_char = reflect<T>::keys[0][uindex];
            return (uint8_t(it[uindex] ^ first_key_char) * HashInfo.seed) % 4;
         }
      }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::front_hash>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::front_hash, N);

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end) noexcept
      {
         if constexpr (HashInfo.front_hash_bytes == 2) {
            if ((it + 2) >= end) [[unlikely]] {
               return N; // error
            }
            uint16_t h;
            if (std::is_constant_evaluated()) {
               h = 0;
               for (size_t i = 0; i < 2; ++i) {
                  h |= static_cast<uint16_t>(it[i]) << (8 * i);
               }
            }
            else {
               std::memcpy(&h, it, 2);
            }
            return HashInfo.table[bitmix(h, HashInfo.seed) % bsize];
         }
         else if constexpr (HashInfo.front_hash_bytes == 4) {
            if ((it + 4) >= end) [[unlikely]] {
               return N;
            }
            uint32_t h;
            if (std::is_constant_evaluated()) {
               h = 0;
               for (size_t i = 0; i < 4; ++i) {
                  h |= static_cast<uint32_t>(it[i]) << (8 * i);
               }
            }
            else {
               std::memcpy(&h, it, 4);
            }
            return HashInfo.table[bitmix(h, HashInfo.seed) % bsize];
         }
         else if constexpr (HashInfo.front_hash_bytes == 8) {
            if ((it + 8) >= end) [[unlikely]] {
               return N;
            }
            uint64_t h;
            if (std::is_constant_evaluated()) {
               h = 0;
               for (size_t i = 0; i < 8; ++i) {
                  h |= static_cast<uint64_t>(it[i]) << (8 * i);
               }
            }
            else {
               std::memcpy(&h, it, 8);
            }
            return HashInfo.table[rich_bitmix(h, HashInfo.seed) % bsize];
         }
         else {
            static_assert(false_v<T>, "invalid hash algorithm");
         }
      }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::unique_per_length>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::unique_per_length, N);

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end) noexcept
      {
         const auto* c = quote_memchr<HashInfo.min_length>(it, end);
         if (c) [[likely]] {
            const auto n = uint8_t(static_cast<std::decay_t<decltype(it)>>(c) - it);
            const auto pos = per_length_info<T>.unique_index[n];
            if ((it + pos) >= end) [[unlikely]] {
               return N; // error
            }
            const auto h = bitmix(uint16_t(it[pos]) | (uint16_t(n) << 8), HashInfo.seed);
            return HashInfo.table[h % bsize];
         }
         else [[unlikely]] {
            return N;
         }
      }
   };

   template <class T, auto HashInfo>
   struct decode_hash<JSON, T, HashInfo, hash_type::full_flat>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::full_flat, N);
      static constexpr auto min_length = HashInfo.min_length;
      static constexpr auto max_length = HashInfo.max_length;
      static constexpr auto length_range = max_length - min_length;

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end) noexcept
      {
         // For JSON we require at a minimum ":1} characters after a key (1 being a single char number)
         // This means that we can require all these characters to exist for SWAR parsing

         if constexpr (length_range == 0) {
            if ((it + min_length) >= end) [[unlikely]] {
               return N;
            }
            const auto h = full_hash<HashInfo.min_length, HashInfo.max_length, HashInfo.seed>(it, min_length);
            return HashInfo.table[h % bsize];
         }
         else {
            if constexpr (length_range == 1) {
               auto quote = it + min_length;
               if ((quote + 1) >= end) [[unlikely]] {
                  return N;
               }

               const auto n = min_length + uint8_t(*quote != '"');
               const auto h = full_hash<HashInfo.min_length, HashInfo.max_length, HashInfo.seed>(it, n);
               return HashInfo.table[h % bsize];
            }
            else {
               const auto* c = quote_memchr<HashInfo.min_length>(it, end);
               if (c) [[likely]] {
                  const auto n = uint8_t(static_cast<std::decay_t<decltype(it)>>(c) - it);
                  const auto h = full_hash<HashInfo.min_length, HashInfo.max_length, HashInfo.seed>(it, n);
                  return HashInfo.table[h % bsize];
               }
               else [[unlikely]] {
                  return N;
               }
            }
         }
      }
   };

   template <uint32_t Format, class T, auto HashInfo, hash_type Type>
   struct decode_hash_with_size;

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::single_element>
   {
      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&&, auto&&, const size_t) noexcept { return 0; }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::mod4>
   {
      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&&, const size_t) noexcept
      {
         return uint8_t(*it) % 4;
      }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::xor_mod4>
   {
      static constexpr auto first_key_char = reflect<T>::keys[0][0];

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&&, const size_t) noexcept
      {
         return (uint8_t(*it) ^ first_key_char) % 4;
      }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::minus_mod4>
   {
      static constexpr auto first_key_char = reflect<T>::keys[0][0];

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&&, const size_t) noexcept
      {
         return (uint8_t(*it) - first_key_char) % 4;
      }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::unique_index>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::unique_index, N);
      static constexpr auto uindex = HashInfo.unique_index;

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end, const size_t n) noexcept
      {
         if constexpr (HashInfo.sized_hash) {
            if (n == 0 || n > HashInfo.max_length) {
               return N; // error
            }

            const auto h = bitmix(uint16_t(it[HashInfo.unique_index]) | (uint16_t(n) << 8), HashInfo.seed);
            return HashInfo.table[h % bsize];
         }
         else {
            if constexpr (N == 2) {
               if ((it + uindex) >= end) [[unlikely]] {
                  return N; // error
               }
               // Avoids using a hash table
               if (std::is_constant_evaluated()) {
                  constexpr auto first_key_char = reflect<T>::keys[0][uindex];
                  return size_t(bool(it[uindex] ^ first_key_char));
               }
               else {
                  static constexpr auto first_key_char = reflect<T>::keys[0][uindex];
                  return size_t(bool(it[uindex] ^ first_key_char));
               }
            }
            else {
               if ((it + uindex) >= end) [[unlikely]] {
                  return N; // error
               }
               return HashInfo.table[uint8_t(it[uindex])];
            }
         }
      }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::three_element_unique_index>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto uindex = HashInfo.unique_index;

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end, const size_t) noexcept
      {
         if constexpr (uindex > 0) {
            if ((it + uindex) >= end) [[unlikely]] {
               return N; // error
            }
         }
         // Avoids using a hash table
         if (std::is_constant_evaluated()) {
            constexpr auto first_key_char = reflect<T>::keys[0][uindex];
            return (uint8_t(it[uindex] ^ first_key_char) * HashInfo.seed) % 4;
         }
         else {
            static constexpr auto first_key_char = reflect<T>::keys[0][uindex];
            return (uint8_t(it[uindex] ^ first_key_char) * HashInfo.seed) % 4;
         }
      }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::front_hash>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::front_hash, N);

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&&, const size_t) noexcept
      {
         if constexpr (HashInfo.front_hash_bytes == 2) {
            uint16_t h;
            if (std::is_constant_evaluated()) {
               h = 0;
               for (size_t i = 0; i < 2; ++i) {
                  h |= static_cast<uint16_t>(it[i]) << (8 * i);
               }
            }
            else {
               std::memcpy(&h, it, 2);
            }
            return HashInfo.table[bitmix(h, HashInfo.seed) % bsize];
         }
         else if constexpr (HashInfo.front_hash_bytes == 4) {
            uint32_t h;
            if (std::is_constant_evaluated()) {
               h = 0;
               for (size_t i = 0; i < 4; ++i) {
                  h |= static_cast<uint32_t>(it[i]) << (8 * i);
               }
            }
            else {
               std::memcpy(&h, it, 4);
            }
            return HashInfo.table[bitmix(h, HashInfo.seed) % bsize];
         }
         else if constexpr (HashInfo.front_hash_bytes == 8) {
            uint64_t h;
            if (std::is_constant_evaluated()) {
               h = 0;
               for (size_t i = 0; i < 8; ++i) {
                  h |= static_cast<uint64_t>(it[i]) << (8 * i);
               }
            }
            else {
               std::memcpy(&h, it, 8);
            }
            return HashInfo.table[rich_bitmix(h, HashInfo.seed) % bsize];
         }
         else {
            static_assert(false_v<T>, "invalid hash algorithm");
         }
      }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::unique_per_length>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::unique_per_length, N);

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&& end, const size_t n) noexcept
      {
         const auto pos = per_length_info<T>.unique_index[uint8_t(n)];
         if ((it + pos) >= end) [[unlikely]] {
            return N; // error
         }
         const auto h = bitmix(uint16_t(it[pos]) | (uint16_t(n) << 8), HashInfo.seed);
         return HashInfo.table[h % bsize];
      }
   };

   template <uint32_t Format, class T, auto HashInfo>
   struct decode_hash_with_size<Format, T, HashInfo, hash_type::full_flat>
   {
      static constexpr auto N = reflect<T>::size;
      static constexpr auto bsize = bucket_size(hash_type::full_flat, N);

      GLZ_ALWAYS_INLINE static constexpr size_t op(auto&& it, auto&&, const size_t n) noexcept
      {
         const auto h = full_hash<HashInfo.min_length, HashInfo.max_length, HashInfo.seed>(it, n);
         return HashInfo.table[h % bsize];
      }
   };
}

namespace glz
{
   [[nodiscard]] inline std::string format_error(const error_ctx& pe, const auto& buffer)
   {
      const auto error_type_str = meta<error_code>::keys[uint32_t(pe.ec)];

      const auto info = detail::get_source_info(buffer, pe.location);
      auto error_str = detail::generate_error_string(error_type_str, info);
      if (pe.includer_error.size()) {
         error_str.append(pe.includer_error);
      }
      return error_str;
   }

   template <class T>
   [[nodiscard]] std::string format_error(const expected<T, error_ctx>& pe, const auto& buffer)
   {
      if (not pe) {
         return format_error(pe.error(), buffer);
      }
      else {
         return "";
      }
   }

   [[nodiscard]] inline std::string format_error(const error_ctx& pe)
   {
      return std::string{meta<error_code>::keys[uint32_t(pe.ec)]};
   }

   template <class T>
   [[nodiscard]] std::string format_error(const expected<T, error_ctx>& pe)
   {
      if (not pe) {
         return format_error(pe.error());
      }
      else {
         return "";
      }
   }
}

#ifdef _MSC_VER
// restore disabled warnings
#pragma warning(pop)
#endif
