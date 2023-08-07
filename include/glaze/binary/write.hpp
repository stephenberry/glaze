// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

#include "glaze/binary/header.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/write.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/error.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/murmur.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   namespace detail
   {
      template <class... Args>
      GLZ_ALWAYS_INLINE void dump_type(auto&& value, Args&&... args) noexcept
      {
         dump(std::as_bytes(std::span{&value, 1}), std::forward<Args>(args)...);
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
      GLZ_ALWAYS_INLINE void dump_compressed_int(size_t i, Args&&... args) noexcept
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

      template <>
      struct write<binary>
      {
         template <auto Opts, class T, is_context Ctx, class B>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b) noexcept
         {
            to_binary<std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                          std::forward<B>(b));
         }

         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            to_binary<std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
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

      template <glaze_flags_t T>
      struct to_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix)
         {
            static constexpr auto N = std::tuple_size_v<meta_t<T>>;

            std::array<uint8_t, byte_length<T>()> data{};

            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<T>);

               data[I / 8] |= static_cast<uint8_t>(get_member(value, glz::tuplet::get<1>(item))) << (7 - (I % 8));
            });

            dump(data, b, ix);
         }
      };

      template <is_member_function_pointer T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&&, Args&&...) noexcept
         {}
      };

      template <class T>
      struct to_binary<includer<T>>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&&, Args&&...) noexcept
         {}
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
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&&, Args&&...) noexcept
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

                  uint8_t tag = tag::type;
                  dump_type(tag, args...);
                  dump_compressed_int<index>(args...);
                  write<binary>::op<Opts>(v, ctx, std::forward<Args>(args)...);
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
            constexpr uint8_t type = std::floating_point<T> ? 0 : (std::unsigned_integral<T> ? 0b000'10'000 : 0b000'01'000);
            constexpr uint8_t tag = tag::number | type | (byte_count<T> << 5);
            dump_type(tag, args...);
            dump_type(value, args...);
         }
      };

      template <str_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args) noexcept
         {
            using char_type = std::decay_t<decltype(*value.data())>;
            constexpr uint8_t tag = tag::string | (byte_count<char_type> << 3);
            dump_type(tag, args...);

            dump_compressed_int<Opts>(value.size(), args...);
            dump(std::as_bytes(std::span{value.data(), value.size()}), args...);
         }
      };

      template <array_t T>
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
                  for (size_t byte_i{}, i{}; byte_i < num_bytes - 1; ++byte_i) {
                     for (size_t bit_i = 7; bit_i < 8 && i < value.size(); --bit_i, ++i) {
                        bytes[byte_i] |= uint8_t(value[i]) << uint8_t(bit_i);
                     }
                  }
                  dump(bytes, args...);
               }
               else {
                  const auto num_bytes = (value.size() + 7) / 8;
                  for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i) {
                     uint8_t byte{};
                     for (size_t bit_i = 7; bit_i < 8 && i < value.size(); --bit_i, ++i) {
                        byte |= uint8_t(value[i]) << uint8_t(bit_i);
                     }
                     dump_type(byte, args...);
                  }
               }
            }
            else if constexpr (num_t<V>) {
               constexpr uint8_t type = std::floating_point<V> ? 0 : (std::unsigned_integral<V> ? 0b000'10'000 : 0b000'01'000);
               constexpr uint8_t tag = tag::typed_array | type | (byte_count<V> << 5);
               dump_type(tag, args...);
               dump_compressed_int<Opts>(value.size(), args...);

               if constexpr (contiguous<T>) {
                  dump(std::as_bytes(std::span{value.data(), static_cast<size_t>(value.size())}), args...);
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
               using char_type = std::decay_t<decltype(*std::declval<V>().data())>;
               constexpr uint8_t tag = tag::typed_array | type | string_indicator | (byte_count<char_type> << 6);
               dump_type(tag, args...);
               dump_compressed_int<Opts>(value.size(), args...);

               for (auto& x : value) {
                  dump_compressed_int<Opts>(x.size(), args...);
                  dump(std::as_bytes(std::span{x.data(), x.size()}), args...);
               }
            }
            else {
               constexpr uint8_t tag = tag::untyped_array | (byte_count<V> << 3);
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
            
            constexpr uint8_t type = str_t<Key> ? 0 : (std::unsigned_integral<Key> ? 0b000'10'000 : 0b000'01'000);
            constexpr uint8_t byte_count = str_t<Key> ? 1 : sizeof(Key);
            constexpr uint8_t tag = tag::object | type | (byte_count << 5);
            dump_type(tag, args...);

            dump_compressed_int<Opts>(1, args...);
            const auto& [k, v] = value;
            write<binary>::op<Opts>(k, ctx, args...);
            write<binary>::op<Opts>(v, ctx, args...);
         }
      };

      template <writable_map_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static auto op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using Key = typename T::key_type;
            
            constexpr uint8_t type = str_t<Key> ? 0 : (std::unsigned_integral<Key> ? 0b000'10'000 : 0b000'01'000);
            constexpr uint8_t byte_count = str_t<Key> ? 1 : sizeof(Key);
            constexpr uint8_t tag = tag::object | type | (byte_count << 5);
            dump_type(tag, args...);

            dump_compressed_int<Opts>(value.size(), args...);
            for (auto&& [k, v] : value) {
               write<binary>::op<Opts>(k, ctx, args...);
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
         requires glaze_object_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            constexpr uint8_t type = 0;
            constexpr uint8_t byte_count = 1;
            constexpr uint8_t tag = tag::object | type | (byte_count << 5);
            dump_type(tag, args...);

            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump_compressed_int<N>(args...);

            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
               write<binary>::op<Opts>(glz::tuplet::get<0>(item), ctx, args...);
               write<binary>::op<Opts>(get_member(value, glz::tuplet::get<1>(item)), ctx, args...);
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
            dump<std::byte(tag::untyped_array)>(args...);

            static constexpr auto N = std::tuple_size_v<meta_t<T>>;
            dump_compressed_int<N>(args...);

            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<meta_t<V>>>([&](auto I) {
               write<binary>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<V>)), ctx,
                                       std::forward<Args>(args)...);
            });
         }
      };

      template <class T>
         requires is_std_tuple<std::decay_t<T>>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<std::byte(tag::untyped_array)>(args...);

            static constexpr auto N = std::tuple_size_v<T>;
            dump_compressed_int<N>(args...);

            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<V>>(
               [&](auto I) { write<binary>::op<Opts>(std::get<I>(value), ctx, std::forward<Args>(args)...); });
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
   [[nodiscard]] inline write_error write(T&& value, Buffer& buffer, is_context auto&& ctx) noexcept
   {
      write_error we{};

      if constexpr (std::count(Partial.begin(), Partial.end(), "") > 0) {
         detail::write<binary>::op<Opts>(value, ctx, buffer);
      }
      else {
         using V = std::decay_t<T>;
         static_assert(detail::glaze_object_t<V> || detail::writable_map_t<V>,
                       "Only object types are supported for partial.");
         static constexpr auto sorted = sort_json_ptrs(Partial);
         static constexpr auto groups = glz::group_json_ptrs<sorted>();
         static constexpr auto N = std::tuple_size_v<std::decay_t<decltype(groups)>>;

         constexpr uint8_t type = 0;
         constexpr uint8_t byte_count = sizeof(decltype(buffer[0]));
         constexpr uint8_t tag = tag::object | type | (byte_count << 5);
         detail::dump_type(tag, buffer);

         detail::dump_compressed_int<N>(buffer);

         if constexpr (detail::glaze_object_t<std::decay_t<T>>) {
            glz::for_each<N>([&](auto I) {
               static constexpr auto group = glz::tuplet::get<I>(groups);

               static constexpr auto key = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               static constexpr auto frozen_map = detail::make_map<T>();
               static constexpr auto member_it = frozen_map.find(key);
               static_assert(member_it != frozen_map.end(), "Invalid key passed to partial write");
               static constexpr auto ix = member_it->second.index();
               static constexpr decltype(auto) member_ptr = std::get<ix>(member_it->second);

               detail::write<binary>::op<Opts>(key, ctx, buffer);
               std::ignore = write<sub_partial, Opts>(glz::detail::get_member(value, member_ptr), buffer, ctx);
            });
         }
         else if constexpr (detail::writable_map_t<std::decay_t<T>>) {
            glz::for_each<N>([&](auto I) {
               static constexpr auto group = glz::tuplet::get<I>(groups);

               static constexpr auto key_value = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               if constexpr (findable<std::decay_t<T>, decltype(key_value)>) {
                  detail::write<binary>::op<Opts>(key_value, ctx, buffer);
                  auto it = value.find(key_value);
                  if (it != value.end()) {
                     std::ignore = write<sub_partial, Opts>(it->second, buffer, ctx);
                  }
                  else {
                     we.ec = error_code::invalid_partial_key;
                  }
               }
               else {
                  static thread_local auto key =
                     typename std::decay_t<T>::key_type(key_value); // TODO handle numeric keys
                  detail::write<binary>::op<Opts>(key, ctx, buffer);
                  auto it = value.find(key);
                  if (it != value.end()) {
                     std::ignore = write<sub_partial, Opts>(it->second, buffer, ctx);
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
      context ctx{};
      return write<Partial, Opts>(std::forward<T>(value), buffer, ctx);
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
      write<opts{.format = binary}>(std::forward<T>(value), buffer);

      std::ofstream file(file_name);

      if (file) {
         file << buffer;
      }
      else {
         return {error_code::file_open_failure};
      }

      return {};
   }
}
