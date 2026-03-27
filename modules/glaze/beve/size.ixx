// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.beve.size;

import std;

import glaze.beve.header;
import glaze.beve.key_traits;

import glaze.core.common;
import glaze.core.opts;
import glaze.core.reflect;

import glaze.concepts.container_concepts;

import glaze.tuplet;

import glaze.util.for_each;
import glaze.util.variant;
import glaze.util.tuple;
import glaze.util.string_literal;

#include "glaze/util/inline.hpp"

namespace glz
{
   // Calculate the number of bytes needed to store a compressed integer
   export [[nodiscard]] GLZ_ALWAYS_INLINE constexpr std::size_t compressed_int_size(std::uint64_t i) noexcept
   {
      if (i < 64) return 1;
      if (i < 16384) return 2;
      if (i < 1073741824) return 4;
      return 8;
   }

   // Compile-time version for known values
   export template <std::uint64_t i>
   [[nodiscard]] consteval std::size_t compressed_int_size() noexcept
   {
      if constexpr (i < 64)
         return 1;
      else if constexpr (i < 16384)
         return 2;
      else if constexpr (i < 1073741824)
         return 4;
      else
         return 8;
   }

   // Forward declaration for the size calculation template
   export template <std::uint32_t Format, class T>
   struct calculate_size;

