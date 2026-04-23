// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include "glaze/bson/header.hpp"
#include "glaze/core/buffer_traits.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/to.hpp"
#include "glaze/core/write.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/uuid.hpp"

// BSON writer — https://bsonspec.org/spec.html
//
// BSON's top-level container is always a document:
//     int32 total_length | element* | 0x00
// where `total_length` counts itself and the terminating null byte, and each
// element is:
//     type_byte | key cstring | value bytes
//
// Because the document length precedes its body, the writer reserves 4 bytes
// up front, emits the body, emits the null terminator, then patches the
// reserved slot with the computed length. Arrays are encoded as documents
// whose keys are the decimal strings "0", "1", "2", ...
//
// All multi-byte integers and floats on the wire are little-endian.
//
// Caveat on cstring fields (BSON spec prohibits embedded 0x00):
//   - std::map<std::string, T> keys
//   - bson::regex::pattern and bson::regex::options
// Strings passed in these fields must not contain an embedded NUL; otherwise
// the resulting wire bytes are malformed and a downstream read will terminate
// the cstring at the first NUL and fail with a syntax_error when the trailing
// bytes no longer parse as a valid element stream. Struct field names are
// safe by construction — they come from reflected identifiers.
//
// Caveat on nested optionals: std::optional<std::optional<T>> loses the
// outer/inner distinction on the wire. Both `nullopt` and `optional{nullopt}`
// encode as a single null element, so reads cannot tell them apart. This is
// inherent to the tagged-null encoding (same limitation in JSON, CBOR, etc.).

namespace glz
{
   namespace bson_detail
   {
      // --- Little-endian byte writers -----------------------------------------
      //
      // On little-endian hosts (x86, ARM64) a std::memcpy of the native integer
      // is already in wire order. On big-endian hosts we byteswap first.

      template <std::integral T, class B>
      GLZ_ALWAYS_INLINE void dump_le(T value, B& b, size_t& ix) noexcept
      {
         if constexpr (std::endian::native == std::endian::big) {
            value = static_cast<T>(std::byteswap(static_cast<std::make_unsigned_t<T>>(value)));
         }
         std::memcpy(&b[ix], &value, sizeof(T));
         ix += sizeof(T);
      }

      // Patch a little-endian integer into a previously reserved slot without
      // advancing `pos`. Used to backfill document lengths.
      template <std::integral T, class B>
      GLZ_ALWAYS_INLINE void patch_le(T value, B& b, size_t pos) noexcept
      {
         if constexpr (std::endian::native == std::endian::big) {
            value = static_cast<T>(std::byteswap(static_cast<std::make_unsigned_t<T>>(value)));
         }
         std::memcpy(&b[pos], &value, sizeof(T));
      }

      template <class B>
      GLZ_ALWAYS_INLINE void dump_le_double(double value, B& b, size_t& ix) noexcept
      {
         const uint64_t bits = std::bit_cast<uint64_t>(value);
         dump_le<uint64_t>(bits, b, ix);
      }

      // --- Raw byte writers ----------------------------------------------------

      template <class B>
      GLZ_ALWAYS_INLINE void put_byte(uint8_t byte, B& b, size_t& ix) noexcept
      {
         using V = typename std::decay_t<B>::value_type;
         b[ix] = static_cast<V>(byte);
         ++ix;
      }

      template <class B>
      GLZ_ALWAYS_INLINE void put_bytes(const void* src, size_t n, B& b, size_t& ix) noexcept
      {
         if (n) {
            std::memcpy(&b[ix], src, n);
            ix += n;
         }
      }

      // --- Document lifecycle --------------------------------------------------

      // Reserve 4 bytes for the int32 length field and record the starting ix.
      template <class B>
      GLZ_ALWAYS_INLINE bool reserve_document_length(is_context auto& ctx, B& b, size_t& ix, size_t& start) noexcept
      {
         if (!ensure_space(ctx, b, ix + 4 + write_padding_bytes)) [[unlikely]] {
            return false;
         }
         start = ix;
         ix += 4;
         return true;
      }

      // Emit the terminating null byte and backfill the int32 length.
      template <class B>
      GLZ_ALWAYS_INLINE bool finalize_document(is_context auto& ctx, B& b, size_t& ix, size_t start) noexcept
      {
         if (!ensure_space(ctx, b, ix + 1 + write_padding_bytes)) [[unlikely]] {
            return false;
         }
         put_byte(0, b, ix);
         const size_t total = ix - start;
         if (total > static_cast<size_t>(std::numeric_limits<int32_t>::max())) [[unlikely]] {
            ctx.error = error_code::invalid_length;
            return false;
         }
         patch_le<int32_t>(static_cast<int32_t>(total), b, start);
         return true;
      }

