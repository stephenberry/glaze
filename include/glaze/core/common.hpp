// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/core/context.hpp"
#include "glaze/core/feature_test.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/util/bit_array.hpp"
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
                               pair_t<range_value_t<T>> && map_subscriptable<std::decay_t<T>>;

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
      concept boolean_like =
         std::same_as<std::remove_cvref_t<T>, bool> || std::same_as<T, std::vector<bool>::reference> ||
         std::same_as<T, std::vector<bool>::const_reference>;

      template <class T>
      concept is_no_reflect = requires(T t) { requires T::glaze_reflect == false; };

      /// \brief check if container has fixed size and its subsequent T::value_type
      template <class T>
      concept has_static_size =
         (is_span<T> && !is_dynamic_span<T>) || (
                                                   requires(T container) {
                                                      {
                                                         std::bool_constant<(std::decay_t<T>{}.size(), true)>()
                                                      } -> std::same_as<std::true_type>;
                                                   } && std::decay_t<T>{}.size() > 0 &&
                                                   requires {
                                                      typename T::value_type;
                                                      requires std::is_trivially_copyable_v<typename T::value_type>;
                                                   });
      static_assert(has_static_size<std::array<int, 2>>);
      static_assert(!has_static_size<std::array<std::string, 2>>);

      template <class T>
      constexpr bool is_std_array = false;
      template <class T, std::size_t N>
      constexpr bool is_std_array<std::array<T, N>> = true;

      template <class T>
      concept has_fixed_size_container = std::is_array_v<T> || is_std_array<T>;
      static_assert(has_fixed_size_container<std::array<std::string, 2>>);
      static_assert(has_fixed_size_container<int[54]>);

      template <class T>
      constexpr size_t get_size() noexcept
      {
         if constexpr (is_span<T>) {
            return T::extent;
         }
         else if constexpr (std::is_array_v<T>) {
            return std::extent_v<T>;
         }
         else if constexpr (is_std_array<T>) {
            return glz::tuple_size_v<T>;
         }
         else {
            return std::decay_t<T>{}.size();
         }
      }

      template <class T>
      concept is_reference_wrapper = is_specialization_v<T, std::reference_wrapper>;

      template <class T>
      concept tuple_t = requires(T t) {
         glz::tuple_size<T>::value;
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
         glaze_t<T> && !(glaze_array_t<T> || glaze_object_t<T> || glaze_enum_t<T> || meta_keys<T> || glaze_flags_t<T>);

      template <class T>
      concept reflectable = std::is_aggregate_v<std::remove_cvref_t<T>> && std::is_class_v<std::remove_cvref_t<T>> &&
                            !(is_no_reflect<T> || glaze_value_t<T> || glaze_object_t<T> || glaze_array_t<T> ||
                              glaze_flags_t<T> || range<T> || pair_t<T> || null_t<T>);

      template <class T>
      concept is_memory_object = is_memory_type<T> && (glaze_object_t<memory_type<T>> || reflectable<memory_type<T>>);

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

      template <is_variant T, size_t... I>
      constexpr auto make_variant_id_map_impl(std::index_sequence<I...>, auto&& variant_ids)
      {
         return normal_map<sv, size_t, std::variant_size_v<T>>(std::array{pair<sv, size_t>{sv(variant_ids[I]), I}...});
      }

      template <is_variant T>
      constexpr auto make_variant_id_map()
      {
         constexpr auto indices = std::make_index_sequence<std::variant_size_v<T>>{};

         return make_variant_id_map_impl<T>(indices, ids_v<T>);
      }

      template <class Value, class Element>
      inline decltype(auto) get_member(Value&& value, Element&& element) noexcept
      {
         using V = std::decay_t<decltype(element)>;
         if constexpr (std::is_member_object_pointer_v<V>) {
            return value.*element;
         }
         else if constexpr (std::is_member_function_pointer_v<V>) {
            return element;
         }
         else if constexpr (std::invocable<Element, Value>) {
            return std::invoke(std::forward<Element>(element), std::forward<Value>(value));
         }
         else if constexpr (std::is_pointer_v<V>) {
            if constexpr (std::invocable<decltype(*element), Value>) {
               return std::invoke(*element, std::forward<Value>(value));
            }
            else {
               return *element;
            }
         }
         else {
            return element;
         }
      }

      template <class T, class Element>
      using member_t = decltype(get_member(std::declval<std::add_lvalue_reference_t<T>>(), std::declval<Element>()));

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

      // Output variants in the following format  ["variant_type", variant_json_data] with
      // glz::detail:array_variant(&T::var);
      template <is_variant T>
      struct array_variant_wrapper final
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

   template <class... Args>
   constexpr auto object(Args&&... args) noexcept
   {
      return detail::Object{tuplet::tuple{std::forward<Args>(args)...}};
   }

   constexpr auto enumerate(auto&&... args) noexcept { return detail::Enum{tuplet::tuple{args...}}; }

   constexpr auto flags(auto&&... args) noexcept { return detail::Flags{tuplet::tuple{args...}}; }
}

