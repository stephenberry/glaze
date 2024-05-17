// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/core/context.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/util/bit_array.hpp"
#include "glaze/util/comment.hpp"
#include "glaze/util/expected.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/hash_map.hpp"
#include "glaze/util/help.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/tuple.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/utility.hpp"
#include "glaze/util/validate.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   // write out a string like type without quoting it
   template <class T>
   struct raw_t
   {
      static constexpr bool glaze_wrapper = true;
      using value_type = T;
      T& val;
   };

   template <class... T>
   struct obj final
   {
      glz::tuplet::tuple<std::conditional_t<std::is_convertible_v<std::decay_t<T>, sv>, sv, T>...> value;
      static constexpr auto glaze_reflect = false;
   };

   template <class... T>
   obj(T&&...) -> obj<T...>;

   template <class... T>
   struct obj_copy final
   {
      glz::tuplet::tuple<T...> value;
      static constexpr auto glaze_reflect = false;
   };

   template <class... T>
   obj_copy(T...) -> obj_copy<T...>;

   template <class... T>
   struct arr final
   {
      glz::tuplet::tuple<std::conditional_t<std::is_convertible_v<std::decay_t<T>, sv>, sv, T>...> value;
      static constexpr auto glaze_reflect = false;
   };

   template <class... T>
   arr(T&&...) -> arr<T...>;

   template <class... T>
   struct arr_copy final
   {
      glz::tuplet::tuple<T...> value;
   };

   template <class... T>
   arr_copy(T...) -> arr_copy<T...>;

   // used to merge multiple JSON objects into a single JSON object
   template <class... T>
   struct merge final
   {
      glz::tuplet::tuple<std::conditional_t<std::is_convertible_v<std::decay_t<T>, sv>, sv, T>...> value;
      static constexpr auto glaze_reflect = false;
   };

   template <class... T>
   merge(T&&...) -> merge<T...>;

   template <class... T>
   struct overload : T...
   {
      using T::operator()...;
   };
   // explicit deduction guide (not needed as of C++20)
   template <class... T>
   overload(T...) -> overload<T...>;

   struct hidden
   {};

   // hide class is a wrapper to denote that the exposed variable should be excluded or hidden for serialization output
   template <class T>
   struct hide final
   {
      T value;

      constexpr decltype(auto) operator()(auto&&) const { return hidden{}; }
   };

   template <class T>
   hide(T) -> hide<T>;

   struct skip
   {}; // to skip a keyed value in input

   template <class T>
   struct includer
   {
      T& value;

      static constexpr auto glaze_includer = true;
      static constexpr auto glaze_reflect = false;
   };

   template <class T>
   struct meta<includer<T>>
   {
      static constexpr std::string_view name = join_v<chars<"includer<">, name_v<T>, chars<">">>;
   };

   // Register this with an object to allow file including (direct writes) to the meta object
   struct file_include final
   {
      bool reflection_helper{}; // needed for count_members
      static constexpr auto glaze_includer = true;
      static constexpr auto glaze_reflect = false;

      constexpr decltype(auto) operator()(auto&& value) const noexcept
      {
         return includer<decay_keep_volatile_t<decltype(value)>>{value};
      }
   };

   template <class T>
   concept is_includer = requires(T t) { requires T::glaze_includer == true; };

   template <class T>
   concept is_member_function_pointer = std::is_member_function_pointer_v<T>;

   namespace detail
   {
      template <uint32_t Format>
      struct read
      {};

      template <uint32_t Format>
      struct write
      {};

      template <uint32_t Format>
      struct write_partial
      {};
   } // namespace detail

   // Use std::stringview if you know the buffer is going to outlive this
   template <class string_type = std::string>
   struct basic_raw_json
   {
      string_type str;

      basic_raw_json() = default;

      template <class T>
         requires(!std::same_as<std::decay_t<T>, basic_raw_json>)
      basic_raw_json(T&& s) : str(std::forward<T>(s))
      {}

      basic_raw_json(const basic_raw_json&) = default;
      basic_raw_json(basic_raw_json&&) = default;
      basic_raw_json& operator=(const basic_raw_json&) = default;
      basic_raw_json& operator=(basic_raw_json&&) = default;
   };

   using raw_json = basic_raw_json<std::string>;
   using raw_json_view = basic_raw_json<std::string_view>;

   template <class T>
   struct meta<basic_raw_json<T>>
   {
      static constexpr std::string_view name = "raw_json";
   };

   // glz::text (i.e. glz::basic_text<std::string>) just reads in the entire contents of the message or writes out
   // whatever contents it holds it avoids JSON reading/writing and is a pass through mechanism
   template <class string_type>
   struct basic_text
   {
      string_type str;

      basic_text() = default;

      template <class T>
         requires(!std::same_as<std::decay_t<T>, basic_text>)
      basic_text(T&& s) : str(std::forward<T>(s))
      {}

      basic_text(const basic_text&) = default;
      basic_text(basic_text&&) = default;
      basic_text& operator=(const basic_text&) = default;
      basic_text& operator=(basic_text&&) = default;
   };

   using text = basic_text<std::string>;
   using text_view = basic_text<std::string_view>;

   namespace detail
   {
      template <class T>
      concept constructible = requires { meta<std::decay_t<T>>::construct; } || local_construct_t<std::decay_t<T>>;

      template <class T>
      concept meta_value_t = glaze_t<std::decay_t<T>>;

      template <class T>
      concept str_t = !std::same_as<std::nullptr_t, T> && std::convertible_to<std::decay_t<T>, std::string_view>;

      // this concept requires that T is string and copies the string in json
      template <class T>
      concept string_t = str_t<T> && !std::same_as<std::decay_t<T>, std::string_view> && has_push_back<T>;

      template <class T>
      concept char_array_t = str_t<T> && std::is_array_v<std::remove_pointer_t<std::remove_reference_t<T>>>;

      // this concept requires that T is just a view
      template <class T>
      concept string_view_t = std::same_as<std::decay_t<T>, std::string_view>;

      template <class T>
      concept readable_map_t = !custom_read<T> && !meta_value_t<T> && !str_t<T> && range<T> &&
                               pair_t<range_value_t<T>> && map_subscriptable<T>;

      template <class T>
      concept writable_map_t =
         !custom_write<T> && !meta_value_t<T> && !str_t<T> && range<T> && pair_t<range_value_t<T>>;

      template <class Map>
      concept heterogeneous_map = requires {
         typename Map::key_compare;
         requires(std::same_as<typename Map::key_compare, std::less<>> ||
                  std::same_as<typename Map::key_compare, std::greater<>> ||
                  requires { typename Map::key_compare::is_transparent; });
      };

      template <class T>
      concept array_t = (!meta_value_t<T> && !str_t<T> && !(readable_map_t<T> || writable_map_t<T>)&&range<T>);

      template <class T>
      concept readable_array_t =
         (range<T> && !custom_read<T> && !meta_value_t<T> && !str_t<T> && !readable_map_t<T> && !filesystem_path<T>);

      template <class T>
      concept writable_array_t =
         (range<T> && !custom_write<T> && !meta_value_t<T> && !str_t<T> && !writable_map_t<T> && !filesystem_path<T>);

      template <class T>
      concept fixed_array_value_t = array_t<std::decay_t<decltype(std::declval<T>()[0])>> &&
                                    !resizable<std::decay_t<decltype(std::declval<T>()[0])>>;

      template <class T>
      concept boolean_like = std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> ||
                             std::same_as<T, std::vector<bool>::const_reference>;

      template <class T>
      concept is_no_reflect = requires(T t) { requires T::glaze_reflect == false; };

      template <class T>
      concept has_static_size = (is_span<T> && !is_dynamic_span<T>) || (requires(T container) {
                                   {
                                      std::bool_constant<(std::decay_t<T>{}.size(), true)>()
                                   } -> std::same_as<std::true_type>;
                                } && std::decay_t<T>{}.size() > 0);

      template <class T>
      constexpr size_t get_size() noexcept
      {
         if constexpr (is_span<T>) {
            return T::extent;
         }
         else {
            return std::decay_t<T>{}.size();
         }
      }

      template <class T>
      concept is_reference_wrapper = is_specialization_v<T, std::reference_wrapper>;

      template <class T>
      concept tuple_t = requires(T t) {
         std::tuple_size<T>::value;
         glz::get<0>(t);
      } && !meta_value_t<T> && !range<T>;

      template <class T>
      concept glaze_wrapper = requires { requires T::glaze_wrapper == true; };

      template <class T>
      concept always_null_t =
         std::same_as<T, std::nullptr_t> || std::convertible_to<T, std::monostate> || std::same_as<T, std::nullopt_t>;

      template <class T>
      concept nullable_t = !meta_value_t<T> && !str_t<T> && requires(T t) {
         bool(t);
         {
            *t
         };
      };

      template <class T>
      concept nullable_wrapper = glaze_wrapper<T> && nullable_t<typename T::value_type>;

      template <class T>
      concept null_t = nullable_t<T> || always_null_t<T> || nullable_wrapper<T>;

      template <class T>
      concept func_t = requires(T t) {
         typename T::result_type;
         std::function(t);
      } && !glaze_t<T>;

      template <class T>
      concept glaze_array_t = glaze_t<T> && is_specialization_v<meta_wrapper_t<T>, Array>;

      template <class T>
      concept glaze_object_t = glaze_t<T> && is_specialization_v<meta_wrapper_t<T>, Object>;

      template <class T>
      concept glaze_enum_t = glaze_t<T> && is_specialization_v<meta_wrapper_t<T>, Enum>;

      template <class T>
      concept glaze_flags_t = glaze_t<T> && is_specialization_v<meta_wrapper_t<T>, Flags>;

      template <class T>
      concept glaze_value_t =
         glaze_t<T> && !(glaze_array_t<T> || glaze_object_t<T> || glaze_enum_t<T> || glaze_flags_t<T>);

      template <class T>
      concept reflectable = std::is_aggregate_v<std::remove_cvref_t<T>> && std::is_class_v<std::remove_cvref_t<T>> &&
                            !(is_no_reflect<T> || glaze_value_t<T> || glaze_object_t<T> || glaze_array_t<T> ||
                              glaze_flags_t<T> || range<T> || pair_t<T> || null_t<T>);

      template <class T>
      concept glaze_const_value_t = glaze_value_t<T> && std::is_pointer_v<glz::meta_wrapper_t<T>> &&
                                    std::is_const_v<std::remove_pointer_t<glz::meta_wrapper_t<T>>>;

      template <class From, class To>
      concept non_narrowing_convertable = requires(From from, To to) {
#if __GNUC__
         // TODO: guard gcc against narrowing conversions when fixed
         to = from;
#else
         To{from};
#endif
      };

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

      template <class T>
      struct tuple_variant;

      template <class... Ts>
      struct tuple_variant<glz::tuplet::tuple<Ts...>> : unique<std::variant<>, Ts...>
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

      template <class Tuple, class = std::make_index_sequence<glz::tuple_size<Tuple>::value>>
      struct value_tuple_variant;

      template <class Tuple, size_t I>
      struct member_type
      {
         using T0 = decay_keep_volatile_t<glz::tuple_element_t<0, glz::tuple_element_t<I, Tuple>>>;
         using type = glz::tuple_element_t<std::is_member_pointer_v<T0> ? 0 : 1, glz::tuple_element_t<I, Tuple>>;
      };

      template <class Tuple, size_t... I>
      struct value_tuple_variant<Tuple, std::index_sequence<I...>>
      {
         using type = typename tuple_variant<decltype(glz::tuplet::tuple_cat(
            std::declval<tuplet::tuple<typename member_type<Tuple, I>::type>>()...))>::type;
      };

      template <class Tuple>
      using value_tuple_variant_t = typename value_tuple_variant<Tuple>::type;

      template <class T>
      inline constexpr auto make_array()
      {
         return []<size_t... I>(std::index_sequence<I...>) {
            using value_t = typename tuple_variant<meta_t<T>>::type;
            return std::array<value_t, glz::tuple_size_v<meta_t<T>>>{glz::get<I>(meta_v<T>)...};
         }(std::make_index_sequence<glz::tuple_size_v<meta_t<T>>>{});
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

      template <class T, size_t I>
      constexpr auto key_value() noexcept
      {
         using value_t = value_tuple_variant_t<meta_t<T>>;
         constexpr auto first = get<0>(get<I>(meta_v<T>));
         using T0 = std::decay_t<decltype(first)>;
         if constexpr (std::is_member_pointer_v<T0>) {
            return std::pair<sv, value_t>{get_name<first>(), first};
         }
         else {
            return std::pair<sv, value_t>{sv(first), get<1>(get<I>(meta_v<T>))};
         }
      }

      template <class T, size_t I>
      constexpr sv get_key() noexcept
      {
         constexpr auto first = get<0>(get<I>(meta_v<T>));
         using T0 = std::decay_t<decltype(first)>;
         if constexpr (std::is_member_pointer_v<T0>) {
            return get_name<first>();
         }
         else {
            return {first};
         }
      }

      template <class T, size_t I>
      constexpr sv get_enum_key() noexcept
      {
         constexpr auto first = get<0>(get<I>(meta_v<T>));
         using T0 = std::decay_t<decltype(first)>;
         if constexpr (std::is_enum_v<T0>) {
            return get_name<first>();
         }
         else {
            return {first};
         }
      }

      template <class T, size_t I>
      constexpr auto get_enum_value() noexcept
      {
         constexpr auto first = get<0>(get<I>(meta_v<T>));
         using T0 = std::decay_t<decltype(first)>;
         if constexpr (std::is_enum_v<T0>) {
            return first;
         }
         else {
            return get<1>(get<I>(meta_v<T>));
         }
      }

      template <class T, size_t I>
      struct meta_sv
      {
         static constexpr sv value = get_key<T, I>();
      };

      template <class T, bool use_hash_comparison, size_t... I>
      constexpr auto make_map_impl(std::index_sequence<I...>)
      {
         using value_t = value_tuple_variant_t<meta_t<T>>;
         constexpr auto n = glz::tuple_size_v<meta_t<T>>;

         if constexpr (n == 0) {
            return nullptr; // Hack to fix MSVC
            // static_assert(false_v<T>, "Empty object map is illogical. Handle empty upstream.");
         }
         else if constexpr (n == 1) {
            return micro_map1<value_t, meta_sv<T, I>::value...>{key_value<T, I>()...};
         }
         else if constexpr (n == 2) {
            return micro_map2<value_t, meta_sv<T, I>::value...>{key_value<T, I>()...};
         }
         else if constexpr (n < 64) // don't even attempt a first character hash if we have too many keys
         {
            constexpr std::array<sv, n> keys{get_key<T, I>()...};
            constexpr auto front_desc = single_char_hash<n>(keys);

            if constexpr (front_desc.valid) {
               return make_single_char_map<value_t, front_desc>({key_value<T, I>()...});
            }
            else {
               constexpr single_char_hash_opts rear_hash{.is_front_hash = false};
               constexpr auto back_desc = single_char_hash<n, rear_hash>(keys);

               if constexpr (back_desc.valid) {
                  return make_single_char_map<value_t, back_desc>({key_value<T, I>()...});
               }
               else {
                  constexpr single_char_hash_opts sum_hash{.is_front_hash = true, .is_sum_hash = true};
                  constexpr auto sum_desc = single_char_hash<n, sum_hash>(keys);

                  if constexpr (sum_desc.valid) {
                     return make_single_char_map<value_t, sum_desc>({key_value<T, I>()...});
                  }
                  else {
                     if constexpr (n <= naive_map_max_size) {
                        constexpr auto naive_desc = naive_map_hash<use_hash_comparison, n>(keys);
                        return glz::detail::make_naive_map<value_t, naive_desc>({key_value<T, I>()...});
                     }
                     else {
                        return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>({key_value<T, I>()...});
                     }
                  }
               }
            }
         }
         else {
            return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>({key_value<T, I>()...});
         }
      }

      template <class T, bool use_hash_comparison = false>
         requires(!reflectable<T>)
      constexpr auto make_map()
      {
         constexpr auto indices = std::make_index_sequence<glz::tuple_size_v<meta_t<T>>>{};
         return make_map_impl<decay_keep_volatile_t<T>, use_hash_comparison>(indices);
      }

      template <class T>
      constexpr auto make_key_int_map()
      {
         constexpr auto N = glz::tuple_size_v<meta_t<T>>;
         return [&]<size_t... I>(std::index_sequence<I...>) {
            return normal_map<sv, size_t, glz::tuple_size_v<meta_t<T>>>(
               {std::make_pair<sv, size_t>(get_enum_key<T, I>(), I)...});
         }(std::make_index_sequence<N>{});
      }

      template <class T>
      constexpr auto make_enum_to_string_map()
      {
         constexpr auto N = glz::tuple_size_v<meta_t<T>>;
         return [&]<size_t... I>(std::index_sequence<I...>) {
            using key_t = std::underlying_type_t<T>;
            return normal_map<key_t, sv, N>(
               {std::make_pair<key_t, sv>(static_cast<key_t>(get_enum_value<T, I>()), get_enum_key<T, I>())...});
         }(std::make_index_sequence<N>{});
      }

      // TODO: This faster approach can be used if the enum has an integer type base and sequential numbering
      template <class T>
      constexpr auto make_enum_to_string_array() noexcept
      {
         return []<size_t... I>(std::index_sequence<I...>) {
            return std::array<sv, sizeof...(I)>{get_enum_key<T, I>()...};
         }(std::make_index_sequence<glz::tuple_size_v<meta_t<T>>>{});
      }

      template <class T>
      constexpr auto make_string_to_enum_map() noexcept
      {
         constexpr auto N = glz::tuple_size_v<meta_t<T>>;
         return [&]<size_t... I>(std::index_sequence<I...>) {
            return normal_map<sv, T, N>({std::pair<sv, T>{get_enum_key<T, I>(), T(get_enum_value<T, I>())}...});
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

      template <class T, size_t N>
      constexpr size_t get_max_keys = [] {
         size_t res{};
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            if constexpr (reflectable<V>) {
               res += count_members<V>;
            }
            else {
               res += glz::tuple_size_v<meta_t<V>>;
            }
         });
         return res;
      }();

      template <class T>
      constexpr auto get_combined_keys_from_variant()
      {
         constexpr auto N = std::variant_size_v<T>;

         std::array<std::string_view, get_max_keys<T, N>> keys{};
         // This intermediate pointer is necessary for GCC 13 (otherwise segfaults with reflection logic)
         auto* data_ptr = &keys;
         size_t index = 0;
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            if constexpr (reflectable<V>) {
               for_each<glz::tuple_size_v<decltype(member_names<V>)>>(
                  [&](auto J) { (*data_ptr)[index++] = glz::get<J>(member_names<V>); });
            }
            else {
               for_each<glz::tuple_size_v<meta_t<V>>>([&](auto J) {
                  constexpr auto item = get<J>(meta_v<V>);
                  using T0 = std::decay_t<decltype(get<0>(item))>;
                  auto key_getter = [&] {
                     if constexpr (std::is_member_pointer_v<T0>) {
                        return get_name<get<0>(get<J>(meta_v<V>))>();
                     }
                     else {
                        return get<0>(get<J>(meta_v<V>));
                     }
                  };
                  (*data_ptr)[index++] = key_getter();
               });
            }
         });

         std::sort(keys.begin(), keys.end());
         const auto end = std::unique(keys.begin(), keys.end());
         const auto size = std::distance(keys.begin(), end);

         return std::pair{keys, size};
      }

      template <class T, size_t... I>
      consteval auto make_variant_deduction_base_map(std::index_sequence<I...>, auto&& keys)
      {
         using V = bit_array<std::variant_size_v<T>>;
         return normal_map<sv, V, sizeof...(I)>({std::make_pair<sv, V>(sv(std::get<I>(keys)), V{})...});
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
            if constexpr (reflectable<V>) {
               for_each<glz::tuple_size_v<decltype(member_names<V>)>>(
                  [&](auto J) { deduction_map.find(get<J>(member_names<V>))->second[I] = true; });
            }
            else {
               for_each<glz::tuple_size_v<meta_t<V>>>([&](auto J) {
                  constexpr auto item = get<J>(meta_v<V>);
                  using T0 = std::decay_t<decltype(get<0>(item))>;
                  auto key_getter = [&] {
                     if constexpr (std::is_member_pointer_v<T0>) {
                        return get_name<get<0>(get<J>(meta_v<V>))>();
                     }
                     else {
                        return get<0>(get<J>(meta_v<V>));
                     }
                  };
                  deduction_map.find(key_getter())->second[I] = true;
               });
            }
         });

         return deduction_map;
      }

      template <is_variant T, size_t... I>
      constexpr auto make_variant_id_map_impl(std::index_sequence<I...>, auto&& variant_ids)
      {
         return normal_map<sv, size_t, std::variant_size_v<T>>({std::make_pair<sv, size_t>(sv(variant_ids[I]), I)...});
      }

      template <is_variant T>
      constexpr auto make_variant_id_map()
      {
         constexpr auto indices = std::make_index_sequence<std::variant_size_v<T>>{};

         return make_variant_id_map_impl<T>(indices, ids_v<T>);
      }

      template <class Value, class MemPtr>
      inline decltype(auto) get_member(Value&& value, MemPtr&& member_ptr)
      {
         using V = std::decay_t<decltype(member_ptr)>;
         if constexpr (std::is_member_object_pointer_v<V>) {
            return value.*member_ptr;
         }
         else if constexpr (std::is_member_function_pointer_v<V>) {
            return member_ptr;
         }
         else if constexpr (std::invocable<MemPtr, Value>) {
            return std::invoke(std::forward<MemPtr>(member_ptr), std::forward<Value>(value));
         }
         else if constexpr (std::is_pointer_v<V>) {
            if constexpr (std::invocable<decltype(*member_ptr), Value>) {
               return std::invoke(*member_ptr, std::forward<Value>(value));
            }
            else {
               return *member_ptr;
            }
         }
         else {
            return member_ptr;
         }
      }

      // member_ptr and lambda wrapper helper
      template <template <class> class Wrapper, class Wrapped>
      struct wrap
      {
         Wrapped wrapped;
         constexpr decltype(auto) operator()(auto&& value) const
         {
            return Wrapper<std::decay_t<decltype(get_member(value, wrapped))>>{get_member(value, wrapped)};
         }

         constexpr decltype(auto) unwrap(auto&& value) const { return get_member(value, wrapped); }
      };

      template <class T, class mptr_t>
      using member_t = decltype(get_member(std::declval<T>(), std::declval<std::decay_t<mptr_t>&>()));

      template <class T, class = std::make_index_sequence<glz::tuple_size<meta_t<T>>::value>>
      struct members_from_meta;

      template <class T, size_t... I>
      inline constexpr auto members_from_meta_impl()
      {
         if constexpr (glaze_object_t<std::decay_t<T>>) {
            return glz::tuplet::tuple<std::decay_t<member_t<T, typename member_type<meta_t<T>, I>::type>>...>{};
         }
         else {
            return glz::tuplet::tuple{};
         }
      }

      template <class T, size_t... I>
      struct members_from_meta<T, std::index_sequence<I...>>
      {
         using type = decltype(members_from_meta_impl<T, I...>());
      };

      template <class T>
      using member_tuple_t = typename members_from_meta<T>::type;

      // Output variants in the following format  ["variant_type", variant_json_data] with
      // glz::detail:array_variant(&T::var);
      template <is_variant T>
      struct array_variant_wrapper
      {
         T& value;
      };
      // TODO: Could do this if the compiler supports alias template deduction
      // template <class T>
      // using array_var = wrap<array_var_wrapper, T>;
      template <class T>
      struct array_variant : wrap<array_variant_wrapper, T>
      {};
      template <class T>
      array_variant(T) -> array_variant<T>; // Only needed on older compilers until we move to template alias deduction
   } // namespace detail

   constexpr decltype(auto) conv_sv(auto&& value) noexcept
   {
      using V = std::decay_t<decltype(value)>;
      if constexpr (std::is_convertible_v<V, sv>) {
         return sv{value};
      }
      else {
         return value;
      }
   }

   constexpr auto array(auto&&... args) noexcept { return detail::Array{glz::tuplet::tuple{conv_sv(args)...}}; }

   constexpr auto object(auto&&... args) noexcept
   {
      if constexpr (sizeof...(args) == 0) {
         return glz::detail::Object{glz::tuplet::tuple{}};
      }
      else {
         using Tuple = std::decay_t<decltype(glz::tuplet::tuple{conv_sv(args)...})>;
         return glz::detail::Object{group_builder<Tuple>::op(glz::tuplet::tuple{conv_sv(args)...})};
      }
   }

   constexpr auto enumerate(auto&&... args) noexcept
   {
      using Tuple = std::decay_t<decltype(glz::tuplet::tuple{conv_sv(args)...})>;
      return glz::detail::Enum{group_builder<Tuple>::op(glz::tuplet::tuple{conv_sv(args)...})};
   }

   // A faster compiling version of enumerate that does not support reflection
   constexpr auto enumerate_no_reflect(auto&&... args) noexcept
   {
      return [t = glz::tuplet::tuple{args...}]<size_t... I>(std::index_sequence<I...>) noexcept {
         return glz::detail::Enum{std::array{std::pair{conv_sv(get<2 * I>(t)), get<2 * I + 1>(t)}...}};
      }(std::make_index_sequence<sizeof...(args) / 2>{});
   }

   constexpr auto flags(auto&&... args) noexcept
   {
      using Tuple = std::decay_t<decltype(glz::tuplet::tuple{conv_sv(args)...})>;
      return glz::detail::Flags{group_builder<Tuple>::op(glz::tuplet::tuple{conv_sv(args)...})};
   }

   template <detail::glaze_flags_t T>
   consteval auto byte_length() noexcept
   {
      constexpr auto N = glz::tuple_size_v<meta_t<T>>;

      if constexpr (N % 8 == 0) {
         return N / 8;
      }
      else {
         return (N / 8) + 1;
      }
   }
}

