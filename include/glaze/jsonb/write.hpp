// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "glaze/core/buffer_traits.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/to.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
#include "glaze/json/generic.hpp"
#include "glaze/jsonb/header.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   template <>
   struct serialize<JSONB>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<JSONB, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                              std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   namespace jsonb_detail
   {
      // Write a scalar element whose payload is already known (type + exact bytes).
      // Emits a minimal header followed by the payload bytes.
      template <class B, class IX>
      GLZ_ALWAYS_INLINE bool write_scalar(is_context auto& ctx, uint8_t type_code, const char* data, size_t size, B& b,
                                          IX& ix)
      {
         const size_t hdr_bytes = jsonb::header_bytes_for_payload(size);
         if (!ensure_space(ctx, b, ix + hdr_bytes + size + write_padding_bytes)) [[unlikely]] {
            return false;
         }
         jsonb::write_header(type_code, size, b, ix);
         if (size) {
            std::memcpy(&b[ix], data, size);
            ix += size;
         }
         return true;
      }

      // Reserve a 9-byte container header at the current ix. Returns the position of the
      // reserved slot; advances ix past it. Later callers patch with patch_header_9.
      template <class B, class IX>
      GLZ_ALWAYS_INLINE bool reserve_container_header(is_context auto& ctx, B& b, IX& ix, size_t& header_pos)
      {
         if (!ensure_space(ctx, b, ix + jsonb::max_header_bytes + write_padding_bytes)) [[unlikely]] {
            return false;
         }
         header_pos = ix;
         ix += jsonb::max_header_bytes;
         return true;
      }
   }

   // Null
   template <always_null_t T>
   struct to<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 1 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         b[ix] = static_cast<typename std::decay_t<decltype(b)>::value_type>(jsonb::make_initial(jsonb::type::null_, 0));
         ++ix;
      }
   };

   // Boolean
   template <boolean_like T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 1 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         const uint8_t byte = value ? jsonb::make_initial(jsonb::type::true_, 0) : jsonb::make_initial(jsonb::type::false_, 0);
         b[ix] = static_cast<typename std::decay_t<decltype(b)>::value_type>(byte);
         ++ix;
      }
   };

   // Integer (signed + unsigned)
   template <class T>
      requires(std::integral<T> && !std::same_as<T, bool>)
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         // int64_t max is 20 chars including sign. uint64_t max is 20 chars.
         std::array<char, 32> tmp;
         auto* end_ptr = glz::to_chars(tmp.data(), static_cast<T>(value));
         const size_t n = static_cast<size_t>(end_ptr - tmp.data());
         jsonb_detail::write_scalar(ctx, jsonb::type::int_, tmp.data(), n, b, ix);
      }
   };

   // Floating point
   template <std::floating_point T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         const double d = static_cast<double>(value);

         // Handle non-finite specially: JSON5 allows NaN / Infinity / -Infinity.
         if (!std::isfinite(d)) {
            if (std::isnan(d)) {
               static constexpr char s[] = "NaN";
               jsonb_detail::write_scalar(ctx, jsonb::type::float5, s, 3, b, ix);
            }
            else if (d > 0) {
               static constexpr char s[] = "Infinity";
               jsonb_detail::write_scalar(ctx, jsonb::type::float5, s, 8, b, ix);
            }
            else {
               static constexpr char s[] = "-Infinity";
               jsonb_detail::write_scalar(ctx, jsonb::type::float5, s, 9, b, ix);
            }
            return;
         }

         // write_chars guarantees it only needs 64 chars for any float. Reserve a 2-byte
         // header (sufficient for any payload ≤ 255 bytes) plus the number bytes, write the
         // number, then patch the header and shift the payload left by 1 byte if we ended up
         // with ≤ 11 characters.
         constexpr size_t kReserve = 2 + 64 + write_padding_bytes;
         if (!ensure_space(ctx, b, ix + kReserve)) [[unlikely]] {
            return;
         }
         const size_t num_start = ix + 2;
         size_t tmp_ix = num_start;
         write_chars::op<opts{.format = JSON}>(d, ctx, b, tmp_ix);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         const size_t n = tmp_ix - num_start;
         if (n <= 11) {
            // Use 1-byte inline header, shift payload 1 byte left.
            b[ix] = static_cast<typename std::decay_t<decltype(b)>::value_type>(
               jsonb::make_initial(jsonb::type::float_, static_cast<uint8_t>(n)));
            std::memmove(&b[ix + 1], &b[num_start], n);
            ix = ix + 1 + n;
         }
         else {
            // 2-byte header (u8_follows).
            b[ix] = static_cast<typename std::decay_t<decltype(b)>::value_type>(
               jsonb::make_initial(jsonb::type::float_, jsonb::size_code::u8_follows));
            b[ix + 1] = static_cast<typename std::decay_t<decltype(b)>::value_type>(n);
            ix = num_start + n;
         }
      }
   };

   // Text strings (UTF-8)
   template <str_t T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         const sv str = [&]() -> const sv {
            if constexpr (!char_array_t<T> && std::is_pointer_v<std::decay_t<T>>) {
               return value ? value : "";
            }
            else {
               return sv{value};
            }
         }();

         // Emit as TEXTRAW: the bytes are raw UTF-8 as the C++ program holds them.
         // A consumer rendering to JSON must escape them.
         jsonb_detail::write_scalar(ctx, jsonb::type::textraw, str.data(), str.size(), b, ix);
      }
   };

   // Arrays, vectors, ranges (writable arrays)
   template <writable_array_t T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         for (auto&& item : value) {
            serialize<JSONB>::op<Opts>(item, ctx, b, ix);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }

         const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
         jsonb::patch_header_9(b, header_pos, jsonb::type::array, payload_size);
      }
   };

   // Tuples
   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         static constexpr auto N = glz::tuple_size_v<T>;
         if constexpr (is_std_tuple<T>) {
            [&]<size_t... I>(std::index_sequence<I...>) {
               (serialize<JSONB>::op<Opts>(std::get<I>(value), ctx, b, ix), ...);
            }(std::make_index_sequence<N>{});
         }
         else {
            [&]<size_t... I>(std::index_sequence<I...>) {
               (serialize<JSONB>::op<Opts>(glz::get<I>(value), ctx, b, ix), ...);
            }(std::make_index_sequence<N>{});
         }

         const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
         jsonb::patch_header_9(b, header_pos, jsonb::type::array, payload_size);
      }
   };

   // Maps (std::map, std::unordered_map) — keys must serialize as string types.
   template <writable_map_t T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         for (auto&& [k, v] : value) {
            serialize<JSONB>::op<Opts>(k, ctx, b, ix);
            if (bool(ctx.error)) [[unlikely]]
               return;
            serialize<JSONB>::op<Opts>(v, ctx, b, ix);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }

         const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
         jsonb::patch_header_9(b, header_pos, jsonb::type::object, payload_size);
      }
   };

   // Pairs — serialized as a single-entry object for consistency with JSON / CBOR.
   template <pair_t T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         const auto& [k, v] = value;
         serialize<JSONB>::op<Opts>(k, ctx, b, ix);
         if (bool(ctx.error)) [[unlikely]]
            return;
         serialize<JSONB>::op<Opts>(v, ctx, b, ix);
         if (bool(ctx.error)) [[unlikely]]
            return;

         const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
         jsonb::patch_header_9(b, header_pos, jsonb::type::object, payload_size);
      }
   };

   // Reflected objects / glaze-meta structs
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_write<T>)
   struct to<JSONB, T> final
   {
      static constexpr auto N = reflect<T>::size;

      template <auto Opts, size_t I>
      static consteval bool should_skip_field()
      {
         using V = field_t<T, I>;
         if constexpr (std::same_as<V, hidden> || std::same_as<V, skip>) {
            return true;
         }
         else if constexpr (is_any_function_ptr<V>) {
            return !check_write_function_pointers(Opts);
         }
         else {
            return false;
         }
      }

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;
            if constexpr (should_skip_field<Opts, I>()) {
               return;
            }
            else {
               static constexpr sv key = reflect<T>::keys[I];

               // Write key as TEXTRAW (reflected keys are raw identifier bytes).
               jsonb_detail::write_scalar(ctx, jsonb::type::textraw, key.data(), key.size(), b, ix);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               decltype(auto) member = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return get<I>(t);
                  }
                  else {
                     return get<I>(reflect<T>::values);
                  }
               }();

               serialize<JSONB>::op<Opts>(get_member(value, member), ctx, b, ix);
            }
         });

         const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
         jsonb::patch_header_9(b, header_pos, jsonb::type::object, payload_size);
      }
   };

   // glaze_array_t: tuple-like view
   template <class T>
      requires glaze_array_t<T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         static constexpr auto Nf = reflect<T>::size;
         for_each<Nf>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;
            serialize<JSONB>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, b, ix);
         });

         const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
         jsonb::patch_header_9(b, header_pos, jsonb::type::array, payload_size);
      }
   };

   // Optionals / unique_ptr / shared_ptr
   template <nullable_like T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if (value) {
            serialize<JSONB>::op<Opts>(*value, ctx, b, ix);
         }
         else {
            if (!ensure_space(ctx, b, ix + 1 + write_padding_bytes)) [[unlikely]] {
               return;
            }
            b[ix] =
               static_cast<typename std::decay_t<decltype(b)>::value_type>(jsonb::make_initial(jsonb::type::null_, 0));
            ++ix;
         }
      }
   };

   // nullable_value_t (e.g. std::expected without error)
   template <class T>
      requires(nullable_value_t<T> && !nullable_like<T> && !is_expected<T>)
   struct to<JSONB, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if (value.has_value()) {
            serialize<JSONB>::op<Opts>(value.value(), ctx, b, ix);
         }
         else {
            if (!ensure_space(ctx, b, ix + 1 + write_padding_bytes)) [[unlikely]] {
               return;
            }
            b[ix] =
               static_cast<typename std::decay_t<decltype(b)>::value_type>(jsonb::make_initial(jsonb::type::null_, 0));
            ++ix;
         }
      }
   };

   // C-style arrays — serialize as spans
   template <nullable_t T>
      requires(std::is_array_v<T>)
   struct to<JSONB, T>
   {
      template <auto Opts, class V, size_t N>
      GLZ_ALWAYS_INLINE static void op(const V (&value)[N], is_context auto&& ctx, auto&& b, auto& ix)
      {
         serialize<JSONB>::op<Opts>(std::span{value, N}, ctx, b, ix);
      }
   };

   // std::expected — serialize the value on success; on error, wrap as {"unexpected": <error>}
   template <is_expected T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if (value) {
            if constexpr (not std::is_void_v<typename std::decay_t<T>::value_type>) {
               serialize<JSONB>::op<Opts>(*value, ctx, b, ix);
            }
            else {
               // void value type on success → emit empty object
               size_t header_pos{};
               if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) return;
               jsonb::patch_header_9(b, header_pos, jsonb::type::object, 0);
            }
         }
         else {
            serialize<JSONB>::op<Opts>(unexpected_wrapper{&value.error()}, ctx, b, ix);
         }
      }
   };

   // Variants — serialize as [index, value] array (parallels CBOR encoding)
   template <is_variant T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         using Variant = std::decay_t<decltype(value)>;

         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         std::visit(
            [&](auto&& v) {
               using V = std::decay_t<decltype(v)>;
               static constexpr uint64_t index = []<size_t... I>(std::index_sequence<I...>) {
                  return ((std::is_same_v<V, std::variant_alternative_t<I, Variant>> * I) + ...);
               }(std::make_index_sequence<std::variant_size_v<Variant>>{});

               to<JSONB, uint64_t>::template op<Opts>(index, ctx, b, ix);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               serialize<JSONB>::op<Opts>(v, ctx, b, ix);
            },
            value);

         const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
         jsonb::patch_header_9(b, header_pos, jsonb::type::array, payload_size);
      }
   };

   // Enums (reflected and plain) — serialize underlying integer
   template <glaze_enum_t T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         using V = std::underlying_type_t<std::decay_t<T>>;
         to<JSONB, V>::template op<Opts>(static_cast<V>(value), ctx, b, ix);
      }
   };

   template <class T>
      requires(std::is_enum_v<T> && !glaze_enum_t<T>)
   struct to<JSONB, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         using V = std::underlying_type_t<std::decay_t<T>>;
         to<JSONB, V>::template op<Opts>(static_cast<V>(value), ctx, b, ix);
      }
   };

   // Glaze value wrapper
   template <class T>
      requires(glaze_value_t<T> && !custom_write<T>)
   struct to<JSONB, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::remove_cvref_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         to<JSONB, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                         std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   // Member function pointers (no-op)
   template <is_member_function_pointer T>
   struct to<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&, auto&&) noexcept
      {}
   };

   // Generic JSON value — dispatch each variant alternative to the matching JSONB element.
   // Without this specialization, glz::generic would fall through to the variant handler and
   // serialize as an [index, value] array rather than as the native element type.
   template <num_mode Mode, template <class> class MapType>
   struct to<JSONB, generic_json<Mode, MapType>> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         using G = std::decay_t<decltype(value)>;
         using array_t = typename G::array_t;
         using object_t = typename G::object_t;

         std::visit(
            [&](auto&& v) {
               using V = std::decay_t<decltype(v)>;
               if constexpr (std::same_as<V, std::nullptr_t>) {
                  to<JSONB, std::nullptr_t>::template op<Opts>(nullptr, ctx, b, ix);
               }
               else if constexpr (std::same_as<V, bool>) {
                  to<JSONB, bool>::template op<Opts>(v, ctx, b, ix);
               }
               else if constexpr (std::same_as<V, std::string>) {
                  to<JSONB, std::string>::template op<Opts>(v, ctx, b, ix);
               }
               else if constexpr (std::same_as<V, array_t>) {
                  // Container dispatch: array of generic_json — handled by writable_array_t.
                  to<JSONB, array_t>::template op<Opts>(v, ctx, b, ix);
               }
               else if constexpr (std::same_as<V, object_t>) {
                  // Map<string, generic_json> — writable_map_t handles it.
                  to<JSONB, object_t>::template op<Opts>(v, ctx, b, ix);
               }
               else {
                  // Numeric types: uint64_t, int64_t, double depending on Mode.
                  to<JSONB, V>::template op<Opts>(v, ctx, b, ix);
               }
            },
            value.data);
      }
   };

   // ===== High-level write APIs =====

   template <write_supported<JSONB> T, class Buffer>
   [[nodiscard]] error_ctx write_jsonb(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = JSONB}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <auto Opts = opts{}, write_supported<JSONB> T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_jsonb(T&& value)
   {
      return write<set_jsonb<Opts>()>(std::forward<T>(value));
   }

   template <auto Opts = opts{}, write_supported<JSONB> T>
   [[nodiscard]] inline error_code write_file_jsonb(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = write<set_jsonb<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec.ec;
      }
      return buffer_to_file(buffer, file_name);
   }
}