namespace glz
{
   // This wraps glz::expected error (unexpected) values in an object with an "error" key
   // This makes them discernable from the expected value
   template <class T>
   struct unexpected_wrapper final
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

template <>
struct glz::meta<glz::error_code>
{
   static constexpr sv name = "glz::error_code";
   using enum glz::error_code;
   static constexpr std::array keys{"none",
                                    "end_reached",
                                    "no_read_input",
                                    "data_must_be_null_terminated",
                                    "parse_number_failure",
                                    "expected_brace",
                                    "expected_bracket",
                                    "expected_quote",
                                    "expected_comma",
                                    "expected_colon",
                                    "exceeded_static_array_size",
                                    "exceeded_max_recursive_depth",
                                    "unexpected_end",
                                    "expected_end_comment",
                                    "syntax_error",
                                    "unexpected_enum",
                                    "attempt_const_read",
                                    "attempt_member_func_read",
                                    "attempt_read_hidden",
                                    "invalid_nullable_read",
                                    "invalid_variant_object",
                                    "invalid_variant_array",
                                    "invalid_variant_string",
                                    "no_matching_variant_type",
                                    "expected_true_or_false",
                                    "key_not_found",
                                    "unknown_key",
                                    "missing_key",
                                    "invalid_flag_input",
                                    "invalid_escape",
                                    "u_requires_hex_digits",
                                    "unicode_escape_conversion_failure",
                                    "dump_int_error",
                                    "file_open_failure",
                                    "file_close_failure",
                                    "file_include_error",
                                    "file_extension_not_supported",
                                    "could_not_determine_extension",
                                    "get_nonexistent_json_ptr",
                                    "get_wrong_type",
                                    "seek_failure",
                                    "cannot_be_referenced",
                                    "invalid_get",
                                    "invalid_get_fn",
                                    "invalid_call",
                                    "invalid_partial_key",
                                    "name_mismatch",
                                    "array_element_not_found",
                                    "elements_not_convertible_to_design",
                                    "unknown_distribution",
                                    "invalid_distribution_elements",
                                    "hostname_failure",
                                    "includer_error"};
   static constexpr auto value = std::array{none, //
                                            end_reached, // A non-error code for non-null terminated input buffers
                                            no_read_input, //
                                            data_must_be_null_terminated, //
                                            parse_number_failure, //
                                            expected_brace, //
                                            expected_bracket, //
                                            expected_quote, //
                                            expected_comma, //
                                            expected_colon, //
                                            exceeded_static_array_size, //
                                            exceeded_max_recursive_depth, //
                                            unexpected_end, //
                                            expected_end_comment, //
                                            syntax_error, //
                                            unexpected_enum, //
                                            attempt_const_read, //
                                            attempt_member_func_read, //
                                            attempt_read_hidden, //
                                            invalid_nullable_read, //
                                            invalid_variant_object, //
                                            invalid_variant_array, //
                                            invalid_variant_string, //
                                            no_matching_variant_type, //
                                            expected_true_or_false, //
                                            // Key errors
                                            key_not_found, //
                                            unknown_key, //
                                            missing_key, //
                                            // Other errors
                                            invalid_flag_input, //
                                            invalid_escape, //
                                            u_requires_hex_digits, //
                                            unicode_escape_conversion_failure, //
                                            dump_int_error, //
                                            // File errors
                                            file_open_failure, //
                                            file_close_failure, //
                                            file_include_error, //
                                            file_extension_not_supported, //
                                            could_not_determine_extension, //
                                            // JSON pointer access errors
                                            get_nonexistent_json_ptr, //
                                            get_wrong_type, //
                                            seek_failure, //
                                            // Other errors
                                            cannot_be_referenced, //
                                            invalid_get, //
                                            invalid_get_fn, //
                                            invalid_call, //
                                            invalid_partial_key, //
                                            name_mismatch, //
                                            array_element_not_found, //
                                            elements_not_convertible_to_design, //
                                            unknown_distribution, //
                                            invalid_distribution_elements, //
                                            hostname_failure, //
                                            includer_error};
};