   // Primary template for BEVE size calculation dispatch
   template <>
   struct calculate_size<BEVE, void>
   {
      template <auto Opts, class T>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(T&& value)
      {
         return calculate_size<BEVE, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value));
      }

      template <auto Opts, class T>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t no_header(T&& value)
      {
         return calculate_size<BEVE, std::remove_cvref_t<T>>::template no_header<Opts>(std::forward<T>(value));
      }
   };

   template <class T>
      requires(glaze_value_t<T> && !custom_write<T>)
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         using V = std::remove_cvref_t<decltype(get_member(std::declval<decltype(value)>(), meta_wrapper_v<T>))>;
         return calculate_size<BEVE, V>::template op<Opts>(get_member(value, meta_wrapper_v<T>));
      }

      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t no_header(auto&& value)
      {
         using V = std::remove_cvref_t<decltype(get_member(std::declval<decltype(value)>(), meta_wrapper_v<T>))>;
         return calculate_size<BEVE, V>::template no_header<Opts>(get_member(value, meta_wrapper_v<T>));
      }
   };

   template <always_null_t T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&&) noexcept
      {
         return 1; // null tag
      }
   };

   template <is_bitset T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&& value) noexcept
      {
         const auto num_bytes = (value.size() + 7) / 8;
         return 1 + compressed_int_size(value.size()) + num_bytes; // tag + size + data
      }
   };

   template <glaze_flags_t T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&&) noexcept
      {
         static constexpr auto data_size = byte_length<T>();
         return data_size; // flags are written directly without header
      }
   };

   template <is_member_function_pointer T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&&) noexcept
      {
         return 0; // member function pointers are not serialized
      }
   };

   template <is_includer T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&&) noexcept
      {
         return 1 + 1; // tag + compressed_int(0) for empty string
      }
   };

   template <boolean_like T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&&) noexcept
      {
         return 1; // bool tag contains the value
      }
   };

   template <func_t T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         return calculate_size<BEVE, void>::template op<Opts>(name_v<std::decay_t<decltype(value)>>);
      }
   };

   template <class T>
   struct calculate_size<BEVE, basic_raw_json<T>>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         return calculate_size<BEVE, void>::template op<Opts>(value.str);
      }
   };

   template <class T>
   struct calculate_size<BEVE, basic_text<T>>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         return calculate_size<BEVE, void>::template op<Opts>(value.str);
      }
   };

   template <is_variant T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         return std::visit(
            [&](auto&& v) -> std::size_t {
               using V = std::decay_t<decltype(v)>;
               using Variant = std::decay_t<decltype(value)>;
               static constexpr std::uint64_t index = variant_index_v<V, Variant>;

               // 1 byte tag + compressed index + value size
               return 1 + compressed_int_size<index>() + calculate_size<BEVE, void>::template op<Opts>(v);
            },
            value);
      }
   };

   template <class T>
      requires num_t<T> || char_t<T> || glaze_enum_t<T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&&) noexcept
      {
         return 1 + sizeof(T); // tag + value
      }

      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t no_header(auto&&) noexcept
      {
         return sizeof(T); // value only
      }
   };

   template <class T>
      requires(std::is_enum_v<T> && !glaze_enum_t<T>)
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&&) noexcept
      {
         using V = std::underlying_type_t<std::decay_t<T>>;
         return 1 + sizeof(V); // tag + value
      }

      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t no_header(auto&&) noexcept
      {
         using V = std::underlying_type_t<std::decay_t<T>>;
         return sizeof(V); // value only
      }
   };

   template <complex_t T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&&) noexcept
      {
         using V = typename T::value_type;
         return 2 + 2 * sizeof(V); // extension tag + complex header + real + imag
      }

      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t no_header(auto&&) noexcept
      {
         using V = typename T::value_type;
         return 2 * sizeof(V); // real + imag
      }
   };

   template <str_t T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         const sv str = [&]() -> const sv {
            if constexpr (!char_array_t<T> && std::is_pointer_v<std::decay_t<T>>) {
               return value ? value : "";
            }
            else {
               return sv{value};
            }
         }();

         const auto n = str.size();
         return 1 + compressed_int_size(n) + n; // tag + length + data
      }

      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t no_header(auto&& value)
      {
         const auto n = value.size();
         return compressed_int_size(n) + n; // length + data
      }

      template <std::uint64_t N, auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static constexpr std::size_t no_header_cx() noexcept
      {
         return compressed_int_size<N>() + N; // length + data
      }
   };

   template <writable_array_t T>
   struct calculate_size<BEVE, T>
   {
      static constexpr bool map_like_array = pair_t<range_value_t<T>>;

      template <auto Opts>
         requires(map_like_array ? check_concatenate(Opts) == false : true)
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         using V = range_value_t<std::decay_t<T>>;

         std::size_t result = 1; // tag byte
         result += compressed_int_size(value.size()); // element count

         if constexpr (boolean_like<V>) {
            const auto num_bytes = (value.size() + 7) / 8;
            result += num_bytes;
         }
         else if constexpr (num_t<V>) {
            result += value.size() * sizeof(V);
         }
         else if constexpr (str_t<V>) {
            for (auto& x : value) {
               result += compressed_int_size(x.size()) + x.size();
            }
         }
         else if constexpr (complex_t<V>) {
            // extension tag + complex header + count + data
            using X = typename V::value_type;
            result += 1; // complex_header byte
            result += value.size() * 2 * sizeof(X);
         }
         else {
            // generic array - need to sum element sizes
            for (auto&& x : value) {
               result += calculate_size<BEVE, void>::template op<Opts>(x);
            }
         }

         return result;
      }

      template <auto Opts>
         requires(map_like_array && check_concatenate(Opts) == true)
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         std::size_t result = 1; // tag byte
         result += compressed_int_size(value.size()); // element count

         for (auto&& [k, v] : value) {
            result += calculate_size<BEVE, void>::template no_header<Opts>(k);
            result += calculate_size<BEVE, void>::template op<Opts>(v);
         }

         return result;
      }
   };

   template <pair_t T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         std::size_t result = 1; // tag byte
         result += compressed_int_size<1>(); // count = 1

         const auto& [k, v] = value;
         result += calculate_size<BEVE, void>::template no_header<Opts>(k);
         result += calculate_size<BEVE, void>::template op<Opts>(v);

         return result;
      }
   };

   template <writable_map_t T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         std::size_t result = 1; // tag byte
         result += compressed_int_size(value.size()); // element count

         for (auto&& [k, v] : value) {
            result += calculate_size<BEVE, void>::template no_header<Opts>(k);
            result += calculate_size<BEVE, void>::template op<Opts>(v);
         }

         return result;
      }
   };

   template <nullable_t T>
      requires(std::is_array_v<T>)
   struct calculate_size<BEVE, T>
   {
      template <auto Opts, class V, std::size_t N>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(const V (&value)[N])
      {
         return calculate_size<BEVE, void>::template op<Opts>(std::span{value, N});
      }
   };

   template <nullable_t T>
      requires(!std::is_array_v<T>)
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         if (value) {
            return calculate_size<BEVE, void>::template op<Opts>(*value);
         }
         else {
            return 1; // null tag
         }
      }
   };

   template <class T>
      requires(nullable_value_t<T> && not nullable_like<T> && not is_expected<T>)
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE static std::size_t op(auto&& value)
      {
         if (value.has_value()) {
            return calculate_size<BEVE, void>::template op<Opts>(value.value());
         }
         else {
            return 1; // null tag
         }
      }
   };

   template <class T>
      requires is_specialization_v<T, glz::obj> || is_specialization_v<T, glz::obj_copy>
   struct calculate_size<BEVE, T>
   {
      template <auto Options>
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V> / 2;

         std::size_t result = 0;

         if constexpr (!check_opening_handled(Options)) {
            result += 1; // object tag
            result += compressed_int_size<N>(); // member count
         }

         for_each<N>([&]<std::size_t I>() {
            constexpr auto Opts = opening_handled_off<Options>();
            result += calculate_size<BEVE, void>::template no_header<Opts>(get<2 * I>(value.value)); // key
            result += calculate_size<BEVE, void>::template op<Opts>(get<2 * I + 1>(value.value)); // value
         });

         return result;
      }
   };

   template <class T>
      requires is_specialization_v<T, glz::merge>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts, class Value, std::size_t I>
      static consteval bool should_skip_field()
      {
         using V = field_t<Value, I>;

         if constexpr (std::same_as<V, hidden> || std::same_as<V, skip>) {
            return true;
         }
         else if constexpr (is_member_function_pointer<V>) {
            return !check_write_function_pointers(Opts);
         }
         else {
            return false;
         }
      }

      template <auto Opts, class Value>
      static consteval std::size_t count_fields_for_type()
      {
         constexpr auto N = reflect<Value>::size;
         return []<std::size_t... I>(std::index_sequence<I...>) {
            return (std::size_t{} + ... + (should_skip_field<Opts, Value, I>() ? std::size_t{} : std::size_t{1}));
         }(std::make_index_sequence<N>{});
      }

      template <auto Opts>
      static consteval std::size_t merge_element_count()
      {
         std::size_t count{};
         using Tuple = std::decay_t<decltype(std::declval<T>().value)>;
         for_each<glz::tuple_size_v<Tuple>>([&]<auto I>() constexpr {
            using Value = std::decay_t<glz::tuple_element_t<I, Tuple>>;
            if constexpr (is_specialization_v<Value, glz::obj> || is_specialization_v<Value, glz::obj_copy>) {
               count += glz::tuple_size_v<decltype(std::declval<Value>().value)> / 2;
            }
            else {
               count += count_fields_for_type<Opts, Value>();
            }
         });
         return count;
      }

      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V>;

         std::size_t result = 1; // object tag
         result += compressed_int_size<merge_element_count<Opts>()>(); // member count

         [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((result += calculate_size<BEVE, void>::template op<opening_handled<Opts>()>(glz::get<I>(value.value))),
             ...);
         }(std::make_index_sequence<N>{});

         return result;
      }
   };

   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && not custom_write<T>)
   struct calculate_size<BEVE, T>
   {
      static constexpr auto N = reflect<T>::size;

      template <auto Opts, std::size_t I>
      static consteval bool should_skip_field()
      {
         using V = field_t<T, I>;

         if constexpr (std::same_as<V, hidden> || std::same_as<V, skip>) {
            return true;
         }
         else if constexpr (is_member_function_pointer<V>) {
            return !check_write_function_pointers(Opts);
         }
         else {
            return false;
         }
      }

      template <auto Opts>
      static consteval std::size_t count_to_write()
      {
         return []<std::size_t... I>(std::index_sequence<I...>) {
            return (std::size_t{} + ... + (should_skip_field<Opts, I>() ? std::size_t{} : std::size_t{1}));
         }(std::make_index_sequence<N>{});
      }

      template <auto Opts>
         requires(check_structs_as_arrays(Opts) == true)
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         std::size_t result = 1; // generic_array tag
         result += compressed_int_size<count_to_write<Opts>()>(); // element count

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         for_each<N>([&]<std::size_t I>() {
            if constexpr (should_skip_field<Opts, I>()) {
               return;
            }
            else {
               if constexpr (reflectable<T>) {
                  result += calculate_size<BEVE, void>::template op<Opts>(get_member(value, get<I>(t)));
               }
               else {
                  result +=
                     calculate_size<BEVE, void>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)));
               }
            }
         });

         return result;
      }

      template <auto Options>
         requires(check_structs_as_arrays(Options) == false)
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         constexpr auto Opts = opening_handled_off<Options>();

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         std::size_t result = 0;

         if constexpr (maybe_skipped<Options, T>) {
            // Dynamic path: count members at runtime to handle skip_null_members
            std::size_t member_count = 0;

            // First pass: count members that will be written
            for_each<N>([&]<std::size_t I>() {
               if constexpr (should_skip_field<Options, I>()) {
                  return;
               }
               else {
                  using val_t = field_t<T, I>;

                  if constexpr (null_t<val_t> && Options.skip_null_members) {
                     if constexpr (always_null_t<val_t>) {
                        return;
                     }
                     else {
                        const auto is_null = [&]() {
                           decltype(auto) element = [&]() -> decltype(auto) {
                              if constexpr (reflectable<T>) {
                                 return get<I>(t);
                              }
                              else {
                                 return get<I>(reflect<T>::values);
                              }
                           };

                           if constexpr (nullable_wrapper<val_t>) {
                              return !bool(element()(value).val);
                           }
                           else if constexpr (nullable_value_t<val_t>) {
                              return !get_member(value, element()).has_value();
                           }
                           else {
                              return !bool(get_member(value, element()));
                           }
                        }();
                        if (!is_null) {
                           ++member_count;
                        }
                     }
                  }
                  else {
                     ++member_count;
                  }
               }
            });

            // Write header with dynamic count
            if constexpr (!check_opening_handled(Options)) {
               result += 1; // object tag
               result += compressed_int_size(member_count); // member count
            }

            // Second pass: calculate member sizes
            for_each<N>([&]<std::size_t I>() {
               if constexpr (should_skip_field<Options, I>()) {
                  return;
               }
               else {
                  using val_t = field_t<T, I>;

                  if constexpr (null_t<val_t> && Options.skip_null_members) {
                     if constexpr (always_null_t<val_t>) {
                        return;
                     }
                     else {
                        const auto is_null = [&]() {
                           decltype(auto) element = [&]() -> decltype(auto) {
                              if constexpr (reflectable<T>) {
                                 return get<I>(t);
                              }
                              else {
                                 return get<I>(reflect<T>::values);
                              }
                           };

                           if constexpr (nullable_wrapper<val_t>) {
                              return !bool(element()(value).val);
                           }
                           else if constexpr (nullable_value_t<val_t>) {
                              return !get_member(value, element()).has_value();
                           }
                           else {
                              return !bool(get_member(value, element()));
                           }
                        }();
                        if (is_null) {
                           return;
                        }
                     }
                  }

                  static constexpr sv key = reflect<T>::keys[I];
                  result += calculate_size<BEVE, std::remove_cvref_t<decltype(key)>>::template no_header_cx<key.size(),
                                                                                                            Opts>();

                  decltype(auto) member = [&]() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return get<I>(t);
                     }
                     else {
                        return get<I>(reflect<T>::values);
                     }
                  }();

                  result += calculate_size<BEVE, void>::template op<Opts>(get_member(value, member));
               }
            });
         }
         else {
            // Static path: use compile-time count for better performance
            if constexpr (!check_opening_handled(Options)) {
               result += 1; // object tag
               result += compressed_int_size<count_to_write<Options>()>(); // member count
            }

            for_each<N>([&]<std::size_t I>() {
               if constexpr (should_skip_field<Options, I>()) {
                  return;
               }
               else {
                  static constexpr sv key = reflect<T>::keys[I];
                  result += calculate_size<BEVE, std::remove_cvref_t<decltype(key)>>::template no_header_cx<key.size(),
                                                                                                            Opts>();

                  decltype(auto) member = [&]() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return get<I>(t);
                     }
                     else {
                        return get<I>(reflect<T>::values);
                     }
                  }();

                  result += calculate_size<BEVE, void>::template op<Opts>(get_member(value, member));
               }
            });
         }

         return result;
      }
   };

   template <class T>
      requires glaze_array_t<T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         static constexpr auto N = reflect<T>::size;

         std::size_t result = 1; // generic_array tag
         result += compressed_int_size<N>(); // element count

         for_each<N>([&]<std::size_t I>() {
            result += calculate_size<BEVE, void>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)));
         });

         return result;
      }
   };

   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         static constexpr auto N = glz::tuple_size_v<T>;

         std::size_t result = 1; // generic_array tag
         result += compressed_int_size<N>(); // element count

         if constexpr (is_std_tuple<T>) {
            [&]<std::size_t... I>(std::index_sequence<I...>) {
               ((result += calculate_size<BEVE, void>::template op<Opts>(std::get<I>(value))), ...);
            }(std::make_index_sequence<N>{});
         }
         else {
            [&]<std::size_t... I>(std::index_sequence<I...>) {
               ((result += calculate_size<BEVE, void>::template op<Opts>(glz::get<I>(value))), ...);
            }(std::make_index_sequence<N>{});
         }

         return result;
      }
   };

   // Filesystem path support
   template <filesystem_path T>
   struct calculate_size<BEVE, T>
   {
      template <auto Opts>
      [[nodiscard]] static std::size_t op(auto&& value)
      {
         return calculate_size<BEVE, decltype(value.string())>::template op<Opts>(value.string());
      }
   };

   // ============ Public API ============

   // Calculate the size in bytes needed to serialize a value to BEVE format
   export template <auto Opts = opts{}, class T>
   [[nodiscard]] std::size_t beve_size(T&& value)
   {
      return calculate_size<BEVE, std::remove_cvref_t<T>>::template op<set_beve<Opts>()>(std::forward<T>(value));
   }

   // Calculate size for untagged BEVE (structs_as_arrays = true)
   export template <auto Opts = opts{}, class T>
   [[nodiscard]] std::size_t beve_size_untagged(T&& value)
   {
      return calculate_size<BEVE, std::remove_cvref_t<T>>::template op<
         opt_true<set_beve<Opts>(), structs_as_arrays_opt_tag{}>>(std::forward<T>(value));
   }
}