namespace glz
{
   template <class T>
   inline constexpr auto reflection_count = [] {
      if constexpr (detail::reflectable<T>) {
         return detail::count_members<T>;
      }
      else {
         return glz::tuple_size_v<meta_t<T>>;
      }
   }();
}

template <>
struct glz::meta<glz::error_code>
{
   static constexpr sv name = "glz::error_code";
   using enum glz::error_code;
   static constexpr auto value =
      enumerate_no_reflect("none", none, //
                           "no_read_input", no_read_input, //
                           "data_must_be_null_terminated", data_must_be_null_terminated, //
                           "parse_number_failure", parse_number_failure, //
                           "expected_brace", expected_brace, //
                           "expected_bracket", expected_bracket, //
                           "expected_quote", expected_quote, //
                           "expected_comma", expected_comma, //
                           "expected_colon", expected_colon, //
                           "exceeded_static_array_size", exceeded_static_array_size, //
                           "unexpected_end", unexpected_end, //
                           "expected_end_comment", expected_end_comment, //
                           "syntax_error", syntax_error, //
                           "key_not_found", key_not_found, //
                           "unexpected_enum", unexpected_enum, //
                           "attempt_const_read", attempt_const_read, //
                           "attempt_member_func_read", attempt_member_func_read, //
                           "attempt_read_hidden", attempt_read_hidden, //
                           "invalid_nullable_read", invalid_nullable_read, //
                           "invalid_variant_object", invalid_variant_object, //
                           "invalid_variant_array", invalid_variant_array, //
                           "invalid_variant_string", invalid_variant_string, //
                           "no_matching_variant_type", no_matching_variant_type, //
                           "expected_true_or_false", expected_true_or_false, //
                           "unknown_key", unknown_key, //
                           "invalid_flag_input", invalid_flag_input, //
                           "invalid_escape", invalid_escape, //
                           "u_requires_hex_digits", u_requires_hex_digits, //
                           "file_extension_not_supported", file_extension_not_supported, //
                           "could_not_determine_extension", could_not_determine_extension, //
                           "seek_failure", seek_failure, //
                           "unicode_escape_conversion_failure", unicode_escape_conversion_failure, //
                           "file_open_failure", file_open_failure, //
                           "file_close_failure", file_close_failure, //
                           "file_include_error", file_include_error, //
                           "dump_int_error", dump_int_error, //
                           "get_nonexistent_json_ptr", get_nonexistent_json_ptr, //
                           "get_wrong_type", get_wrong_type, //
                           "cannot_be_referenced", cannot_be_referenced, //
                           "invalid_get", invalid_get, //
                           "invalid_get_fn", invalid_get_fn, //
                           "invalid_call", invalid_call, //
                           "invalid_partial_key", invalid_partial_key, //
                           "name_mismatch", name_mismatch, //
                           "array_element_not_found", array_element_not_found, //
                           "elements_not_convertible_to_design", elements_not_convertible_to_design, //
                           "unknown_distribution", unknown_distribution, //
                           "invalid_distribution_elements", invalid_distribution_elements, //
                           "missing_key", missing_key, //
                           "hostname_failure", hostname_failure, //
                           "includer_error", includer_error //
      );
};

