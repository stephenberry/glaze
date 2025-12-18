// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/seek.hpp"
#include "glaze/core/to.hpp"
#include "glaze/core/write.hpp"
#include "glaze/msgpack/common.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/variant.hpp"

namespace glz::msgpack::detail
{
   template <class B>
   GLZ_ALWAYS_INLINE void write_nil(B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      dump(std::byte{nil}, b, ix);
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_bool(const bool value, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      dump(std::byte{static_cast<uint8_t>(value ? bool_true : bool_false)}, b, ix);
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_unsigned(uint64_t value, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if (value <= 0x7F) {
         dump(std::byte{static_cast<uint8_t>(value)}, b, ix);
      }
      else if (value <= std::numeric_limits<uint8_t>::max()) {
         dump(std::byte{uint8}, b, ix);
         dump_uint8(static_cast<uint8_t>(value), b, ix);
      }
      else if (value <= std::numeric_limits<uint16_t>::max()) {
         dump(std::byte{uint16}, b, ix);
         dump_uint16(static_cast<uint16_t>(value), b, ix);
      }
      else if (value <= std::numeric_limits<uint32_t>::max()) {
         dump(std::byte{uint32}, b, ix);
         dump_uint32(static_cast<uint32_t>(value), b, ix);
      }
      else {
         dump(std::byte{uint64}, b, ix);
         dump_uint64(value, b, ix);
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_signed(int64_t value, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if (value >= -32 && value <= 127) {
         dump(std::byte{static_cast<uint8_t>(value)}, b, ix);
      }
      else if (value >= std::numeric_limits<int8_t>::min() && value <= std::numeric_limits<int8_t>::max()) {
         dump(std::byte{int8}, b, ix);
         dump_uint8(static_cast<uint8_t>(value), b, ix);
      }
      else if (value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max()) {
         dump(std::byte{int16}, b, ix);
         dump_uint16(static_cast<uint16_t>(value), b, ix);
      }
      else if (value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max()) {
         dump(std::byte{int32}, b, ix);
         dump_uint32(static_cast<uint32_t>(value), b, ix);
      }
      else {
         dump(std::byte{int64}, b, ix);
         dump_uint64(static_cast<uint64_t>(value), b, ix);
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_floating(const auto value, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      using T = std::decay_t<decltype(value)>;
      if constexpr (sizeof(T) <= sizeof(float)) {
         dump(std::byte{float32}, b, ix);
         dump_float32(static_cast<float>(value), b, ix);
      }
      else {
         dump(std::byte{float64}, b, ix);
         dump_float64(static_cast<double>(value), b, ix);
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_str_header(size_t size, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if (size <= 31) {
         dump(std::byte{static_cast<uint8_t>(fixstr_bits | size)}, b, ix);
      }
      else if (size <= std::numeric_limits<uint8_t>::max()) {
         dump(std::byte{str8}, b, ix);
         dump_uint8(static_cast<uint8_t>(size), b, ix);
      }
      else if (size <= std::numeric_limits<uint16_t>::max()) {
         dump(std::byte{str16}, b, ix);
         dump_uint16(static_cast<uint16_t>(size), b, ix);
      }
      else {
         dump(std::byte{str32}, b, ix);
         dump_uint32(static_cast<uint32_t>(size), b, ix);
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_array_header(size_t size, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if (size <= 15) {
         dump(std::byte{static_cast<uint8_t>(fixarray_bits | size)}, b, ix);
      }
      else if (size <= std::numeric_limits<uint16_t>::max()) {
         dump(std::byte{array16}, b, ix);
         dump_uint16(static_cast<uint16_t>(size), b, ix);
      }
      else {
         dump(std::byte{array32}, b, ix);
         dump_uint32(static_cast<uint32_t>(size), b, ix);
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_map_header(size_t size, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if (size <= 15) {
         dump(std::byte{static_cast<uint8_t>(fixmap_bits | size)}, b, ix);
      }
      else if (size <= std::numeric_limits<uint16_t>::max()) {
         dump(std::byte{map16}, b, ix);
         dump_uint16(static_cast<uint16_t>(size), b, ix);
      }
      else {
         dump(std::byte{map32}, b, ix);
         dump_uint32(static_cast<uint32_t>(size), b, ix);
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void write_binary_header(size_t size, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if (size <= std::numeric_limits<uint8_t>::max()) {
         dump(std::byte{bin8}, b, ix);
         dump_uint8(static_cast<uint8_t>(size), b, ix);
      }
      else if (size <= std::numeric_limits<uint16_t>::max()) {
         dump(std::byte{bin16}, b, ix);
         dump_uint16(static_cast<uint16_t>(size), b, ix);
      }
      else {
         dump(std::byte{bin32}, b, ix);
         dump_uint32(static_cast<uint32_t>(size), b, ix);
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump_raw_bytes(const char* data, size_t size, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if (size == 0) {
         return;
      }
      if constexpr (vector_like<B>) {
         maybe_pad(size, b, ix);
      }
      std::memcpy(&b[ix], data, size);
      ix += size;
   }

}

namespace glz
{
   template <>
   struct serialize<MSGPACK>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<MSGPACK, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   template <auto& Partial, auto Opts, class T, class Ctx, class B, class IX>
   concept write_msgpack_partial_invocable = requires(T&& value, Ctx&& ctx, B&& b, IX&& ix) {
      to_partial<MSGPACK, std::remove_cvref_t<T>>::template op<Partial, Opts>(
         std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
   };

   template <>
   struct serialize_partial<MSGPACK>
   {
      template <auto& Partial, auto Opts, class T, is_context Ctx, class B, class IX>
      static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if constexpr (std::count(Partial.begin(), Partial.end(), "") > 0) {
            serialize<MSGPACK>::op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b),
                                         std::forward<IX>(ix));
         }
         else if constexpr (write_msgpack_partial_invocable<Partial, Opts, T, Ctx, B, IX>) {
            to_partial<MSGPACK, std::remove_cvref_t<T>>::template op<Partial, Opts>(
               std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
         else {
            static_assert(false_v<T>, "Glaze metadata is probably needed for your type");
         }
      }
   };

   template <class T>
      requires(glaze_value_t<T> && !custom_write<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::remove_cvref_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         to<MSGPACK, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                           std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   template <always_null_t T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&... args)
      {
         msgpack::detail::write_nil(std::forward<Args>(args)...);
      }
   };

   template <nullable_like T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if (!value) {
            msgpack::detail::write_nil(b, ix);
            return;
         }
         serialize<MSGPACK>::op<Opts>(*value, ctx, b, ix);
      }
   };

   template <class T>
      requires(nullable_value_t<T> && !nullable_like<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if (!value.has_value()) {
            msgpack::detail::write_nil(b, ix);
            return;
         }
         serialize<MSGPACK>::op<Opts>(value.value(), ctx, b, ix);
      }
   };

   template <nullable_wrapper T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if (!value.val) {
            msgpack::detail::write_nil(b, ix);
            return;
         }
         serialize<MSGPACK>::op<Opts>(*value.val, ctx, b, ix);
      }
   };

   template <boolean_like T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&&, Args&&... args)
      {
         msgpack::detail::write_bool(value, std::forward<Args>(args)...);
      }
   };

   template <is_bitset T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class B, class IX>
      static void op(auto&& value, is_context auto&&, B&& b, IX&& ix)
      {
         const auto num_bytes = (value.size() + 7) / 8;
         std::vector<uint8_t> bytes(num_bytes);
         for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i) {
            for (size_t bit_i = 0; bit_i < 8 && i < value.size(); ++bit_i, ++i) {
               bytes[byte_i] |= uint8_t(value[i]) << uint8_t(bit_i);
            }
         }
         msgpack::detail::write_binary_header(bytes.size(), b, ix);
         msgpack::detail::dump_raw_bytes(reinterpret_cast<const char*>(bytes.data()), bytes.size(), b, ix);
      }
   };

   template <class T>
      requires(std::is_enum_v<T> && !glaze_enum_t<T> && !custom_write<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using U = std::underlying_type_t<std::decay_t<T>>;
         to<MSGPACK, U>::template op<Opts>(static_cast<U>(value), ctx, b, ix);
      }
   };

   template <class T>
      requires(is_named_enum<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         const sv str = get_enum_name(value);
         if (!str.empty()) {
            msgpack::detail::write_str_header(str.size(), b, ix);
            msgpack::detail::dump_raw_bytes(str.data(), str.size(), b, ix);
         }
         else {
            // fallback to numeric representation
            serialize<MSGPACK>::op<Opts>(static_cast<std::underlying_type_t<T>>(value), ctx, b, ix);
         }
      }
   };

   template <class T>
      requires(num_t<T> || char_t<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args)
      {
         if constexpr (std::floating_point<std::remove_cvref_t<decltype(value)>>) {
            msgpack::detail::write_floating(value, std::forward<Args>(args)...);
         }
         else if constexpr (std::is_signed_v<std::remove_cvref_t<decltype(value)>>) {
            msgpack::detail::write_signed(static_cast<int64_t>(value), std::forward<Args>(args)...);
         }
         else {
            msgpack::detail::write_unsigned(static_cast<uint64_t>(value), std::forward<Args>(args)...);
         }
      }
   };

   template <string_t T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&&, B&& b, IX&& ix)
      {
         const std::string_view str{value.data(), value.size()};
         msgpack::detail::write_str_header(str.size(), b, ix);
         msgpack::detail::dump_raw_bytes(str.data(), str.size(), b, ix);
      }
   };

   template <static_string_t T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&&, B&& b, IX&& ix)
      {
         const std::string_view str{value.data(), value.size()};
         msgpack::detail::write_str_header(str.size(), b, ix);
         msgpack::detail::dump_raw_bytes(str.data(), str.size(), b, ix);
      }
   };

   template <str_t T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&&, B&& b, IX&& ix)
      {
         const std::string_view str{value};
         msgpack::detail::write_str_header(str.size(), b, ix);
         msgpack::detail::dump_raw_bytes(str.data(), str.size(), b, ix);
      }
   };

   template <glaze_object_t T>
      requires(!custom_write<T>)
   struct to<MSGPACK, T>
   {
      static constexpr auto N = reflect<T>::size;

      template <auto Opts>
      static consteval size_t count_members()
      {
         return []<size_t... I>(std::index_sequence<I...>) consteval {
            return (size_t{} + ... + (std::same_as<field_t<T, I>, hidden> || std::same_as<field_t<T, I>, skip>
                                         ? size_t{}
                                         : size_t{1}));
         }(std::make_index_sequence<N>{});
      }

      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if constexpr (Opts.structs_as_arrays) {
            msgpack::detail::write_array_header(count_members<Opts>(), b, ix);
            for_each<N>([&]<size_t I>() {
               if constexpr (!std::same_as<field_t<T, I>, hidden> && !std::same_as<field_t<T, I>, skip>) {
                  serialize<MSGPACK>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, b, ix);
               }
            });
         }
         else {
            msgpack::detail::write_map_header(count_members<Opts>(), b, ix);
            for_each<N>([&]<size_t I>() {
               if constexpr (!std::same_as<field_t<T, I>, hidden> && !std::same_as<field_t<T, I>, skip>) {
                  static constexpr sv key = reflect<T>::keys[I];
                  msgpack::detail::write_str_header(key.size(), b, ix);
                  msgpack::detail::dump_raw_bytes(key.data(), key.size(), b, ix);
                  serialize<MSGPACK>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, b, ix);
               }
            });
         }
      }
   };

   template <reflectable T>
      requires(!custom_write<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<MSGPACK, decltype(to_tie(value))>::template op<Opts>(to_tie(value), ctx, b, ix);
      }
   };

   template <writable_map_t T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         msgpack::detail::write_map_header(value.size(), b, ix);
         for (auto&& item : value) {
            serialize<MSGPACK>::op<Opts>(item.first, ctx, b, ix);
            serialize<MSGPACK>::op<Opts>(item.second, ctx, b, ix);
         }
      }
   };

   template <writable_array_t T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if constexpr (msgpack::binary_range_v<Value>) {
            const size_t size = value.size();
            msgpack::detail::write_binary_header(size, b, ix);
            if (size == 0) {
               return;
            }
            const auto* data = reinterpret_cast<const char*>(value.data());
            msgpack::detail::dump_raw_bytes(data, size, b, ix);
            return;
         }

         msgpack::detail::write_array_header(value.size(), b, ix);
         for (auto&& element : value) {
            serialize<MSGPACK>::op<Opts>(element, ctx, b, ix);
         }
      }
   };

   template <>
   struct to<MSGPACK, msgpack::ext>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         const size_t len = value.data.size();
         const auto type_byte = static_cast<uint8_t>(value.type);

         auto dump_payload = [&](auto&& emit_header) {
            emit_header();
            dump(std::byte{type_byte}, b, ix);
            if (len > 0) {
               const auto* data = reinterpret_cast<const char*>(value.data.data());
               msgpack::detail::dump_raw_bytes(data, len, b, ix);
            }
         };

         switch (len) {
         case 1:
            dump_payload([&] {
               dump(std::byte{msgpack::fixext1}, b, ix);
            });
            return;
         case 2:
            dump_payload([&] {
               dump(std::byte{msgpack::fixext2}, b, ix);
            });
            return;
         case 4:
            dump_payload([&] {
               dump(std::byte{msgpack::fixext4}, b, ix);
            });
            return;
         case 8:
            dump_payload([&] {
               dump(std::byte{msgpack::fixext8}, b, ix);
            });
            return;
         case 16:
            dump_payload([&] {
               dump(std::byte{msgpack::fixext16}, b, ix);
            });
            return;
         default:
            break;
         }

         if (len <= std::numeric_limits<uint8_t>::max()) {
            dump_payload([&] {
               dump(std::byte{msgpack::ext8}, b, ix);
               msgpack::dump_uint8(static_cast<uint8_t>(len), b, ix);
            });
         }
         else if (len <= std::numeric_limits<uint16_t>::max()) {
            dump_payload([&] {
               dump(std::byte{msgpack::ext16}, b, ix);
               msgpack::dump_uint16(static_cast<uint16_t>(len), b, ix);
            });
         }
         else if (len <= std::numeric_limits<uint32_t>::max()) {
            dump_payload([&] {
               dump(std::byte{msgpack::ext32}, b, ix);
               msgpack::dump_uint32(static_cast<uint32_t>(len), b, ix);
            });
         }
         else {
            ctx.error = error_code::exceeded_static_array_size;
         }
      }
   };

   // MessagePack timestamp extension (type -1)
   // Chooses the most compact format:
   // - Timestamp 32: when nanoseconds == 0 and seconds fits in uint32
   // - Timestamp 64: when seconds fits in 34 bits (0 to 17179869183)
   // - Timestamp 96: for all other cases (including negative seconds)
   template <>
   struct to<MSGPACK, msgpack::timestamp>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&&, B&& b, IX&& ix)
      {
         const auto type_byte = static_cast<uint8_t>(msgpack::timestamp_type);

         // Timestamp 32: seconds only, fits in uint32, no nanoseconds
         if (value.nanoseconds == 0 && value.seconds >= 0 &&
             value.seconds <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
            dump(std::byte{msgpack::fixext4}, b, ix);
            dump(std::byte{type_byte}, b, ix);
            msgpack::dump_uint32(static_cast<uint32_t>(value.seconds), b, ix);
         }
         // Timestamp 64: 30-bit nanoseconds + 34-bit seconds
         else if (value.seconds >= 0 && value.seconds <= 0x3FFFFFFFF) {
            dump(std::byte{msgpack::fixext8}, b, ix);
            dump(std::byte{type_byte}, b, ix);
            // Upper 30 bits: nanoseconds, lower 34 bits: seconds
            const uint64_t val64 = (static_cast<uint64_t>(value.nanoseconds) << 34) |
                                   static_cast<uint64_t>(value.seconds);
            msgpack::dump_uint64(val64, b, ix);
         }
         // Timestamp 96: full range with signed seconds
         else {
            dump(std::byte{msgpack::ext8}, b, ix);
            msgpack::dump_uint8(12, b, ix); // 12 bytes payload
            dump(std::byte{type_byte}, b, ix);
            msgpack::dump_uint32(value.nanoseconds, b, ix);
            msgpack::dump_uint64(static_cast<uint64_t>(value.seconds), b, ix);
         }
      }
   };

   // std::chrono::system_clock::time_point support
   // Converts to msgpack::timestamp for serialization
   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, std::chrono::system_clock::time_point>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using namespace std::chrono;
         const auto epoch = value.time_since_epoch();
         const auto secs = duration_cast<seconds>(epoch);
         const auto nsecs = duration_cast<nanoseconds>(epoch - secs);

         msgpack::timestamp ts;
         ts.seconds = secs.count();
         ts.nanoseconds = static_cast<uint32_t>(nsecs.count());

         to<MSGPACK, msgpack::timestamp>::template op<Opts>(ts, std::forward<Ctx>(ctx), std::forward<B>(b),
                                                            std::forward<IX>(ix));
      }
   };

   template <class T>
      requires(glaze_object_t<T> || writable_map_t<T> || reflectable<T>)
   struct to_partial<MSGPACK, T>
   {
      template <auto& Partial, auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         static constexpr auto sorted = sort_json_ptrs(Partial);
         static constexpr auto groups = glz::group_json_ptrs<sorted>();
         static constexpr auto N = glz::tuple_size_v<std::decay_t<decltype(groups)>>;

         msgpack::detail::write_map_header(N, b, ix);

         if constexpr (glaze_object_t<T>) {
            for_each<N>([&]<auto I>() {
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               static constexpr auto group = glz::get<I>(groups);
               static constexpr auto key = get<0>(group);
               static constexpr auto sub_partial = get<1>(group);
               static constexpr auto index = key_index<T>(key);
               static_assert(index < reflect<T>::size, "Invalid key passed to partial write");

               msgpack::detail::write_str_header(key.size(), b, ix);
               msgpack::detail::dump_raw_bytes(key.data(), key.size(), b, ix);
               if constexpr (glaze_object_t<T>) {
                  static constexpr auto member = get<index>(reflect<T>::values);
                  serialize_partial<MSGPACK>::op<sub_partial, Opts>(get_member(value, member), ctx, b, ix);
               }
               else {
                  serialize_partial<MSGPACK>::op<sub_partial, Opts>(get_member(value, get<index>(to_tie(value))), ctx,
                                                                    b, ix);
               }
            });
         }
         else if constexpr (writable_map_t<T>) {
            for_each<N>([&]<auto I>() {
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               static constexpr auto group = glz::get<I>(groups);
               static constexpr auto key_value = get<0>(group);
               static constexpr auto sub_partial = get<1>(group);

               if constexpr (findable<std::decay_t<Value>, decltype(key_value)>) {
                  serialize<MSGPACK>::op<Opts>(key_value, ctx, b, ix);
                  auto it = value.find(key_value);
                  if (it != value.end()) {
                     serialize_partial<MSGPACK>::op<sub_partial, Opts>(it->second, ctx, b, ix);
                  }
                  else {
                     ctx.error = error_code::invalid_partial_key;
                     return;
                  }
               }
               else {
                  static thread_local auto key =
                     typename std::decay_t<Value>::key_type(key_value); // TODO handle numeric pointer segments
                  serialize<MSGPACK>::op<Opts>(key, ctx, b, ix);
                  auto it = value.find(key);
                  if (it != value.end()) {
                     serialize_partial<MSGPACK>::op<sub_partial, Opts>(it->second, ctx, b, ix);
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

   template <glaze_array_t T>
      requires(!custom_write<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         static constexpr auto N = reflect<T>::size;
         msgpack::detail::write_array_header(N, b, ix);
         for_each<N>([&]<size_t I>() {
            serialize<MSGPACK>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, b, ix);
         });
      }
   };

   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         static constexpr auto N = glz::tuple_size_v<T>;
         msgpack::detail::write_array_header(N, b, ix);
         if constexpr (is_std_tuple<T>) {
            [&]<size_t... I>(std::index_sequence<I...>) {
               (serialize<MSGPACK>::op<Opts>(std::get<I>(value), ctx, b, ix), ...);
            }(std::make_index_sequence<N>{});
         }
         else {
            [&]<size_t... I>(std::index_sequence<I...>) {
               (serialize<MSGPACK>::op<Opts>(glz::get<I>(value), ctx, b, ix), ...);
            }(std::make_index_sequence<N>{});
         }
      }
   };

   template <is_variant T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         msgpack::detail::write_array_header(2, b, ix);
         static constexpr auto ids = ids_v<T>;
         msgpack::detail::write_str_header(ids[value.index()].size(), b, ix);
         msgpack::detail::dump_raw_bytes(ids[value.index()].data(), ids[value.index()].size(), b, ix);
         std::visit([&](auto&& v) { serialize<MSGPACK>::op<Opts>(v, ctx, b, ix); }, value);
      }
   };

   template <class T>
      requires is_specialization_v<T, arr>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V>;
         msgpack::detail::write_array_header(N, b, ix);
         for_each<N>([&]<size_t I>() {
            serialize<MSGPACK>::op<Opts>(glz::get<I>(value.value), ctx, b, ix);
         });
      }
   };

   template <class T>
      requires is_specialization_v<T, arr_copy>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V>;
         msgpack::detail::write_array_header(N, b, ix);
         for_each<N>([&]<size_t I>() {
            serialize<MSGPACK>::op<Opts>(glz::get<I>(value.value), ctx, b, ix);
         });
      }
   };

   template <class T>
      requires is_specialization_v<T, obj>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V> / 2;
         msgpack::detail::write_map_header(N, b, ix);
         for_each<N>([&]<size_t I>() {
            serialize<MSGPACK>::op<Opts>(glz::get<2 * I>(value.value), ctx, b, ix);
            serialize<MSGPACK>::op<Opts>(glz::get<2 * I + 1>(value.value), ctx, b, ix);
         });
      }
   };

   template <class T>
      requires is_specialization_v<T, obj_copy>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V> / 2;
         msgpack::detail::write_map_header(N, b, ix);
         for_each<N>([&]<size_t I>() {
            serialize<MSGPACK>::op<Opts>(glz::get<2 * I>(value.value), ctx, b, ix);
            serialize<MSGPACK>::op<Opts>(glz::get<2 * I + 1>(value.value), ctx, b, ix);
         });
      }
   };

   template <class T>
      requires is_includer<T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&... args)
      {
         msgpack::detail::write_str_header(0, std::forward<Args>(args)...);
      }
   };

   template <class T>
      requires func_t<T>
   struct to<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&&, B&& b, IX&& ix)
      {
         constexpr auto name = name_v<std::decay_t<decltype(value)>>;
         msgpack::detail::write_str_header(name.size(), b, ix);
         msgpack::detail::dump_raw_bytes(name.data(), name.size(), b, ix);
      }
   };

   template <write_supported<MSGPACK> T, output_buffer Buffer>
   [[nodiscard]] error_ctx write_msgpack(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = MSGPACK}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_supported<MSGPACK> T, raw_buffer Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> write_msgpack(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = MSGPACK}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_supported<MSGPACK> T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_msgpack(T&& value)
   {
      return write<opts{.format = MSGPACK}>(std::forward<T>(value));
   }

   template <auto& Partial, write_supported<MSGPACK> T, output_buffer Buffer>
   [[nodiscard]] error_ctx write_msgpack(T&& value, Buffer&& buffer)
   {
      return write<Partial, opts{.format = MSGPACK}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <auto& Partial, write_supported<MSGPACK> T, raw_buffer Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> write_msgpack(T&& value, Buffer&& buffer)
   {
      return write<Partial, opts{.format = MSGPACK}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <auto& Partial, write_supported<MSGPACK> T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_msgpack(T&& value)
   {
      std::string buffer{};
      const auto ec = write<Partial, opts{.format = MSGPACK}>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return glz::unexpected(ec);
      }
      return {buffer};
   }

   template <auto Opts = opts{}, write_supported<MSGPACK> T>
   [[nodiscard]] error_ctx write_file_msgpack(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = write<set_msgpack<Opts>()>(std::forward<T>(value), buffer);
      if (ec) {
         return ec;
      }
      return {buffer_to_file(buffer, file_name)};
   }
}
