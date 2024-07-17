// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

#include "glaze/binary/header.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/refl.hpp"
#include "glaze/core/seek.hpp"
#include "glaze/core/write.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   namespace detail
   {
      GLZ_ALWAYS_INLINE void dump_type(auto&& value, auto&& b, auto&& ix) noexcept
      {
         using V = std::decay_t<decltype(value)>;
         constexpr auto n = sizeof(V);
         if (ix + n > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, ix + n));
         }

         constexpr auto is_volatile = std::is_volatile_v<std::remove_reference_t<decltype(value)>>;

         if constexpr (is_volatile) {
            const V temp = value;
            std::memcpy(b.data() + ix, &temp, n);
         }
         else {
            std::memcpy(b.data() + ix, &value, n);
         }
         ix += n;
      }

      template <uint64_t i, class... Args>
      GLZ_ALWAYS_INLINE void dump_compressed_int(Args&&... args) noexcept
      {
         if constexpr (i < 64) {
            const uint8_t c = uint8_t(i) << 2;
            dump_type(c, args...);
         }
         else if constexpr (i < 16384) {
            const uint16_t c = uint16_t(1) | (uint16_t(i) << 2);
            dump_type(c, args...);
         }
         else if constexpr (i < 1073741824) {
            const uint32_t c = uint32_t(2) | (uint32_t(i) << 2);
            dump_type(c, args...);
         }
         else if constexpr (i < 4611686018427387904) {
            const uint64_t c = uint64_t(3) | (uint64_t(i) << 2);
            dump_type(c, args...);
         }
         else {
            static_assert(i >= 4611686018427387904, "size not supported");
         }
      }

      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE void dump_compressed_int(uint64_t i, Args&&... args) noexcept
      {
         if (i < 64) {
            const uint8_t c = uint8_t(i) << 2;
            dump_type(c, args...);
         }
         else if (i < 16384) {
            const uint16_t c = uint16_t(1) | (uint16_t(i) << 2);
            dump_type(c, args...);
         }
         else if (i < 1073741824) {
            const uint32_t c = uint32_t(2) | (uint32_t(i) << 2);
            dump_type(c, args...);
         }
         else if (i < 4611686018427387904) {
            const uint64_t c = uint64_t(3) | (uint64_t(i) << 2);
            dump_type(c, args...);
         }
         else {
            std::abort(); // this should never happen because we should never allocate containers of this size
         }
      }

      template <>
      struct write<binary>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            to_binary<std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                 std::forward<B>(b), std::forward<IX>(ix));
         }

         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void no_header(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            to_binary<std::remove_cvref_t<T>>::template no_header<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                        std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <class T>
         requires(glaze_value_t<T> && !custom_write<T>)
      struct to_binary<T>
      {
         template <auto Opts, class Value, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            using V = std::remove_cvref_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
            to_binary<V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                            std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <always_null_t T>
      struct to_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&... args) noexcept
         {
            dump_type(uint8_t{0}, args...);
         }
      };

      template <>
      struct to_binary<hidden>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&, auto&&...) noexcept
         {
            static_assert(false_v<decltype(value)>, "hidden type should not be written");
         }
      };

      template <is_bitset T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&&... args) noexcept
         {
            constexpr uint8_t type = uint8_t(3) << 3;
            constexpr uint8_t tag = tag::typed_array | type;
            dump_type(tag, args...);
            dump_compressed_int<Opts>(value.size(), args...);

            // constexpr auto num_bytes = (value.size() + 7) / 8;
            const auto num_bytes = (value.size() + 7) / 8;
            // .size() should be constexpr, but clang doesn't support this
            std::vector<uint8_t> bytes(num_bytes);
            // std::array<uint8_t, num_bytes> bytes{};
            for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i) {
               for (size_t bit_i = 0; bit_i < 8 && i < value.size(); ++bit_i, ++i) {
                  bytes[byte_i] |= uint8_t(value[i]) << uint8_t(bit_i);
               }
            }
            dump(bytes, args...);
         }
      };

      template <glaze_flags_t T>
      struct to_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix)
         {
            static constexpr auto N = refl<T>.N;

            std::array<uint8_t, byte_length<T>()> data{};

            for_each_flatten<N>([&](auto I) {
               data[I / 8] |= static_cast<uint8_t>(get_member(value, get<I>(refl<T>.values))) << (7 - (I % 8));
            });

            dump(data, b, ix);
         }
      };

      template <is_member_function_pointer T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&...) noexcept
         {}
      };

      // write includers as empty strings
      template <is_includer T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&... args) noexcept
         {
            constexpr uint8_t tag = tag::string;

            dump_type(tag, args...);
            dump_compressed_int<Opts>(0, args...);
         }
      };

      template <boolean_like T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&&, Args&&... args) noexcept
         {
            dump_type(value ? tag::bool_true : tag::bool_false, args...);
         }
      };

      template <func_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            write<binary>::op<Opts>(name_v<std::decay_t<decltype(value)>>, ctx, args...);
         }
      };

      template <class T>
      struct to_binary<basic_raw_json<T>> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            write<binary>::op<Opts>(value.str, ctx, std::forward<Args>(args)...);
         }
      };

      template <class T>
      struct to_binary<basic_text<T>> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            write<binary>::op<Opts>(value.str, ctx, std::forward<Args>(args)...);
         }
      };

      template <class T, class V>
      constexpr size_t variant_index_v = []<size_t... I>(std::index_sequence<I...>) {
         return ((std::is_same_v<T, std::variant_alternative_t<I, V>> * I) + ...);
      }(std::make_index_sequence<std::variant_size_v<V>>{});

      template <is_variant T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using Variant = std::decay_t<decltype(value)>;

            std::visit(
               [&](auto&& v) {
                  using V = std::decay_t<decltype(v)>;

                  static constexpr uint64_t index = variant_index_v<V, Variant>;

                  constexpr uint8_t tag = tag::extensions | 0b00001'000;

                  dump_type(tag, args...);
                  dump_compressed_int<index>(args...);
                  write<binary>::op<Opts>(v, ctx, args...);
               },
               value);
         }
      };

      template <class T>
         requires num_t<T> || char_t<T> || glaze_enum_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args) noexcept
         {
            constexpr uint8_t type = std::floating_point<T> ? 0 : (std::is_signed_v<T> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t tag = tag::number | type | (byte_count<T> << 5);
            dump_type(tag, args...);
            dump_type(value, args...);
         }

         template <auto Opts>
         GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&&, auto&&... args) noexcept
         {
            dump_type(value, args...);
         }
      };

      template <class T>
         requires(std::is_enum_v<T> && !glaze_enum_t<T>)
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args) noexcept
         {
            using V = std::underlying_type_t<std::decay_t<T>>;

            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t tag = tag::number | type | (byte_count<V> << 5);
            dump_type(tag, args...);
            dump_type(value, args...);
         }

         template <auto Opts>
         GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&&, auto&&... args) noexcept
         {
            dump_type(value, args...);
         }
      };

      template <class T>
         requires complex_t<T>
      struct to_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&&... args) noexcept
         {
            constexpr uint8_t tag = tag::extensions | 0b00011'000;
            dump_type(tag, args...);

            using V = typename T::value_type;
            constexpr uint8_t complex_number = 0;
            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t complex_header = complex_number | type | (byte_count<V> << 5);
            dump_type(complex_header, args...);
            dump_type(value.real(), args...);
            dump_type(value.imag(), args...);
         }

         template <auto Opts>
         GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&&, auto&&... args) noexcept
         {
            dump_type(value.real(), args...);
            dump_type(value.imag(), args...);
         }
      };

      template <str_t T>
      struct to_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            const sv str = [&]() -> const sv {
               if constexpr (!char_array_t<T> && std::is_pointer_v<std::decay_t<T>>) {
                  return value ? value : "";
               }
               else {
                  return value;
               }
            }();

            constexpr uint8_t tag = tag::string;

            dump_type(tag, b, ix);
            const auto n = str.size();
            dump_compressed_int<Opts>(n, b, ix);

            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }

            std::memcpy(b.data() + ix, str.data(), n);
            ix += n;
         }

         template <auto Opts>
         GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            dump_compressed_int<Opts>(value.size(), b, ix);

            const auto n = value.size();
            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }

            std::memcpy(b.data() + ix, value.data(), n);
            ix += n;
         }
      };

      template <writable_array_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using V = range_value_t<std::decay_t<T>>;

            if constexpr (boolean_like<V>) {
               constexpr uint8_t type = uint8_t(3) << 3;
               constexpr uint8_t tag = tag::typed_array | type;
               dump_type(tag, args...);
               dump_compressed_int<Opts>(value.size(), args...);

               // booleans must be dumped using single bits
               if constexpr (has_static_size<T>) {
                  constexpr auto num_bytes = (value.size() + 7) / 8;
                  std::array<uint8_t, num_bytes> bytes{};
                  for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i) {
                     for (size_t bit_i = 7; bit_i < 8 && i < value.size(); --bit_i, ++i) {
                        bytes[byte_i] |= uint8_t(value[i]) << uint8_t(bit_i);
                     }
                  }
                  dump(bytes, args...);
               }
               else if constexpr (accessible<T>) {
                  const auto num_bytes = (value.size() + 7) / 8;
                  for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i) {
                     uint8_t byte{};
                     for (size_t bit_i = 7; bit_i < 8 && i < value.size(); --bit_i, ++i) {
                        byte |= uint8_t(value[i]) << uint8_t(bit_i);
                     }
                     dump_type(byte, args...);
                  }
               }
               else {
                  static_assert(false_v<T>);
               }
            }
            else if constexpr (num_t<V>) {
               constexpr uint8_t type =
                  std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t tag = tag::typed_array | type | (byte_count<V> << 5);
               dump_type(tag, args...);
               dump_compressed_int<Opts>(value.size(), args...);

               if constexpr (contiguous<T>) {
                  constexpr auto is_volatile =
                     std::is_volatile_v<std::remove_reference_t<std::remove_pointer_t<decltype(value.data())>>>;

                  auto dump_array = [&](auto&& b, auto&& ix) {
                     const auto n = value.size() * sizeof(V);
                     if (ix + n > b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, ix + n));
                     }

                     if constexpr (is_volatile) {
                        V temp;
                        const auto n_elements = value.size();
                        for (size_t i = 0; i < n_elements; ++i) {
                           temp = value[i];
                           std::memcpy(b.data() + ix, &temp, sizeof(V));
                           ix += sizeof(V);
                        }
                     }
                     else {
                        std::memcpy(b.data() + ix, value.data(), n);
                        ix += n;
                     }
                  };

                  dump_array(args...);
               }
               else {
                  for (auto& x : value) {
                     dump_type(x, args...);
                  }
               }
            }
            else if constexpr (str_t<V>) {
               constexpr uint8_t type = uint8_t(3) << 3;
               constexpr uint8_t string_indicator = uint8_t(1) << 5;
               constexpr uint8_t tag = tag::typed_array | type | string_indicator;
               dump_type(tag, args...);
               dump_compressed_int<Opts>(value.size(), args...);

               for (auto& x : value) {
                  dump_compressed_int<Opts>(x.size(), args...);

                  auto dump_array = [&](auto&& b, auto&& ix) {
                     const auto n = x.size();
                     if (ix + n > b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, ix + n));
                     }

                     std::memcpy(b.data() + ix, x.data(), n);
                     ix += n;
                  };

                  dump_array(args...);
               }
            }
            else if constexpr (complex_t<V>) {
               constexpr uint8_t tag = tag::extensions | 0b00011'000;
               dump_type(tag, args...);

               using X = typename V::value_type;
               constexpr uint8_t complex_array = 1;
               constexpr uint8_t type =
                  std::floating_point<X> ? 0 : (std::is_signed_v<X> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t complex_header = complex_array | type | (byte_count<X> << 5);
               dump_type(complex_header, args...);

               dump_compressed_int<Opts>(value.size(), args...);

               for (auto&& x : value) {
                  write<binary>::no_header<Opts>(x, ctx, args...);
               }
            }
            else {
               constexpr uint8_t tag = tag::generic_array;
               dump_type(tag, args...);
               dump_compressed_int<Opts>(value.size(), args...);

               for (auto&& x : value) {
                  write<binary>::op<Opts>(x, ctx, args...);
               }
            }
         }
      };

      template <pair_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static auto op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using Key = typename T::first_type;

            constexpr uint8_t type = str_t<Key> ? 0 : (std::is_signed_v<Key> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t byte_cnt = str_t<Key> ? 0 : byte_count<Key>;
            constexpr uint8_t tag = tag::object | type | (byte_cnt << 5);
            dump_type(tag, args...);

            dump_compressed_int<Opts>(1, args...);
            const auto& [k, v] = value;
            write<binary>::no_header<Opts>(k, ctx, args...);
            write<binary>::op<Opts>(v, ctx, args...);
         }
      };

      template <writable_map_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using Key = typename T::key_type;

            constexpr uint8_t type = str_t<Key> ? 0 : (std::is_signed_v<Key> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t byte_cnt = str_t<Key> ? 0 : byte_count<Key>;
            constexpr uint8_t tag = tag::object | type | (byte_cnt << 5);
            dump_type(tag, args...);

            dump_compressed_int<Opts>(value.size(), args...);
            for (auto&& [k, v] : value) {
               write<binary>::no_header<Opts>(k, ctx, args...);
               write<binary>::op<Opts>(v, ctx, args...);
            }
         }
      };

      template <nullable_t T>
         requires(std::is_array_v<T>)
      struct to_binary<T>
      {
         template <auto Opts, class V, size_t N, class... Args>
         GLZ_ALWAYS_INLINE static void op(const V (&value)[N], is_context auto&& ctx, Args&&... args) noexcept
         {
            write<binary>::op<Opts>(std::span{value, N}, ctx, std::forward<Args>(args)...);
         }
      };

      template <nullable_t T>
         requires(!std::is_array_v<T>)
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            if (value) {
               write<binary>::op<Opts>(*value, ctx, args...);
            }
            else {
               dump<tag::null>(args...);
            }
         }
      };

      template <class T>
         requires is_specialization_v<T, glz::obj> || is_specialization_v<T, glz::obj_copy>
      struct to_binary<T>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = glz::tuple_size_v<V> / 2;

            if constexpr (!has_opening_handled(Options)) {
               constexpr uint8_t type = 0; // string key
               constexpr uint8_t tag = tag::object | type;
               dump_type(tag, args...);
               dump_compressed_int<N>(args...);
            }

            for_each_flatten<N>([&](auto I) {
               constexpr auto Opts = opening_handled_off<Options>();
               write<binary>::no_header<Opts>(get<2 * I>(value.value), ctx, args...);
               write<binary>::op<Opts>(get<2 * I + 1>(value.value), ctx, args...);
            });
         }
      };

      template <class T>
         requires is_specialization_v<T, glz::merge>
      consteval size_t merge_element_count()
      {
         size_t count{};
         using Tuple = std::decay_t<decltype(std::declval<T>().value)>;
         for_each<glz::tuple_size_v<Tuple>>([&](auto I) constexpr {
            using Value = std::decay_t<glz::tuple_element_t<I, Tuple>>;
            if constexpr (is_specialization_v<Value, glz::obj> || is_specialization_v<Value, glz::obj_copy>) {
               count += glz::tuple_size_v<decltype(std::declval<Value>().value)> / 2;
            }
            else {
               count += refl<Value>.N;
            }
         });
         return count;
      }

      template <class T>
         requires is_specialization_v<T, glz::merge>
      struct to_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = glz::tuple_size_v<V>;

            constexpr uint8_t type = 0; // string key
            constexpr uint8_t tag = tag::object | type;
            dump_type(tag, b, ix);
            dump_compressed_int<merge_element_count<T>()>(b, ix);

            [&]<size_t... I>(std::index_sequence<I...>) {
               (write<binary>::op<opening_handled<Opts>()>(glz::get<I>(value.value), ctx, b, ix), ...);
            }(std::make_index_sequence<N>{});
         }
      };

      template <class T>
         requires(glaze_object_t<T> || reflectable<T>)
      struct to_binary<T> final
      {
         static constexpr auto N = refl<T>.N;
         static constexpr size_t count_to_write = [] {
            size_t count{};
            for_each_flatten<N>([&](auto I) {
               using V = std::remove_cvref_t<refl_t<T, I>>;

               if constexpr (std::same_as<V, hidden> || std::same_as<V, skip>) {
                  // do not serialize
                  // not serializing is_includer<V> would be a breaking change
               }
               else {
                  ++count;
               }
            });
            return count;
         }();

         template <auto Opts, class... Args>
            requires(Opts.structs_as_arrays == true)
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<tag::generic_array>(args...);
            dump_compressed_int<count_to_write>(args...);

            [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
               if constexpr (reflectable<T>) {
                  return to_tuple(value);
               }
               else {
                  return nullptr;
               }
            }();

            for_each_flatten<N>([&](auto I) {
               using val_t = std::remove_cvref_t<refl_t<T, I>>;

               if constexpr (std::same_as<val_t, hidden> || std::same_as<val_t, skip>) {
                  return;
               }
               else {
                  if constexpr (reflectable<T>) {
                     write<binary>::op<Opts>(get_member(value, get<I>(t)), ctx, args...);
                  }
                  else {
                     write<binary>::op<Opts>(get_member(value, get<I>(refl<T>.values)), ctx, args...);
                  }
               }
            });
         }

         template <auto Options, class... Args>
            requires(Options.structs_as_arrays == false)
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            if constexpr (!has_opening_handled(Options)) {
               constexpr uint8_t type = 0; // string key
               constexpr uint8_t tag = tag::object | type;
               dump_type(tag, args...);
               dump_compressed_int<count_to_write>(args...);
            }
            constexpr auto Opts = opening_handled_off<Options>();

            [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
               if constexpr (reflectable<T>) {
                  return to_tuple(value);
               }
               else {
                  return nullptr;
               }
            }();

            for_each_flatten<N>([&](auto I) {
               using val_t = std::remove_cvref_t<refl_t<T, I>>;

               if constexpr (std::same_as<val_t, hidden> || std::same_as<val_t, skip>) {
                  return;
               }
               else {
                  static constexpr sv key = refl<T>.keys[I];
                  write<binary>::no_header<Opts>(key, ctx, args...);

                  decltype(auto) member = [&]() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return get<I>(t);
                     }
                     else {
                        return get<I>(refl<T>.values);
                     }
                  }();

                  write<binary>::op<Opts>(get_member(value, member), ctx, args...);
               }
            });
         }
      };

      template <class T>
         requires glaze_array_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<tag::generic_array>(args...);

            static constexpr auto N = refl<T>.N;
            dump_compressed_int<N>(args...);

            for_each_flatten<refl<T>.N>(
               [&](auto I) { write<binary>::op<Opts>(get_member(value, get<I>(refl<T>.values)), ctx, args...); });
         }
      };

      template <class T>
         requires(tuple_t<T> || is_std_tuple<T>)
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<tag::generic_array>(args...);

            static constexpr auto N = glz::tuple_size_v<T>;
            dump_compressed_int<N>(args...);

            if constexpr (is_std_tuple<T>) {
               [&]<size_t... I>(std::index_sequence<I...>) {
                  (write<binary>::op<Opts>(std::get<I>(value), ctx, args...), ...);
               }(std::make_index_sequence<N>{});
            }
            else {
               [&]<size_t... I>(std::index_sequence<I...>) {
                  (write<binary>::op<Opts>(glz::get<I>(value), ctx, args...), ...);
               }(std::make_index_sequence<N>{});
            }
         }
      };

      template <filesystem_path T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args) noexcept
         {
            to_binary<decltype(value.string())>::template op<Opts>(value.string(), std::forward<Args>(args)...);
         }
      };

      template <class T = void>
      struct to_binary_partial
      {};

      template <auto& Partial, auto Opts, class T, class Ctx, class B, class IX>
      concept write_binary_partial_invocable = requires(T&& value, Ctx&& ctx, B&& b, IX&& ix) {
         to_binary_partial<std::remove_cvref_t<T>>::template op<Partial, Opts>(
            std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
      };

      template <>
      struct write_partial<binary>
      {
         template <auto& Partial, auto Opts, class T, is_context Ctx, class B, class IX>
         static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            if constexpr (std::count(Partial.begin(), Partial.end(), "") > 0) {
               detail::write<binary>::op<Opts>(value, ctx, b, ix);
            }
            else if constexpr (write_binary_partial_invocable<Partial, Opts, T, Ctx, B, IX>) {
               to_binary_partial<std::remove_cvref_t<T>>::template op<Partial, Opts>(
                  std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
            }
            else {
               static_assert(false_v<T>, "Glaze metadata is probably needed for your type");
            }
         }
      };

      // Only object types are supported for partial
      template <class T>
         requires(glaze_object_t<T> || writable_map_t<T> || reflectable<T>)
      struct to_binary_partial<T> final
      {
         template <auto& Partial, auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            static constexpr auto sorted = sort_json_ptrs(Partial);
            static constexpr auto groups = glz::group_json_ptrs<sorted>();
            static constexpr auto N = glz::tuple_size_v<std::decay_t<decltype(groups)>>;

            constexpr uint8_t type = 0; // string
            constexpr uint8_t tag = tag::object | type;
            detail::dump_type(tag, b, ix);

            detail::dump_compressed_int<N>(b, ix);

            if constexpr (glaze_object_t<T>) {
               for_each<N>([&](auto I) {
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  static constexpr auto group = glz::get<I>(groups);

                  static constexpr auto key = get<0>(group);
                  static constexpr auto sub_partial = get<1>(group);
                  static constexpr auto frozen_map = detail::make_map<T>();
                  static constexpr auto member_it = frozen_map.find(key);
                  static_assert(member_it != frozen_map.end(), "Invalid key passed to partial write");
                  static constexpr auto index = member_it->second.index();
                  static constexpr decltype(auto) element = get<index>(member_it->second);

                  detail::write<binary>::no_header<Opts>(key, ctx, b, ix);
                  write_partial<binary>::op<sub_partial, Opts>(glz::detail::get_member(value, element), ctx, b, ix);
               });
            }
            else if constexpr (writable_map_t<T>) {
               for_each<N>([&](auto I) {
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  static constexpr auto group = glz::get<I>(groups);

                  static constexpr auto key_value = get<0>(group);
                  static constexpr auto sub_partial = get<1>(group);
                  if constexpr (findable<std::decay_t<T>, decltype(key_value)>) {
                     detail::write<binary>::no_header<Opts>(key_value, ctx, b, ix);
                     auto it = value.find(key_value);
                     if (it != value.end()) {
                        write_partial<binary>::op<sub_partial, Opts>(it->second, ctx, b, ix);
                     }
                     else {
                        ctx.error = error_code::invalid_partial_key;
                        return;
                     }
                  }
                  else {
                     static thread_local auto key =
                        typename std::decay_t<T>::key_type(key_value); // TODO handle numeric keys
                     detail::write<binary>::no_header<Opts>(key, ctx, b, ix);
                     auto it = value.find(key);
                     if (it != value.end()) {
                        write_partial<binary>::op<sub_partial, Opts>(it->second, ctx, b, ix);
                     }
                     else {
                        ctx.error = error_code::invalid_partial_key;
                        return;
                     }
                  }
               });
            }
         }
      };
   }

   template <write_binary_supported T, class Buffer>
   [[nodiscard]] error_ctx write_binary(T&& value, Buffer&& buffer) noexcept
   {
      return write<opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <opts Opts = opts{}, write_binary_supported T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_binary(T&& value) noexcept
   {
      return write<set_binary<Opts>()>(std::forward<T>(value));
   }

   template <auto& Partial, write_binary_supported T, class Buffer>
   [[nodiscard]] error_ctx write_binary(T&& value, Buffer&& buffer) noexcept
   {
      return write<Partial, opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   // requires file_name to be null terminated
   template <opts Opts = opts{}, write_binary_supported T>
   [[nodiscard]] error_ctx write_file_binary(T&& value, const sv file_name, auto&& buffer) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      const auto ec = write<set_binary<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec;
      }

      std::ofstream file(file_name.data(), std::ios::binary);

      if (file) {
         file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
      }
      else {
         return {error_code::file_open_failure};
      }

      return {};
   }

   template <write_binary_supported T, class Buffer>
   [[nodiscard]] error_ctx write_binary_untagged(T&& value, Buffer&& buffer) noexcept
   {
      return write<opts{.format = binary, .structs_as_arrays = true}>(std::forward<T>(value),
                                                                      std::forward<Buffer>(buffer));
   }

   template <write_binary_supported T>
   [[nodiscard]] error_ctx write_binary_untagged(T&& value) noexcept
   {
      return write<opts{.format = binary, .structs_as_arrays = true}>(std::forward<T>(value));
   }

   template <opts Opts = opts{}, write_binary_supported T>
   [[nodiscard]] error_ctx write_file_binary_untagged(T&& value, const std::string& file_name, auto&& buffer) noexcept
   {
      return write_file_binary<opt_true<Opts, &opts::structs_as_arrays>>(std::forward<T>(value), file_name, buffer);
   }
}