namespace glz
{
   // This wraps glz::expected error (unexpected) values in an object with an "error" key
   // This makes them discernable from the expected value
   template <class T>
   struct unexpected_wrapper
   {
      T* unexpected;

      struct glaze
      {
         using V = unexpected_wrapper;
         static constexpr auto value = glz::object("unexpected", &V::unexpected);
      };
   };

   template <class T>
   unexpected_wrapper(T*) -> unexpected_wrapper<T>;
}

namespace glz
{
   template <auto Enum>
      requires(std::is_enum_v<decltype(Enum)>)
   constexpr sv enum_name_v = []() -> std::string_view {
      using T = std::decay_t<decltype(Enum)>;

      if constexpr (detail::glaze_t<T>) {
         using U = std::underlying_type_t<T>;
         return detail::get_enum_key<T, static_cast<U>(Enum)>();
      }
      else {
         static_assert(false_v<decltype(Enum)>, "Enum requires glaze metadata for name");
      }
   }();

   [[nodiscard]] inline std::string format_error(const parse_error& pe, const auto& buffer)
   {
      static constexpr auto arr = detail::make_enum_to_string_array<error_code>();
      const auto error_type_str = arr[static_cast<uint32_t>(pe.ec)];

      const auto info = detail::get_source_info(buffer, pe.location);
      if (info) {
         auto error_str = detail::generate_error_string(error_type_str, *info);
         if (pe.includer_error.size()) {
            error_str += pe.includer_error;
         }
         return error_str;
      }
      auto error_str = std::string(error_type_str);
      if (pe.includer_error.size()) {
         error_str += pe.includer_error;
      }
      return error_str;
   }
}

