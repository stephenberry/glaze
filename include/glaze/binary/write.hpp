// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

#include "glaze/binary/header.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/write.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   namespace detail
   {
      GLZ_ALWAYS_INLINE void dump_type(auto&& value, auto&& b, auto&& ix) noexcept
      {
         assert(ix <= b.size());
         constexpr auto n = sizeof(std::decay_t<decltype(value)>);
         if (ix + n > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, ix + n));
         }

         std::memcpy(b.data() + ix, &value, n);
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

      template <class T = void>
      struct to_binary
      {};

      template <auto Opts, class T, class Ctx, class B, class IX>
      concept write_binary_invocable = requires(T&& value, Ctx&& ctx, B&& b, IX&& ix) {
         to_binary<std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                              std::forward<B>(b), std::forward<IX>(ix));
      };

      template <>
      struct write<binary>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            if constexpr (write_binary_invocable<Opts, T, Ctx, B, IX>) {
               to_binary<std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                    std::forward<B>(b), std::forward<IX>(ix));
            }
            else {
               static_assert(false_v<T>, "Glaze metadata is probably needed for your type");
            }
         }

         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void no_header(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            to_binary<std::remove_cvref_t<T>>::template no_header<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                        std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <glaze_value_t T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args) noexcept
         {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            to_binary<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Args>(args)...);
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
            // dump(bytes, args...);
            dump(std::as_bytes(std::span{bytes}), args...);
         }
      };

      template <glaze_flags_t T>
      struct to_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix)
         {
            static constexpr auto N = std::tuple_size_v<meta_t<T>>;

            std::array<uint8_t, byte_length<T>()> data{};

            for_each<N>([&](auto I) {
               static constexpr auto item = glz::get<I>(meta_v<T>);

               data[I / 8] |= static_cast<uint8_t>(get_member(value, glz::get<1>(item))) << (7 - (I % 8));
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

      template <class T>
      struct to_binary<includer<T>>
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
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&...) noexcept
         {}
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

      template <is_variant T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            std::visit(
               [&](auto&& v) {
                  using V = std::decay_t<decltype(v)>;
                  static constexpr auto index = []() {
                     T var = V{};
                     return var.index();
                  }();

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
            constexpr uint8_t tag = tag::string;

            dump_type(tag, b, ix);
            const auto n = value.size();
            dump_compressed_int<Opts>(n, b, ix);

            assert(ix <= b.size());
            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }

            std::memcpy(b.data() + ix, value.data(), n);
            ix += n;
         }

         template <auto Opts>
         GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            dump_compressed_int<Opts>(value.size(), b, ix);

            assert(ix <= b.size());
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
                  auto dump_array = [&](auto&& b, auto&& ix) {
                     assert(ix <= b.size());
                     const auto n = value.size() * sizeof(V);
                     if (ix + n > b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, ix + n));
                     }

                     std::memcpy(b.data() + ix, value.data(), n);
                     ix += n;
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
                     assert(ix <= b.size());
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
         GLZ_FLATTEN static auto op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
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
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            if (value) {
               write<binary>::op<Opts>(*value, ctx, args...);
            }
            else {
               dump<std::byte(tag::null)>(args...);
            }
         }
      };

      template <class T>
         requires is_specialization_v<T, glz::obj> || is_specialization_v<T, glz::obj_copy>
      struct to_binary<T>
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            constexpr uint8_t type = 0; // string key
            constexpr uint8_t tag = tag::object | type;
            dump_type(tag, args...);

            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = std::tuple_size_v<V> / 2;
            dump_compressed_int<N>(args...);

            for_each<N>([&](auto I) {
               write<binary>::no_header<Opts>(get<2 * I>(value.value), ctx, args...);
               write<binary>::op<Opts>(get<2 * I + 1>(value.value), ctx, args...);
            });
         }
      };

      template <class T>
         requires glaze_object_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            constexpr uint8_t type = 0; // string key
            constexpr uint8_t tag = tag::object | type;
            dump_type(tag, args...);

            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump_compressed_int<N>(args...);

            for_each<N>([&](auto I) {
               static constexpr auto item = get<I>(meta_v<V>);
               using T0 = std::decay_t<decltype(get<0>(item))>;
               static constexpr bool use_reflection = std::is_member_object_pointer_v<T0>;
               static constexpr auto member_index = use_reflection ? 0 : 1;
               using Value = std::decay_t<decltype(get<member_index>(item))>;
               if constexpr (std::is_same_v<Value, includer<V>>) {
                  return;
               }
               if constexpr (use_reflection) {
                  write<binary>::no_header<Opts>(get_name<get<0>(item)>(), ctx, args...);
               }
               else {
                  write<binary>::no_header<Opts>(get<0>(item), ctx, args...);
               }
               write<binary>::op<Opts>(get_member(value, get<member_index>(item)), ctx, args...);
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
            dump<std::byte(tag::generic_array)>(args...);

            static constexpr auto N = std::tuple_size_v<meta_t<T>>;
            dump_compressed_int<N>(args...);

            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<meta_t<V>>>(
               [&](auto I) { write<binary>::op<Opts>(get_member(value, glz::get<I>(meta_v<V>)), ctx, args...); });
         }
      };

      template <class T>
         requires is_std_tuple<std::decay_t<T>>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<std::byte(tag::generic_array)>(args...);

            static constexpr auto N = std::tuple_size_v<T>;
            dump_compressed_int<N>(args...);

            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<V>>([&](auto I) { write<binary>::op<Opts>(std::get<I>(value), ctx, args...); });
         }
      };
   }

   template <class T, class Buffer>
   inline void write_binary(T&& value, Buffer&& buffer)
   {
      write<opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   inline auto write_binary(T&& value)
   {
      std::string buffer{};
      write<opts{.format = binary}>(std::forward<T>(value), buffer);
      return buffer;
   }

   template <class Map, class Key>
   concept findable = requires(Map& map, const Key& key) { map.find(key); };

   template <auto& Partial, opts Opts, class T, output_buffer Buffer>
   [[nodiscard]] inline write_error write(T&& value, is_context auto&& ctx, Buffer& buffer, auto& ix) noexcept
   {
      write_error we{};

      if constexpr (std::count(Partial.begin(), Partial.end(), "") > 0) {
         detail::write<binary>::op<Opts>(value, ctx, buffer, ix);
      }
      else {
         using V = std::decay_t<T>;
         static_assert(detail::glaze_object_t<V> || detail::writable_map_t<V>,
                       "Only object types are supported for partial.");
         static constexpr auto sorted = sort_json_ptrs(Partial);
         static constexpr auto groups = glz::group_json_ptrs<sorted>();
         static constexpr auto N = std::tuple_size_v<std::decay_t<decltype(groups)>>;

         constexpr uint8_t type = 0; // string
         constexpr uint8_t tag = tag::object | type;
         detail::dump_type(tag, buffer, ix);

         detail::dump_compressed_int<N>(buffer, ix);

         if constexpr (detail::glaze_object_t<std::decay_t<T>>) {
            for_each<N>([&](auto I) {
               static constexpr auto group = glz::get<I>(groups);

               static constexpr auto key = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               static constexpr auto frozen_map = detail::make_map<T>();
               static constexpr auto member_it = frozen_map.find(key);
               static_assert(member_it != frozen_map.end(), "Invalid key passed to partial write");
               static constexpr auto index = member_it->second.index();
               static constexpr decltype(auto) member_ptr = std::get<index>(member_it->second);

               detail::write<binary>::no_header<Opts>(key, ctx, buffer, ix);
               std::ignore = write<sub_partial, Opts>(glz::detail::get_member(value, member_ptr), ctx, buffer, ix);
            });
         }
         else if constexpr (detail::writable_map_t<std::decay_t<T>>) {
            for_each<N>([&](auto I) {
               static constexpr auto group = glz::get<I>(groups);

               static constexpr auto key_value = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               if constexpr (findable<std::decay_t<T>, decltype(key_value)>) {
                  detail::write<binary>::no_header<Opts>(key_value, ctx, buffer, ix);
                  auto it = value.find(key_value);
                  if (it != value.end()) {
                     std::ignore = write<sub_partial, Opts>(it->second, ctx, buffer, ix);
                  }
                  else {
                     we.ec = error_code::invalid_partial_key;
                  }
               }
               else {
                  static thread_local auto key =
                     typename std::decay_t<T>::key_type(key_value); // TODO handle numeric keys
                  detail::write<binary>::no_header<Opts>(key, ctx, buffer, ix);
                  auto it = value.find(key);
                  if (it != value.end()) {
                     std::ignore = write<sub_partial, Opts>(it->second, ctx, buffer, ix);
                  }
                  else {
                     we.ec = error_code::invalid_partial_key;
                  }
               }
            });
         }
      }

      return we;
   }

   template <auto& Partial, opts Opts, class T, output_buffer Buffer>
   [[nodiscard]] inline write_error write(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (detail::resizeable<Buffer>) {
         if (buffer.empty()) {
            buffer.resize(128);
         }
      }
      context ctx{};
      size_t ix = 0;
      const auto error = write<Partial, Opts>(std::forward<T>(value), ctx, buffer, ix);
      if constexpr (detail::resizeable<Buffer>) {
         buffer.resize(ix);
      }
      return error;
   }

   template <auto& Partial, class T, class Buffer>
   inline auto write_binary(T&& value, Buffer&& buffer)
   {
      return write<Partial, opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   // std::string file_name needed for std::ofstream
   template <class T>
   [[nodiscard]] inline write_error write_file_binary(T&& value, const std::string& file_name, auto&& buffer) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      write<opts{.format = binary}>(std::forward<T>(value), buffer);

      std::ofstream file(file_name, std::ios::binary);

      if (file) {
         file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
      }
      else {
         return {error_code::file_open_failure};
      }

      return {};
   }
}