      // --- Element prefix: type_byte | key cstring | 0x00 ---------------------

      template <class B>
      GLZ_ALWAYS_INLINE bool write_element_prefix(is_context auto& ctx, uint8_t type_byte, std::string_view key, B& b,
                                                  size_t& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 2 + key.size() + write_padding_bytes)) [[unlikely]] {
            return false;
         }
         put_byte(type_byte, b, ix);
         put_bytes(key.data(), key.size(), b, ix);
         put_byte(0, b, ix);
         return true;
      }

      // --- BSON string value: int32(len+1) | UTF-8 | 0x00 ---------------------

      template <class B>
      GLZ_ALWAYS_INLINE bool dump_string_value(is_context auto& ctx, std::string_view str, B& b, size_t& ix) noexcept
      {
         // The length field stores byte count of the UTF-8 payload PLUS the
         // trailing null byte, per spec.
         if (str.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()) - 1) [[unlikely]] {
            ctx.error = error_code::invalid_length;
            return false;
         }
         if (!ensure_space(ctx, b, ix + 4 + str.size() + 1 + write_padding_bytes)) [[unlikely]] {
            return false;
         }
         const int32_t len = static_cast<int32_t>(str.size() + 1);
         dump_le<int32_t>(len, b, ix);
         put_bytes(str.data(), str.size(), b, ix);
         put_byte(0, b, ix);
         return true;
      }

      // --- Array key emission: decimal string of index, null-terminated -------

      // For small indices a static string table is used; larger indices fall
      // back to std::to_chars. Each element is a null-terminated C string
      // suitable for a BSON e_name. 1024 covers nearly all real-world array
      // sizes at ~5KB of .rodata, keeping the hot path branch-free.
      inline constexpr auto make_small_array_keys()
      {
         std::array<std::array<char, 5>, 1024> t{};
         for (int i = 0; i < 1024; ++i) {
            if (i < 10) {
               t[i][0] = static_cast<char>('0' + i);
               t[i][1] = '\0';
            }
            else if (i < 100) {
               t[i][0] = static_cast<char>('0' + i / 10);
               t[i][1] = static_cast<char>('0' + i % 10);
               t[i][2] = '\0';
            }
            else if (i < 1000) {
               t[i][0] = static_cast<char>('0' + i / 100);
               t[i][1] = static_cast<char>('0' + (i / 10) % 10);
               t[i][2] = static_cast<char>('0' + i % 10);
               t[i][3] = '\0';
            }
            else {
               t[i][0] = static_cast<char>('0' + i / 1000);
               t[i][1] = static_cast<char>('0' + (i / 100) % 10);
               t[i][2] = static_cast<char>('0' + (i / 10) % 10);
               t[i][3] = static_cast<char>('0' + i % 10);
               t[i][4] = '\0';
            }
         }
         return t;
      }

      inline constexpr auto small_array_keys = make_small_array_keys();

      // Returns the key bytes for the given index as a string_view over a
      // caller-owned scratch buffer or the precomputed table. The returned
      // string_view does not include the null terminator.
      GLZ_ALWAYS_INLINE std::string_view array_key(size_t index, std::array<char, 24>& scratch) noexcept
      {
         if (index < small_array_keys.size()) {
            const auto& entry = small_array_keys[index];
            size_t n = 0;
            while (entry[n] != '\0') ++n;
            return std::string_view{entry.data(), n};
         }
         auto [end, ec] = std::to_chars(scratch.data(), scratch.data() + scratch.size(), index);
         return std::string_view{scratch.data(), static_cast<size_t>(end - scratch.data())};
      }

      // --- Skip logic (ported from JSONB) ------------------------------------

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
   } // namespace bson_detail

   // Dispatcher — routes top-level writes through to<BSON, T>::op.
   template <>
   struct serialize<BSON>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<BSON, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                             std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   // ==========================================================================
   // Value-only writers: to<BSON, T>::op writes just the value bytes (no type
   // byte, no key). Each specialization also exposes a static constexpr
   // `type_code` member giving the BSON element type byte for that T, which
   // the enclosing document/array writer emits before the key and value.
   //
   // For types with runtime-varying type codes (std::optional, std::variant),
   // `type_code` is not defined; the member-element helper dispatches those
   // cases explicitly.
   // ==========================================================================

   // --- Boolean --------------------------------------------------------------
   template <>
   struct to<BSON, bool>
   {
      static constexpr uint8_t type_code = bson::type::boolean;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(bool value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 1 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         bson_detail::put_byte(value ? 1 : 0, b, ix);
      }
   };

   // --- Integers -------------------------------------------------------------
   //
   // BSON has two integer element types: int32 (0x10) and int64 (0x12). We pick
   // based on the C++ type's range, not the runtime value, so the encoding is
   // deterministic from the schema alone.
   //
   //   int8/int16/int32, uint8/uint16           → int32
   //   uint32, int64, uint64                    → int64 (uint64 overflow → error)
   //
   // uint32 goes to int64 because UINT32_MAX exceeds INT32_MAX; emitting int32
   // would silently truncate values in 2^31..2^32-1. This differs from the
   // MongoDB shell default (NumberInt for anything 32-bit wide), so drivers or
   // tools inspecting the wire output will see int64 where they may expect
   // int32. The round-trip is lossless and the encoding stays type-driven.
   template <class T>
      requires(std::integral<T> && !std::same_as<T, bool>)
   struct to<BSON, T>
   {
      static constexpr bool fits_int32 = (sizeof(T) <= 2) || (sizeof(T) == 4 && std::is_signed_v<T>);
      static constexpr uint8_t type_code = fits_int32 ? bson::type::int32 : bson::type::int64;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if constexpr (fits_int32) {
            if (!ensure_space(ctx, b, ix + 4 + write_padding_bytes)) [[unlikely]] {
               return;
            }
            bson_detail::dump_le<int32_t>(static_cast<int32_t>(value), b, ix);
         }
         else {
            if (!ensure_space(ctx, b, ix + 8 + write_padding_bytes)) [[unlikely]] {
               return;
            }
            if constexpr (std::same_as<T, uint64_t>) {
               if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            bson_detail::dump_le<int64_t>(static_cast<int64_t>(value), b, ix);
         }
      }
   };

   // --- Floating point -------------------------------------------------------
   //
   // BSON has only `double` (type 0x01). `float` promotes without loss.
   template <std::floating_point T>
   struct to<BSON, T>
   {
      static constexpr uint8_t type_code = bson::type::double_;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 8 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         bson_detail::dump_le_double(static_cast<double>(value), b, ix);
      }
   };

   // --- Enums ----------------------------------------------------------------
   //
   // Always emitted as the underlying integer. `Opts.reflect_enums` is
   // intentionally not honored — JSON (which does honor it and emits the
   // reflected name as a string) and BSON will therefore produce different
   // wire shapes from the same shared `opts`. This is a deliberate interop
   // choice, not a spec constraint: BSON strings (type 0x02) can carry any
   // UTF-8 name, but MongoDB and other driver ecosystems expect integer-
   // backed enums on the wire, and emitting names here would silently
   // break cross-driver round-trips.
   template <class T>
      requires(std::is_enum_v<T>)
   struct to<BSON, T>
   {
      using U = std::underlying_type_t<T>;
      static constexpr uint8_t type_code = to<BSON, U>::type_code;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         to<BSON, U>::template op<Opts>(static_cast<U>(value), ctx, b, ix);
      }
   };

   // --- Strings --------------------------------------------------------------
   //
   // BSON string value layout: int32(byte_count + 1) | UTF-8 bytes | 0x00
   template <str_t T>
   struct to<BSON, T>
   {
      static constexpr uint8_t type_code = bson::type::string;

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         const std::string_view str = [&]() -> std::string_view {
            if constexpr (!char_array_t<T> && std::is_pointer_v<std::decay_t<decltype(value)>>) {
               return value ? std::string_view{value} : std::string_view{};
            }
            else {
               return std::string_view{value};
            }
         }();
         (void)bson_detail::dump_string_value(ctx, str, b, ix);
      }
   };

   // --- BSON-native helper types --------------------------------------------

   template <>
   struct to<BSON, bson::object_id>
   {
      static constexpr uint8_t type_code = bson::type::object_id;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bson::object_id& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 12 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         bson_detail::put_bytes(value.bytes.data(), 12, b, ix);
      }
   };

   template <>
   struct to<BSON, bson::datetime>
   {
      static constexpr uint8_t type_code = bson::type::datetime;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bson::datetime& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 8 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         bson_detail::dump_le<int64_t>(value.ms_since_epoch, b, ix);
      }
   };

   template <>
   struct to<BSON, bson::timestamp>
   {
      static constexpr uint8_t type_code = bson::type::timestamp;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bson::timestamp& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 8 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         // BSON timestamp wire format: low 32 bits = increment, high 32 bits = seconds.
         bson_detail::dump_le<uint32_t>(value.increment, b, ix);
         bson_detail::dump_le<uint32_t>(value.seconds, b, ix);
      }
   };

   template <>
   struct to<BSON, bson::regex>
   {
      static constexpr uint8_t type_code = bson::type::regex;

      template <auto Opts>
      static void op(const bson::regex& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + value.pattern.size() + value.options.size() + 2 + write_padding_bytes))
            [[unlikely]] {
            return;
         }
         bson_detail::put_bytes(value.pattern.data(), value.pattern.size(), b, ix);
         bson_detail::put_byte(0, b, ix);
         // BSON spec requires regex options be stored in alphabetical order.
         // std::sort works on the raw buffer regardless of element type: char,
         // uint8_t, and std::byte all expose ordering (std::byte via its scoped
         // enum underlying_type, per C++20), which matches ASCII lex order.
         const auto opts_start = ix;
         bson_detail::put_bytes(value.options.data(), value.options.size(), b, ix);
         std::sort(&b[opts_start], &b[ix]);
         bson_detail::put_byte(0, b, ix);
      }
   };

   template <>
   struct to<BSON, bson::javascript>
   {
      static constexpr uint8_t type_code = bson::type::javascript;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bson::javascript& value, is_context auto&& ctx, auto&& b,
                                       auto& ix) noexcept
      {
         (void)bson_detail::dump_string_value(ctx, value.code, b, ix);
      }
   };

   template <>
   struct to<BSON, bson::decimal128>
   {
      static constexpr uint8_t type_code = bson::type::decimal128;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bson::decimal128& value, is_context auto&& ctx, auto&& b,
                                       auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 16 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         bson_detail::put_bytes(value.bytes.data(), 16, b, ix);
      }
   };

   template <>
   struct to<BSON, bson::min_key>
   {
      static constexpr uint8_t type_code = bson::type::min_key;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bson::min_key&, is_context auto&&, auto&&, auto&) noexcept
      {
         // min_key carries no value bytes.
      }
   };

   template <>
   struct to<BSON, bson::max_key>
   {
      static constexpr uint8_t type_code = bson::type::max_key;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bson::max_key&, is_context auto&&, auto&&, auto&) noexcept
      {
         // max_key carries no value bytes.
      }
   };

   // Binary payload: int32(size) | subtype_byte | bytes. The `Bytes` container
   // is whatever the user chose (std::vector<std::byte>, std::string, etc.).
   template <class Bytes>
   struct to<BSON, bson::binary<Bytes>>
   {
      static constexpr uint8_t type_code = bson::type::binary;

      template <auto Opts>
      static void op(const bson::binary<Bytes>& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         const size_t n = value.data.size();
         // Subtype 0x02 (binary_old) is spec-required to wrap the payload in
         // a redundant inner int32 length, so its outer length is 4 + N.
         const bool is_binary_old = value.subtype == bson::binary_subtype::binary_old;
         const size_t cap = static_cast<size_t>(std::numeric_limits<int32_t>::max()) - (is_binary_old ? 4 : 0);
         if (n > cap) [[unlikely]] {
            ctx.error = error_code::invalid_length;
            return;
         }
         const size_t extra = is_binary_old ? 4 : 0;
         if (!ensure_space(ctx, b, ix + 5 + extra + n + write_padding_bytes)) [[unlikely]] {
            return;
         }
         bson_detail::dump_le<int32_t>(static_cast<int32_t>(n + extra), b, ix);
         bson_detail::put_byte(value.subtype, b, ix);
         if (is_binary_old) {
            bson_detail::dump_le<int32_t>(static_cast<int32_t>(n), b, ix);
         }
         if (n) {
            bson_detail::put_bytes(value.data.data(), n, b, ix);
         }
      }
   };

   // --- glz::uuid → binary subtype 0x04 -------------------------------------
   template <>
   struct to<BSON, uuid>
   {
      static constexpr uint8_t type_code = bson::type::binary;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const uuid& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 5 + 16 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         bson_detail::dump_le<int32_t>(16, b, ix);
         bson_detail::put_byte(bson::binary_subtype::uuid, b, ix);
         bson_detail::put_bytes(value.bytes.data(), 16, b, ix);
      }
   };

   // --- std::chrono::system_clock::time_point → datetime --------------------
   template <>
   struct to<BSON, std::chrono::system_clock::time_point>
   {
      static constexpr uint8_t type_code = bson::type::datetime;

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const std::chrono::system_clock::time_point& value, is_context auto&& ctx,
                                       auto&& b, auto& ix) noexcept
      {
         if (!ensure_space(ctx, b, ix + 8 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count();
         bson_detail::dump_le<int64_t>(static_cast<int64_t>(ms), b, ix);
      }
   };

   // ==========================================================================
   // Member-element writer: emits a complete (type_byte | key cstring | value)
   // element for a named member of a document. Handles the runtime-dispatched
   // cases (nullable/optional, variant) that `to<BSON, T>::type_code` cannot.
   //
   // Both the nullable and variant branches recurse back into this helper so
   // alternatives that are themselves nullable, always_null_t, or nested
   // variants resolve to a concrete (type_byte | value) pair. Only the final
   // else-branch reads `to<BSON, DT>::type_code` directly.
   // ==========================================================================
   namespace bson_detail
   {
      template <auto Opts, class T, class B>
      void write_member_element(std::string_view key, T&& value, is_context auto& ctx, B&& b, size_t& ix)
      {
         using DT = std::remove_cvref_t<T>;

         if constexpr (always_null_t<DT>) {
            (void)write_element_prefix(ctx, bson::type::null, key, b, ix);
         }
         else if constexpr (nullable_like<DT>) {
            if (value) {
               write_member_element<Opts>(key, *value, ctx, b, ix);
            }
            else {
               (void)write_element_prefix(ctx, bson::type::null, key, b, ix);
            }
         }
         else if constexpr (is_variant<DT>) {
            std::visit([&](auto&& alt) { write_member_element<Opts>(key, alt, ctx, b, ix); }, std::forward<T>(value));
         }
         else {
            if (!write_element_prefix(ctx, to<BSON, DT>::type_code, key, b, ix)) [[unlikely]] {
               return;
            }
            to<BSON, DT>::template op<Opts>(std::forward<T>(value), ctx, b, ix);
         }
      }

      // Returns true iff the struct field at index I should be omitted from
      // output under the given options (skip_null_members on an empty optional,
      // skip_default_members on a default-valued field, etc.).
      template <class T, auto Opts, size_t I, class Value, class Tie>
      GLZ_ALWAYS_INLINE bool should_skip_field_runtime(const Value& value, [[maybe_unused]] const Tie& t) noexcept
      {
         using val_t = field_t<T, I>;

         decltype(auto) member = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return get<I>(t);
            }
            else {
               return get<I>(reflect<T>::values);
            }
         }();

         if constexpr (Opts.skip_null_members && null_t<val_t>) {
            if constexpr (always_null_t<val_t>) {
               return true;
            }
            else if constexpr (nullable_value_t<val_t>) {
               if (!get_member(value, member).has_value()) return true;
            }
            else {
               if (!bool(get_member(value, member))) return true;
            }
         }
         if constexpr (check_skip_default_members(Opts) && has_skippable_default<val_t>) {
            if (is_default_value(get_member(value, member))) return true;
         }
         return false;
      }
   } // namespace bson_detail

   // ==========================================================================
   // Document writers — top-level or embedded. These write a full BSON
   // document: [int32 length][elements...][0x00]. The enclosing element byte
   // (0x03 for a struct/map, 0x04 for an array) is emitted by the caller in
   // `write_member_element`.
   // ==========================================================================

   // --- Reflected struct / glaze_object_t -----------------------------------
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_write<T>)
   struct to<BSON, T>
   {
      static constexpr uint8_t type_code = bson::type::document;
      static constexpr auto N = reflect<T>::size;

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         size_t start{};
         if (!bson_detail::reserve_document_length(ctx, b, ix, start)) [[unlikely]] {
            return;
         }

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;
            if constexpr (bson_detail::should_skip_reflected_field<T, Opts, I>()) {
               return;
            }
            else {
               if (bson_detail::should_skip_field_runtime<T, Opts, I>(value, t)) return;

               decltype(auto) member = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return get<I>(t);
                  }
                  else {
                     return get<I>(reflect<T>::values);
                  }
               }();

               static constexpr sv key = reflect<T>::keys[I];
               bson_detail::write_member_element<Opts>(key, get_member(value, member), ctx, b, ix);
            }
         });

         if (bool(ctx.error)) [[unlikely]]
            return;
         (void)bson_detail::finalize_document(ctx, b, ix, start);
      }
   };

   // --- Map with string keys ------------------------------------------------
   //
   // BSON keys are cstrings; non-string map keys are rejected at compile time
   // to avoid silent loss of fidelity.
   template <writable_map_t T>
   struct to<BSON, T>
   {
      static constexpr uint8_t type_code = bson::type::document;

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         using map_t = std::remove_cvref_t<decltype(value)>;
         using key_t = typename map_t::key_type;
         static_assert(str_t<key_t> || std::is_convertible_v<key_t, std::string_view>,
                       "BSON map keys must be string-like (cstring on the wire)");

         size_t start{};
         if (!bson_detail::reserve_document_length(ctx, b, ix, start)) [[unlikely]] {
            return;
         }

         using val_t = std::remove_cvref_t<detail::iterator_second_type<map_t>>;
         constexpr bool may_skip = null_t<val_t> && Opts.skip_null_members;

         for (auto&& [k, v] : value) {
            if (bool(ctx.error)) [[unlikely]]
               return;
            if constexpr (may_skip) {
               if (skip_member<Opts>(v)) continue;
            }
            const std::string_view key_sv{k};
            bson_detail::write_member_element<Opts>(key_sv, v, ctx, b, ix);
         }

         if (bool(ctx.error)) [[unlikely]]
            return;
         (void)bson_detail::finalize_document(ctx, b, ix, start);
      }
   };

   // --- Arrays / vectors / ranges -------------------------------------------
   //
   // Encoded as a BSON document whose keys are "0", "1", "2", ..., matching
   // the MongoDB convention. The document framing is identical to a struct.
   template <writable_array_t T>
   struct to<BSON, T>
   {
      static constexpr uint8_t type_code = bson::type::array;

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         size_t start{};
         if (!bson_detail::reserve_document_length(ctx, b, ix, start)) [[unlikely]] {
            return;
         }

         std::array<char, 24> scratch{};
         size_t i = 0;
         for (auto&& item : value) {
            if (bool(ctx.error)) [[unlikely]]
               return;
            const std::string_view key = bson_detail::array_key(i, scratch);
            bson_detail::write_member_element<Opts>(key, item, ctx, b, ix);
            ++i;
         }

         if (bool(ctx.error)) [[unlikely]]
            return;
         (void)bson_detail::finalize_document(ctx, b, ix, start);
      }
   };

   // --- glaze_array_t (tuple-like fixed array) ------------------------------
   template <class T>
      requires glaze_array_t<T>
   struct to<BSON, T>
   {
      static constexpr uint8_t type_code = bson::type::array;
      static constexpr auto N = reflect<T>::size;

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         size_t start{};
         if (!bson_detail::reserve_document_length(ctx, b, ix, start)) [[unlikely]] {
            return;
         }

         std::array<char, 24> scratch{};
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;
            const std::string_view key = bson_detail::array_key(I, scratch);
            bson_detail::write_member_element<Opts>(key, get_member(value, get<I>(reflect<T>::values)), ctx, b, ix);
         });

         if (bool(ctx.error)) [[unlikely]]
            return;
         (void)bson_detail::finalize_document(ctx, b, ix, start);
      }
   };

   // ==========================================================================
   // Public entry points
   // ==========================================================================

   // BSON's wire format requires the top-level value to be a document. Allow
   // reflected structs, maps with string keys, sequence containers (encoded
   // as integer-keyed documents), and glaze_array_t. Primitives are rejected
   // at the call site — they have no valid BSON document encoding on their
   // own.
   template <class T>
   concept bson_top_level_t = glaze_object_t<std::remove_cvref_t<T>> || reflectable<std::remove_cvref_t<T>> ||
                              writable_map_t<std::remove_cvref_t<T>> || writable_array_t<std::remove_cvref_t<T>> ||
                              glaze_array_t<std::remove_cvref_t<T>>;

   template <auto Opts = opts{}, bson_top_level_t T, class Buffer>
   [[nodiscard]] error_ctx write_bson(T&& value, Buffer&& buffer)
   {
      return write<set_bson<Opts>()>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <auto Opts = opts{}, bson_top_level_t T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_bson(T&& value)
   {
      return write<set_bson<Opts>()>(std::forward<T>(value));
   }

   template <auto Opts = opts{}, bson_top_level_t T>
   [[nodiscard]] inline error_code write_file_bson(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = write<set_bson<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec.ec;
      }
      return buffer_to_file(buffer, file_name);
   }
}