namespace glz::detail
{
   // This useless code and the inclusion of N is required for MSVC to build, but not Clang or GCC
   template <size_t I, size_t N, class T>
   struct glaze_tuple_element
   {
      using V = std::decay_t<T>;
      using Item = tuplet::tuple<>;
      using T0 = T;
      static constexpr bool use_reflection = false; // for member object reflection
      static constexpr size_t member_index = 0;
      using mptr_t = T;
      using type = T;
   };

   // This shouldn't need the requires or the N template paramter, except for the current MSVC
   template <size_t I, size_t N, class T>
      requires(N > 0 && !reflectable<T>)
   struct glaze_tuple_element<I, N, T>
   {
      using V = std::decay_t<T>;
      using Item = std::decay_t<decltype(glz::get<I>(meta_v<V>))>;
      using T0 = std::decay_t<glz::tuple_element_t<0, Item>>;
      static constexpr bool use_reflection = std::is_member_pointer_v<T0>; // for member object reflection
      static constexpr size_t member_index = use_reflection ? 0 : 1;
      using mptr_t = std::decay_t<glz::tuple_element_t<member_index, Item>>;
      using type = member_t<V, mptr_t>;
   };

   template <size_t I, size_t N, reflectable T>
      requires(N > 0)
   struct glaze_tuple_element<I, N, T>
   {
      using V = std::decay_t<T>;
      static constexpr bool use_reflection = false; // for member object reflection
      static constexpr size_t member_index = 0;
      using Item = decltype(to_tuple(std::declval<T>()));
      using mptr_t = glz::tuple_element_t<I, Item>;
      using type = member_t<V, mptr_t>;
      using T0 = mptr_t;
   };

