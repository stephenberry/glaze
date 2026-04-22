// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.jsonb.write;

import std;

import glaze.json.generic;
import glaze.jsonb.header;

import glaze.concepts.container_concepts;

import glaze.core.buffer_traits;
import glaze.core.common;
import glaze.core.context;
import glaze.core.custom;
import glaze.core.meta;
import glaze.core.opts;
import glaze.core.reflect;
import glaze.core.to;
import glaze.core.write;
import glaze.core.write_chars;

import glaze.file.file_ops;

import glaze.reflection.to_tuple;

import glaze.util.expected;
import glaze.util.for_each;
import glaze.util.tuple;
import glaze.util.type_traits;
import glaze.util.variant;
import glaze.util.itoa;
import glaze.util.string_literal;

import glaze.tuplet;

#include "glaze/util/inline.hpp"

using std::int64_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::size_t;

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
         b[ix] =
            static_cast<typename std::decay_t<decltype(b)>::value_type>(jsonb::make_initial(jsonb::type::null_, 0));
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
         const uint8_t byte =
            value ? jsonb::make_initial(jsonb::type::true_, 0) : jsonb::make_initial(jsonb::type::false_, 0);
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

   namespace jsonb_detail
   {
      // True if any byte in [data, data+size) is a control character (<0x20), an unescaped
      // double quote, or an unescaped backslash — i.e. the payload could not stand as the
      // body of a JSON string literal without modification.
      GLZ_ALWAYS_INLINE bool string_needs_json_escape(const char* data, size_t size) noexcept
      {
         for (size_t i = 0; i < size; ++i) {
            const auto c = static_cast<uint8_t>(data[i]);
            if (c < 0x20 || c == '"' || c == '\\') {
               return true;
            }
         }
         return false;
      }
   }

   // Text strings (UTF-8). Pre-scan to pick the tightest spec-conformant type:
   //   * TEXT (7)    — payload is already a valid JSON string body, so downstream JSON
   //                   emitters can just wrap it in quotes and memcpy.
   //   * TEXTRAW (10) — payload contains bytes that would require JSON escaping.
   // This costs one O(n) byte scan on write but spares every future JSON emission (SQLite's
   // `json()` and Glaze's own converter included) from scanning the payload.
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

         const uint8_t tc =
            jsonb_detail::string_needs_json_escape(str.data(), str.size()) ? jsonb::type::textraw : jsonb::type::text;
         jsonb_detail::write_scalar(ctx, tc, str.data(), str.size(), b, ix);
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

   namespace jsonb_detail
   {
      template <class T, auto Opts, size_t I>
      consteval bool should_skip_reflected_field()
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

      // Write the (key, value) pairs of a reflected struct without an enclosing OBJECT
      // header. Used both by the reflected writer (which wraps the call with header
      // reservation/patching) and by the tagged-variant writer (which writes a tag pair
      // first, then this body).
      template <auto Opts, class T, class B, class IX>
      void write_reflected_body(T&& value, is_context auto& ctx, B&& b, IX& ix)
      {
         using DT = std::remove_cvref_t<T>;
         static constexpr auto N = reflect<DT>::size;
         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<DT>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;
            if constexpr (should_skip_reflected_field<DT, Opts, I>()) {
               return;
            }
            else {
               using val_t = field_t<DT, I>;

               decltype(auto) member = [&]() -> decltype(auto) {
                  if constexpr (reflectable<DT>) {
                     return get<I>(t);
                  }
                  else {
                     return get<I>(reflect<DT>::values);
                  }
               }();

               if constexpr (Opts.skip_null_members && null_t<val_t>) {
                  if constexpr (always_null_t<val_t>) {
                     return;
                  }
                  else {
                     const auto is_null = [&]() {
                        if constexpr (nullable_wrapper<val_t>) {
                           return !bool(member(value).val);
                        }
                        else if constexpr (nullable_value_t<val_t>) {
                           return !get_member(value, member).has_value();
                        }
                        else {
                           return !bool(get_member(value, member));
                        }
                     }();
                     if (is_null) return;
                  }
               }
               else if constexpr (is_specialization_v<val_t, custom_t> && Opts.skip_null_members &&
                                  custom_getter_returns_nullable<val_t>()) {
                  if (is_custom_field_null<DT, I>(value, t, ctx)) return;
               }
               else if constexpr (Opts.skip_null_members && glaze_value_is_nullable<val_t>()) {
                  if (is_glaze_value_field_null<DT, I>(value, t)) return;
               }

               if constexpr (check_skip_default_members(Opts) && has_skippable_default<val_t>) {
                  if (is_default_value(get_member(value, member))) return;
               }

               static constexpr sv key = reflect<DT>::keys[I];
               jsonb_detail::write_scalar(ctx, jsonb::type::text, key.data(), key.size(), b, ix);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               serialize<JSONB>::op<Opts>(get_member(value, member), ctx, b, ix);
            }
         });
      }
   }

   // Reflected objects / glaze-meta structs
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_write<T>)
   struct to<JSONB, T> final
   {
      static constexpr auto N = reflect<T>::size;

      template <auto Opts, size_t I>
      static consteval bool should_skip_field()
      {
         return jsonb_detail::should_skip_reflected_field<T, Opts, I>();
      }

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         size_t header_pos{};
         if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
            return;
         }
         const size_t payload_start = ix;

         jsonb_detail::write_reflected_body<Opts>(value, ctx, b, ix);
         if (bool(ctx.error)) [[unlikely]]
            return;

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

   // Variants — serialize the active alternative directly, mirroring Glaze's JSON
   // convention. The reader deduces which alternative was written from the JSONB
   // type code byte (NULL/bool/INT/FLOAT/TEXT*/ARRAY/OBJECT), so no [index, value]
   // wrapper is needed. JSONB's type granularity is finer than JSON's (INT and
   // FLOAT are distinct), which lets variant<int, double> auto-deduce here even
   // though it cannot in JSON.
   //
   // When the variant has a non-empty `tag_v<T>` and the active alternative is a
   // reflected struct, the writer wraps it as `{tag_v: id, ...inlined fields}`
   // (Glaze's JSON tagged-variant convention). The reader expects the tag to be
   // the first key.
   template <is_variant T>
   struct to<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         std::visit(
            [&](auto&& v) {
               using V = std::decay_t<decltype(v)>;
               constexpr bool is_reflected_object = glaze_object_t<V> || reflectable<V>;
               if constexpr (check_write_type_info(Opts) && not tag_v<T>.empty() && is_reflected_object) {
                  size_t header_pos{};
                  if (!jsonb_detail::reserve_container_header(ctx, b, ix, header_pos)) [[unlikely]] {
                     return;
                  }
                  const size_t payload_start = ix;

                  // Write the tag key + id pair first, then the alternative's fields inline.
                  static constexpr auto tag = tag_v<T>;
                  jsonb_detail::write_scalar(ctx, jsonb::type::text, tag.data(), tag.size(), b, ix);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  using id_type = std::decay_t<decltype(ids_v<T>[0])>;
                  if constexpr (std::integral<id_type>) {
                     to<JSONB, id_type>::template op<Opts>(ids_v<T>[value.index()], ctx, b, ix);
                  }
                  else {
                     // String id (typically a struct name); emit as a TEXT element.
                     const sv id = ids_v<T>[value.index()];
                     to<JSONB, sv>::template op<Opts>(id, ctx, b, ix);
                  }
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  jsonb_detail::write_reflected_body<Opts>(v, ctx, b, ix);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  const uint64_t payload_size = static_cast<uint64_t>(ix - payload_start);
                  jsonb::patch_header_9(b, header_pos, jsonb::type::object, payload_size);
               }
               else {
                  serialize<JSONB>::op<Opts>(v, ctx, b, ix);
               }
            },
            value);
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

   export template <write_supported<JSONB> T, class Buffer>
   [[nodiscard]] error_ctx write_jsonb(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = JSONB}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   export template <auto Opts = opts{}, write_supported<JSONB> T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_jsonb(T&& value)
   {
      return write<set_jsonb<Opts>()>(std::forward<T>(value));
   }

   export template <auto Opts = opts{}, write_supported<JSONB> T>
   [[nodiscard]] inline error_code write_file_jsonb(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = write<set_jsonb<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec.ec;
      }
      return buffer_to_file(buffer, file_name);
   }
}
