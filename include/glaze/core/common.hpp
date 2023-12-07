// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "glaze/api/name.hpp"
#include "glaze/core/context.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/util/bit_array.hpp"
#include "glaze/util/expected.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/hash_map.hpp"
#include "glaze/util/string_view.hpp"
#include "glaze/util/tuple.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   // write out a string like type without quoting it
   template <class T>
   struct raw_t
   {
      using value_type = T;
      T& val;
   };

   // Allows developers to add `static constexpr auto custom_read = true;` to their glz::meta to prevent ambiguous
   // partial specialization for custom parsers
   template <class T>
   concept custom_read = requires { meta<T>::custom_read == true; };

   template <class T>
   concept custom_write = requires { meta<T>::custom_write == true; };

   template <class... T>
   struct obj final
   {
      glz::tuplet::tuple<std::conditional_t<std::is_convertible_v<std::decay_t<T>, sv>, sv, T>...> value;
      static constexpr auto reflect = false;
   };

   template <class... T>
   obj(T&&...) -> obj<T...>;

   template <class... T>
   struct obj_copy final
   {
      glz::tuplet::tuple<T...> value;
      static constexpr auto reflect = false;
   };

   template <class... T>
   obj_copy(T...) -> obj_copy<T...>;

   template <class... T>
   struct arr final
   {
      glz::tuplet::tuple<std::conditional_t<std::is_convertible_v<std::decay_t<T>, sv>, sv, T>...> value;
      static constexpr auto reflect = false;
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
      static constexpr auto reflect = false;
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

   template <class T, class... U>
   concept is_any_of = (std::same_as<T, U> || ...);

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
   };

   template <class T>
   struct meta<includer<T>>
   {
      static constexpr std::string_view name = detail::join_v<chars<"includer<">, name_v<T>, chars<">">>;
   };

   // Register this with an object to allow file including (direct writes) to the meta object
   struct file_include
   {
      constexpr decltype(auto) operator()(auto&& value) const { return includer<std::decay_t<decltype(value)>>{value}; }
   };

   template <class T>
   concept range = requires(T& t) {
      requires !std::same_as<void, decltype(t.begin())>;
      requires !std::same_as<void, decltype(t.end())>;
      requires std::input_iterator<decltype(t.begin())>;
   };

   // range like
   template <class T>
   using iterator_t = decltype(std::begin(std::declval<T&>()));

   template <range R>
   using range_value_t = std::iter_value_t<iterator_t<R>>;

   template <range R>
   [[nodiscard]] constexpr bool empty_range(R&& rng)
   {
#ifdef __cpp_lib_ranges
      return std::ranges::empty(rng);
#else
      // in lieu of std::ranges::empty
      if constexpr (requires() {
                       {
                          rng.empty()
                       } -> std::convertible_to<bool>;
                    }) {
         return rng.empty();
      }
      else if constexpr (requires() {
                            {
                               rng.size()
                            } -> std::same_as<std::size_t>;
                         }) {
         return rng.size() == std::size_t{0};
      }
      else {
         return std::cbegin(rng) == std::cend(rng);
      }
#endif
   }

   template <class T>
   concept is_member_function_pointer = std::is_member_function_pointer_v<T>;

   namespace detail
   {
      template <int... I>
      using is = std::integer_sequence<int, I...>;
      template <int N>
      using make_is = std::make_integer_sequence<int, N>;

      constexpr auto size(const char* s) noexcept
      {
         int i = 0;
         while (*s != 0) {
            ++i;
            ++s;
         }
         return i;
      }

      template <const char*, typename, const char*, typename>
      struct concat_impl;

      template <const char* S1, int... I1, const char* S2, int... I2>
      struct concat_impl<S1, is<I1...>, S2, is<I2...>>
      {
         static constexpr const char value[]{S1[I1]..., S2[I2]..., 0};
      };

      template <const char* S1, const char* S2>
      constexpr auto concat_char()
      {
         return concat_impl<S1, make_is<size(S1)>, S2, make_is<size(S2)>>::value;
      }

      template <size_t... Is>
      struct seq
      {};
      template <size_t N, size_t... Is>
      struct gen_seq : gen_seq<N - 1, N - 1, Is...>
      {};
      template <size_t... Is>
      struct gen_seq<0, Is...> : seq<Is...>
      {};

      template <size_t N1, size_t... I1, size_t N2, size_t... I2>
      constexpr std::array<const char, N1 + N2 - 1> concat(const char (&a1)[N1], const char (&a2)[N2], seq<I1...>,
                                                           seq<I2...>)
      {
         return {{a1[I1]..., a2[I2]...}};
      }

      template <size_t N1, size_t N2>
      constexpr std::array<const char, N1 + N2 - 1> concat_arrays(const char (&a1)[N1], const char (&a2)[N2])
      {
         return concat(a1, a2, gen_seq<N1 - 1>{}, gen_seq<N2>{});
      }

      template <uint32_t Format>
      struct read
      {};

      template <uint32_t Format>
      struct write
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

   using basic =
      std::variant<bool, char, char8_t, unsigned char, signed char, char16_t, short, unsigned short, wchar_t, char32_t,
                   float, int, unsigned int, long, unsigned long, double, long long, unsigned long long, std::string>;

   // Explicitly defining basic_ptr to avoid additional template instantiations
   using basic_ptr = std::variant<bool*, char*, char8_t*, unsigned char*, signed char*, char16_t*, short*,
                                  unsigned short*, wchar_t*, char32_t*, float*, int*, unsigned int*, long*,
                                  unsigned long*, double*, long long*, unsigned long long*, std::string*>;

   namespace detail
   {
      template <class T>
      concept char_t = std::same_as<std::decay_t<T>, char> || std::same_as<std::decay_t<T>, char16_t> ||
                       std::same_as<std::decay_t<T>, char32_t> || std::same_as<std::decay_t<T>, wchar_t>;

      template <class T>
      concept bool_t =
         std::same_as<std::decay_t<T>, bool> || std::same_as<std::decay_t<T>, std::vector<bool>::reference>;

      template <class T>
      concept int_t = std::integral<std::decay_t<T>> && !char_t<std::decay_t<T>> && !bool_t<T>;

      template <class T>
      concept num_t = std::floating_point<std::decay_t<T>> || int_t<T>;

      template <typename T>
      concept complex_t = requires(T a, T b) {
         {
            a.real()
         } -> std::convertible_to<typename T::value_type>;
         {
            a.imag()
         } -> std::convertible_to<typename T::value_type>;
         {
            T(a.real(), a.imag())
         } -> std::same_as<T>;
         {
            a + b
         } -> std::same_as<T>;
         {
            a - b
         } -> std::same_as<T>;
         {
            a* b
         } -> std::same_as<T>;
         {
            a / b
         } -> std::same_as<T>;
      };

      template <class T>
      concept constructible = requires { meta<std::decay_t<T>>::construct; } || local_construct_t<std::decay_t<T>>;

      template <class T>
      concept meta_value_t = glaze_t<std::decay_t<T>>;

      template <class T>
      concept str_t = !std::same_as<std::nullptr_t, T> && std::convertible_to<std::decay_t<T>, std::string_view>;

      template <class T>
      concept has_push_back = requires(T t, typename T::value_type v) { t.push_back(v); };

      // this concept requires that T is string and copies the string in json
      template <class T>
      concept string_t = str_t<T> && !std::same_as<std::decay_t<T>, std::string_view> && has_push_back<T>;

      template <class T>
      concept char_array_t = str_t<T> && std::is_array_v<std::remove_pointer_t<std::remove_reference_t<T>>>;

      // this concept requires that T is just a view
      template <class T>
      concept str_view_t = std::same_as<std::decay_t<T>, std::string_view>;

      template <class T>
      concept pair_t = requires(T pair) {
         {
            pair.first
         } -> std::same_as<typename T::first_type&>;
         {
            pair.second
         } -> std::same_as<typename T::second_type&>;
      };

      template <class T>
      concept map_subscriptable = requires(T container) {
         {
            container[std::declval<typename T::key_type>()]
         } -> std::same_as<typename T::mapped_type&>;
      };

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
      concept readable_array_t = (!custom_read<T> && array_t<T>);

      template <class T>
      concept writable_array_t = (!custom_write<T> && array_t<T>);

      template <class T>
      concept emplace_backable = requires(T container) {
         {
            container.emplace_back()
         } -> std::same_as<typename T::reference>;
      };

      template <class T>
      concept emplaceable = requires(T container) {
         {
            container.emplace(std::declval<typename T::value_type>())
         };
      };

      template <class T>
      concept push_backable = requires(T container) {
         {
            container.push_back(std::declval<typename T::value_type>())
         };
      };

      template <class T>
      concept resizeable = requires(T container) { container.resize(0); };

      template <class T>
      concept erasable = requires(T container) { container.erase(container.cbegin(), container.cend()); };

      template <class T>
      concept fixed_array_value_t = array_t<std::decay_t<decltype(std::declval<T>()[0])>> &&
                                    !resizeable<std::decay_t<decltype(std::declval<T>()[0])>>;

      template <class T>
      concept has_size = requires(T container) { container.size(); };

      template <class T>
      concept has_empty = requires(T container) {
         {
            container.empty()
         } -> std::convertible_to<bool>;
      };

      template <class T>
      concept has_data = requires(T container) { container.data(); };

      template <class T>
      concept contiguous = has_size<T> && has_data<T>;

      template <class T>
      concept accessible = requires(T container) {
         {
            container[size_t{}]
         } -> std::same_as<typename T::reference>;
      };

      template <class T>
      concept boolean_like = std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> ||
                             std::same_as<T, std::vector<bool>::const_reference>;

      template <class T>
      concept vector_like = resizeable<T> && accessible<T> && has_data<T>;

      template <class T>
      concept is_span = requires(T t) {
         T::extent;
         typename T::element_type;
      };

      template <class T>
      concept is_no_reflect = requires(T t) { requires T::reflect == false; };

      template <class T>
      concept is_dynamic_span = T::extent == static_cast<size_t>(-1);

      template <class T>
      concept has_static_size = (is_span<T> && !is_dynamic_span<T>) || (requires(T container) {
                                   {
                                      std::bool_constant<(std::decay_t<T>{}.size(), true)>()
                                   } -> std::same_as<std::true_type>;
                                } && std::decay_t<T>{}.size() > 0);

      template <typename T>
      concept is_bitset = requires(T bitset) {
         bitset.flip();
         bitset.set(0);
         {
            bitset.to_string()
         } -> std::same_as<std::string>;
         {
            bitset.count()
         } -> std::same_as<std::size_t>;
      };

      template <class T>
      concept is_float128 = requires(T x) {
         requires sizeof(x) == 16;
         requires std::floating_point<T>;
      };

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
      concept raw_nullable = is_specialization_v<T, raw_t> && requires { requires nullable_t<typename T::value_type>; };

      template <class T>
      concept null_t = nullable_t<T> || always_null_t<T> || raw_nullable<T>;

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
      concept reflectable =
         !(is_no_reflect<T> || glaze_value_t<T> || glaze_object_t<T> || glaze_array_t<T> || glaze_flags_t<T> ||
           range<T> || pair_t<T> || null_t<T>)&&std::is_aggregate_v<std::remove_cvref_t<T>> &&
         std::is_class_v<T>;

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

      template <class Tuple, class = std::make_index_sequence<std::tuple_size<Tuple>::value>>
      struct value_tuple_variant;

      template <class Tuple, size_t I>
      struct member_type
      {
         using T0 = std::decay_t<std::tuple_element_t<0, std::tuple_element_t<I, Tuple>>>;
         using type = std::tuple_element_t<std::is_member_object_pointer_v<T0> ? 0 : 1, std::tuple_element_t<I, Tuple>>;
      };

      template <class Tuple, size_t... I>
      struct value_tuple_variant<Tuple, std::index_sequence<I...>>
      {
         using type = typename tuple_variant<decltype(glz::tuplet::tuple_cat(
            std::declval<tuplet::tuple<typename member_type<Tuple, I>::type>>()...))>::type;
      };

      template <class Tuple>
      using value_tuple_variant_t = typename value_tuple_variant<Tuple>::type;

      template <class T, size_t... I>
      inline constexpr auto make_array_impl(std::index_sequence<I...>)
      {
         using value_t = typename tuple_variant<meta_t<T>>::type;
         return std::array<value_t, std::tuple_size_v<meta_t<T>>>{glz::get<I>(meta_v<T>)...};
      }

      template <class T>
      inline constexpr auto make_array()
      {
         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_array_impl<T>(indices);
      }

      template <class Tuple, std::size_t... Is>
      inline constexpr auto tuple_runtime_getter(std::index_sequence<Is...>)
      {
         using value_t = typename tuple_ptr_variant<Tuple>::type;
         using tuple_ref = std::add_lvalue_reference_t<Tuple>;
         using getter_t = value_t (*)(tuple_ref);
         return std::array<getter_t, std::tuple_size_v<Tuple>>{+[](tuple_ref t) -> value_t {
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
         static constexpr auto indices = std::make_index_sequence<std::tuple_size_v<T>>{};
         static constexpr auto runtime_getter = tuple_runtime_getter<T>(indices);
         return runtime_getter[index](t);
      }

      template <class T, size_t I>
      constexpr auto key_value() noexcept
      {
         using value_t = value_tuple_variant_t<meta_t<T>>;
         constexpr auto first = get<0>(get<I>(meta_v<T>));
         using T0 = std::decay_t<decltype(first)>;
         if constexpr (std::is_member_object_pointer_v<T0>) {
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
         if constexpr (std::is_member_object_pointer_v<T0>) {
            return get_name<first>();
         }
         else {
            return {first};
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
         constexpr auto n = std::tuple_size_v<meta_t<T>>;

         auto naive_or_normal_hash = [&] {
            if constexpr (n <= 20) {
               return glz::detail::naive_map<value_t, n, use_hash_comparison>({key_value<T, I>()...});
            }
            else {
               return glz::detail::normal_map<sv, value_t, n, use_hash_comparison>({key_value<T, I>()...});
            }
         };

         if constexpr (n == 0) {
            static_assert(false_v<T>, "Empty object map is illogical. Handle empty upstream.");
         }
         else if constexpr (n == 1) {
            return micro_map1<value_t, meta_sv<T, I>::value...>{key_value<T, I>()...};
         }
         else if constexpr (n == 2) {
            return micro_map2<value_t, meta_sv<T, I>::value...>{key_value<T, I>()...};
         }
         else if constexpr (n < 128) // don't even attempt a first character hash if we have too many keys
         {
            constexpr auto front_desc = single_char_hash<n>(std::array<sv, n>{get_key<T, I>()...});

            if constexpr (front_desc.valid) {
               return make_single_char_map<value_t, front_desc>({key_value<T, I>()...});
            }
            else {
               constexpr auto back_desc = single_char_hash<n, false>(std::array<sv, n>{get_key<T, I>()...});

               if constexpr (back_desc.valid) {
                  return make_single_char_map<value_t, back_desc>({key_value<T, I>()...});
               }
               else {
                  return naive_or_normal_hash();
               }
            }
         }
         else {
            return naive_or_normal_hash();
         }
      }

      template <class T, bool use_hash_comparison = false>
      constexpr auto make_map()
      {
         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_map_impl<std::decay_t<T>, use_hash_comparison>(indices);
      }

      template <class T, size_t... I>
      constexpr auto make_int_storage_impl(std::index_sequence<I...>)
      {
         using value_t = value_tuple_variant_t<meta_t<T>>;
         return std::array<value_t, std::tuple_size_v<meta_t<T>>>({glz::get<1>(glz::get<I>(meta_v<T>))...});
      }

      template <class T>
      constexpr auto make_int_storage()
      {
         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_int_storage_impl<T>(indices);
      }

      template <class T, size_t... I>
      constexpr auto make_key_int_map_impl(std::index_sequence<I...>)
      {
         return normal_map<sv, size_t, std::tuple_size_v<meta_t<T>>>(
            {std::make_pair<sv, size_t>(glz::get<0>(glz::get<I>(meta_v<T>)), I)...});
      }

      template <class T>
      constexpr auto make_key_int_map()
      {
         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_key_int_map_impl<T>(indices);
      }

      template <class T, size_t... I>
      constexpr auto make_enum_to_string_map_impl(std::index_sequence<I...>)
      {
         using key_t = std::underlying_type_t<T>;
         return normal_map<key_t, sv, std::tuple_size_v<meta_t<T>>>({std::make_pair<key_t, sv>(
            static_cast<key_t>(glz::get<1>(glz::get<I>(meta_v<T>))), sv(glz::get<0>(glz::get<I>(meta_v<T>))))...});
      }

      template <class T>
      constexpr auto make_enum_to_string_map()
      {
         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_enum_to_string_map_impl<T>(indices);
      }

      // TODO: This faster approach can be used if the enum has an integer type base and sequential numbering
      template <class T>
      constexpr auto make_enum_to_string_array()
      {
         std::array<sv, std::tuple_size_v<meta_t<T>>> arr;
         for_each<std::tuple_size_v<meta_t<T>>>(
            [&](auto I) { arr[I] = enum_name_v<static_cast<T>(decltype(I)::value)>; });
         return arr;
      }

      template <class T, size_t... I>
      constexpr auto make_string_to_enum_map_impl(std::index_sequence<I...>)
      {
         return normal_map<sv, T, std::tuple_size_v<meta_t<T>>>({std::make_pair<sv, T>(
            sv(glz::get<0>(glz::get<I>(meta_v<T>))), T(glz::get<1>(glz::get<I>(meta_v<T>))))...});
      }

      template <class T>
      constexpr auto make_string_to_enum_map()
      {
         constexpr auto indices = std::make_index_sequence<std::tuple_size_v<meta_t<T>>>{};
         return make_string_to_enum_map_impl<T>(indices);
      }

      template <class T>
      constexpr auto get_combined_keys_from_variant()
      {
         constexpr auto N = std::variant_size_v<T>;
         constexpr size_t max_keys = []() {
            size_t res{};
            for_each<N>([&](auto I) {
               using V = std::decay_t<std::variant_alternative_t<I, T>>;
               if constexpr (reflectable<V>) {
                  res += count_members<V>();
               }
               else {
                  res += std::tuple_size_v<meta_t<V>>;
               }
            });
            return res;
         }();

         std::array<std::string_view, max_keys> data{};
         size_t index = 0;
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            if constexpr (reflectable<V>) {
               constexpr auto members = member_names<V>();
               for_each<std::tuple_size_v<decltype(members)>>([&](auto J) { data[index++] = glz::get<J>(members); });
            }
            else {
               for_each<std::tuple_size_v<meta_t<V>>>([&](auto J) {
                  constexpr auto item = get<J>(meta_v<V>);
                  using T0 = std::decay_t<decltype(get<0>(item))>;
                  constexpr bool use_reflection = std::is_member_object_pointer_v<T0>;
                  auto key_getter = [&] {
                     if constexpr (use_reflection) {
                        return get_name<get<0>(item)>();
                     }
                     else {
                        return get<0>(item);
                     }
                  };
                  data[index++] = key_getter();
               });
            }
         });

         std::sort(data.data(), data.data() + max_keys);
         const auto end = std::unique(data.data(), data.data() + max_keys);
         const auto size = std::distance(data.data(), end);

         return std::pair{data, size};
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
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            if constexpr (reflectable<V>) {
               constexpr auto members = member_names<V>();
               for_each<std::tuple_size_v<decltype(members)>>(
                  [&](auto J) { deduction_map.find(get<J>(members))->second[I] = true; });
            }
            else {
               for_each<std::tuple_size_v<meta_t<V>>>([&](auto J) {
                  constexpr auto item = get<J>(meta_v<V>);
                  using T0 = std::decay_t<decltype(get<0>(item))>;
                  constexpr bool use_reflection = std::is_member_object_pointer_v<T0>;
                  auto key_getter = [&] {
                     if constexpr (use_reflection) {
                        return get_name<get<0>(item)>();
                     }
                     else {
                        return get<0>(item);
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
            return *member_ptr;
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

      template <class T, class = std::make_index_sequence<std::tuple_size<meta_t<T>>::value>>
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

      template <class T, auto Opts>
      constexpr auto required_fields()
      {
         constexpr auto n = std::tuple_size_v<meta_t<T>>;
         bit_array<n> fields{};
         if constexpr (Opts.error_on_missing_keys) {
            for_each<n>([&](auto I) constexpr {
               fields[I] =
                  !bool(Opts.skip_null_members) ||
                  !null_t<std::decay_t<member_t<T, std::tuple_element_t<1, std::tuple_element_t<I, meta_t<T>>>>>>;
            });
         }
         return fields;
      }
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

   constexpr auto array(auto&&... args) { return detail::Array{glz::tuplet::make_copy_tuple(conv_sv(args)...)}; }

   constexpr auto object(auto&&... args)
   {
      if constexpr (sizeof...(args) == 0) {
         return glz::detail::Object{glz::tuplet::tuple{}};
      }
      else {
         return glz::detail::Object{
            group_builder<std::decay_t<decltype(glz::tuplet::make_copy_tuple(conv_sv(args)...))>>::op(
               glz::tuplet::make_copy_tuple(conv_sv(args)...))};
      }
   }

   constexpr auto enumerate(auto&&... args)
   {
      return glz::detail::Enum{
         group_builder<std::decay_t<decltype(glz::tuplet::make_copy_tuple(conv_sv(args)...))>>::op(
            glz::tuplet::make_copy_tuple(conv_sv(args)...))};
   }

   constexpr auto flags(auto&&... args)
   {
      return glz::detail::Flags{
         group_builder<std::decay_t<decltype(glz::tuplet::make_copy_tuple(conv_sv(args)...))>>::op(
            glz::tuplet::make_copy_tuple(conv_sv(args)...))};
   }
}

template <>
struct glz::meta<glz::error_code>
{
   static constexpr sv name = "glz::error_code";
   static constexpr auto value =
      enumerate("none", glz::error_code::none, //
                "no_read_input", glz::error_code::no_read_input, //
                "data_must_be_null_terminated", glz::error_code::data_must_be_null_terminated, //
                "parse_number_failure", glz::error_code::parse_number_failure, //
                "expected_brace", glz::error_code::expected_brace, //
                "expected_bracket", glz::error_code::expected_bracket, //
                "expected_quote", glz::error_code::expected_quote, //
                "exceeded_static_array_size", glz::error_code::exceeded_static_array_size, //
                "unexpected_end", glz::error_code::unexpected_end, //
                "expected_end_comment", glz::error_code::expected_end_comment, //
                "syntax_error", glz::error_code::syntax_error, //
                "key_not_found", glz::error_code::key_not_found, //
                "unexpected_enum", glz::error_code::unexpected_enum, //
                "attempt_const_read", glz::error_code::attempt_const_read, //
                "attempt_member_func_read", glz::error_code::attempt_member_func_read, //
                "attempt_read_hidden", glz::error_code::attempt_read_hidden, //
                "invalid_nullable_read", glz::error_code::invalid_nullable_read, //
                "invalid_variant_object", glz::error_code::invalid_variant_object, //
                "invalid_variant_array", glz::error_code::invalid_variant_array, //
                "invalid_variant_string", glz::error_code::invalid_variant_string, //
                "no_matching_variant_type", glz::error_code::no_matching_variant_type, //
                "expected_true_or_false", glz::error_code::expected_true_or_false, //
                "unknown_key", glz::error_code::unknown_key, //
                "invalid_flag_input", glz::error_code::invalid_flag_input, //
                "invalid_escape", glz::error_code::invalid_escape, //
                "u_requires_hex_digits", glz::error_code::u_requires_hex_digits, //
                "file_extension_not_supported", glz::error_code::file_extension_not_supported, //
                "could_not_determine_extension", glz::error_code::could_not_determine_extension, //
                "seek_failure", glz::error_code::seek_failure, //
                "unicode_escape_conversion_failure", glz::error_code::unicode_escape_conversion_failure, //
                "file_open_failure", glz::error_code::file_open_failure, //
                "file_include_error", glz::error_code::file_include_error, //
                "dump_int_error", glz::error_code::dump_int_error, //
                "get_nonexistent_json_ptr", glz::error_code::get_nonexistent_json_ptr, //
                "get_wrong_type", glz::error_code::get_wrong_type, //
                "cannot_be_referenced", glz::error_code::cannot_be_referenced, //
                "invalid_get", glz::error_code::invalid_get, //
                "invalid_get_fn", glz::error_code::invalid_get_fn, //
                "invalid_call", glz::error_code::invalid_call, //
                "invalid_partial_key", glz::error_code::invalid_partial_key, //
                "name_mismatch", glz::error_code::name_mismatch, //
                "array_element_not_found", glz::error_code::array_element_not_found, //
                "elements_not_convertible_to_design", glz::error_code::elements_not_convertible_to_design, //
                "unknown_distribution", glz::error_code::unknown_distribution, //
                "invalid_distribution_elements", glz::error_code::invalid_distribution_elements, //
                "missing_key", glz::error_code::missing_key //
      );
};

namespace glz
{
   [[nodiscard]] inline std::string format_error(const parse_error& pe, const auto& buffer)
   {
      static constexpr auto arr = detail::make_enum_to_string_array<error_code>();
      const auto error_type_str = arr[static_cast<uint32_t>(pe.ec)];

      const auto info = detail::get_source_info(buffer, pe.location);
      if (info) {
         return detail::generate_error_string(error_type_str, *info);
      }
      return std::string(error_type_str);
   }
}