   template <size_t I, size_t N, class T>
   using glaze_tuple_element_t = typename glaze_tuple_element<I, N, T>::type;

   template <auto Opts, class T>
   struct object_type_info
   {
      static constexpr auto N = reflection_count<T>;

      // Allows us to remove a branch if the first item will always be written
      static constexpr bool first_will_be_written = [] {
         if constexpr (N > 0) {
            using Element = glaze_tuple_element<0, N, T>;
            using V = std::remove_cvref_t<typename Element::type>;

            if constexpr (null_t<V> && Opts.skip_null_members) {
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
               using Element = glaze_tuple_element<I, N, T>;
               using V = std::remove_cvref_t<typename Element::type>;

               if constexpr (Opts.skip_null_members && null_t<V>) {
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

   template <size_t I, class T, bool use_reflection>
   static constexpr auto key_name = [] {
      if constexpr (reflectable<T>) {
         return get<I>(member_names<T>);
      }
      else {
         using V = std::decay_t<T>;
         if constexpr (use_reflection) {
            return get_name<get<0>(get<I>(meta_v<V>))>();
         }
         else {
            return get<0>(get<I>(meta_v<V>));
         }
      }
   }();

   template <class T, auto Opts>
   constexpr auto required_fields()
   {
      constexpr auto N = reflection_count<T>;

      bit_array<N> fields{};
      if constexpr (Opts.error_on_missing_keys) {
         for_each<N>([&](auto I) constexpr {
            using Element = glaze_tuple_element<I, N, T>;
            fields[I] = !bool(Opts.skip_null_members) || !null_t<std::decay_t<typename Element::type>>;
         });
      }
      return fields;
   }
}
