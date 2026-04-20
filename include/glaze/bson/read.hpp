// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include "glaze/bson/header.hpp"
#include "glaze/bson/skip.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/to.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/uuid.hpp"
#include "glaze/util/variant.hpp"

// BSON reader — https://bsonspec.org/spec.html
//
// BSON values do not carry a self-describing tag byte; the element type lives
// one level up, in the containing document. The document reader therefore
// reads `type_byte | cstring key | value` triples and supplies the tag to the
// per-type parser via `from<BSON, T>::op(value, tag, ctx, it, end)`.

namespace glz
{
   namespace bson_detail
   {
      // RAII guard bumping ctx.depth on entry to a document/array reader and
      // popping on exit. Errors with exceeded_max_recursive_depth before the
      // stack can overflow on adversarial input — pathologically nested BSON
      // (e.g. `{"a": {"a": {...}}}`) is cheap to produce and a real DoS vector.
      template <class Ctx>
      struct depth_guard
      {
         Ctx& ctx;
         bool entered = false;

         depth_guard(Ctx& c) noexcept : ctx(c)
         {
            if (ctx.depth >= max_recursive_depth_limit) [[unlikely]] {
               ctx.error = error_code::exceeded_max_recursive_depth;
               return;
            }
            ++ctx.depth;
            entered = true;
         }
         ~depth_guard()
         {
            if (entered) --ctx.depth;
         }
         explicit operator bool() const noexcept { return entered; }
      };

      // --- Little-endian readers ----------------------------------------------
      //
      // On little-endian hosts these compile to a single aligned load. On big-
      // endian hosts we byteswap first.

      template <std::integral T, class It, class End>
      GLZ_ALWAYS_INLINE bool read_le(is_context auto& ctx, It& it, const End& end, T& out) noexcept
      {
         if (static_cast<size_t>(end - it) < sizeof(T)) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         T v{};
         std::memcpy(&v, it, sizeof(T));
         if constexpr (std::endian::native == std::endian::big) {
            v = static_cast<T>(std::byteswap(static_cast<std::make_unsigned_t<T>>(v)));
         }
         it += sizeof(T);
         out = v;
         return true;
      }

      template <class It, class End>
      GLZ_ALWAYS_INLINE bool read_le_double(is_context auto& ctx, It& it, const End& end, double& out) noexcept
      {
         uint64_t bits{};
         if (!read_le<uint64_t>(ctx, it, end, bits)) return false;
         out = std::bit_cast<double>(bits);
         return true;
      }

      // Return a string_view over a cstring in the buffer. Advances `it` to
      // just past the trailing 0x00.
      template <class It, class End>
      GLZ_ALWAYS_INLINE bool read_cstring(is_context auto& ctx, It& it, const End& end, std::string_view& out) noexcept
      {
         auto start = it;
         while (it < end) {
            if (*it == 0) {
               out = std::string_view{reinterpret_cast<const char*>(static_cast<const void*>(start)),
                                      static_cast<size_t>(it - start)};
               ++it;
               return true;
            }
            ++it;
         }
         ctx.error = error_code::unexpected_end;
         return false;
      }

      // Read a BSON string value: int32(len) | UTF-8 | 0x00. `out` points into
      // the input buffer; lifetime matches the buffer.
      template <class It, class End>
      GLZ_ALWAYS_INLINE bool read_bson_string(is_context auto& ctx, It& it, const End& end,
                                              std::string_view& out) noexcept
      {
         int32_t len{};
         if (!read_le<int32_t>(ctx, it, end, len)) return false;
         if (len < 1) [[unlikely]] {
            // Length must include the trailing null.
            ctx.error = error_code::syntax_error;
            return false;
         }
         const size_t payload_bytes = static_cast<size_t>(len) - 1; // Drop the trailing null.
         if (static_cast<size_t>(end - it) < static_cast<size_t>(len)) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         if (it[payload_bytes] != 0) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return false;
         }
         out = std::string_view{reinterpret_cast<const char*>(static_cast<const void*>(it)), payload_bytes};
         it += len;
         return true;
      }

      // Helper — read a BSON document's 4-byte length and compute the
      // terminating position (`stop`) such that a correct document consumes
      // exactly `stop - doc_start` bytes ending in 0x00 at `stop - 1`.
      template <class It, class End>
      GLZ_ALWAYS_INLINE bool read_document_stop(is_context auto& ctx, It& it, const End& end, It& stop) noexcept
      {
         auto doc_start = it;
         int32_t len{};
         if (!read_le<int32_t>(ctx, it, end, len)) return false;
         if (len < 5) [[unlikely]] {
            // Minimum legal document: 4-byte length + 0x00 terminator.
            ctx.error = error_code::syntax_error;
            return false;
         }
         if (static_cast<size_t>(end - doc_start) < static_cast<size_t>(len)) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         stop = doc_start + len;
         return true;
      }

      // Confirm the trailing 0x00 of a document is where we expect it, then
      // advance past it.
      template <class It, class End>
      GLZ_ALWAYS_INLINE bool finish_document(is_context auto& ctx, It& it, const End& /*end*/, It stop) noexcept
      {
         // Caller guarantees it <= stop. After the element loop the document
         // reader expects the null terminator at `stop - 1` and `it` pointing
         // there.
         if (it != stop - 1) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return false;
         }
         if (*it != 0) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return false;
         }
         ++it;
         return true;
      }
   } // namespace bson_detail

   // Top-level dispatcher. BSON's top-level is always a document, so the
   // implicit tag is bson::type::document. Per-element dispatch inside a
   // document is handled by the struct/array reader calling `from<BSON,
   // Member>::op` directly with the element's tag.
   template <>
   struct parse<BSON>
   {
      template <auto Opts, class T, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if constexpr (const_value_v<T>) {
            if constexpr (check_error_on_const_read(Opts)) {
               ctx.error = error_code::attempt_const_read;
            }
            else {
               skip_value<BSON>::template op<Opts>(bson::type::document, ctx, it, end);
            }
            return;
         }
         using V = std::remove_cvref_t<T>;
         // Top-level BSON has no element tag byte on the wire; a top-level
         // array is byte-for-byte identical to a document. Dispatch with the
         // tag the target type's reader expects so array-shaped top-level
         // targets (std::array, std::vector, std::tuple, glaze_array_t) route
         // to the array reader instead of tripping its tag check.
         constexpr uint8_t top_tag =
            (writable_array_t<V> || glaze_array_t<V>) ? bson::type::array : bson::type::document;
         from<BSON, V>::template op<Opts>(std::forward<T>(value), top_tag, ctx, it, end);
      }
   };

   // ==========================================================================
   // Primitive readers. Each from<BSON, T> receives the element tag from the
   // enclosing document and reads the corresponding value bytes.
   // ==========================================================================

   // --- Boolean --------------------------------------------------------------
   template <>
   struct from<BSON, bool>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(bool& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::boolean) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         value = (*it++ != 0);
      }
   };

   // --- Integers -------------------------------------------------------------
   //
   // Accept both int32 (0x10) and int64 (0x12) tags regardless of the C++
   // target width — the usual coercion rules apply: error on out-of-range.
   template <class T>
      requires(std::integral<T> && !std::same_as<T, bool>)
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(T& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag == bson::type::int32) {
            int32_t v{};
            if (!bson_detail::read_le<int32_t>(ctx, it, end, v)) return;
            // Reject negative wire values into any unsigned target (covers
            // uint32_t, uint64_t, and the smaller widths uniformly).
            if constexpr (!std::is_signed_v<T>) {
               if (v < 0) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
            }
            // Narrowing bounds check only when T is strictly narrower than
            // int32. For sizeof(T) == 4 && unsigned (uint32_t), the non-negative
            // check above plus the int32 input width guarantee the value fits.
            if constexpr (sizeof(T) < 4) {
               if (v < static_cast<int32_t>(std::numeric_limits<T>::min()) ||
                   v > static_cast<int32_t>(std::numeric_limits<T>::max())) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
            }
            value = static_cast<T>(v);
            return;
         }
         if (tag == bson::type::int64) {
            int64_t v{};
            if (!bson_detail::read_le<int64_t>(ctx, it, end, v)) return;
            // Narrow to T with range check. uint64_t target: accept only
            // non-negative values.
            if constexpr (std::is_same_v<T, uint64_t>) {
               if (v < 0) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               value = static_cast<uint64_t>(v);
               return;
            }
            else if constexpr (sizeof(T) <= 8) {
               if constexpr (std::is_signed_v<T>) {
                  if (v < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
                      v > static_cast<int64_t>(std::numeric_limits<T>::max())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
               }
               else {
                  if (v < 0 || static_cast<uint64_t>(v) > static_cast<uint64_t>(std::numeric_limits<T>::max()))
                     [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
               }
               value = static_cast<T>(v);
               return;
            }
         }
         ctx.error = error_code::syntax_error;
      }
   };

   // --- Floating point -------------------------------------------------------
   //
   // BSON double (0x01) is the native mapping; integer tags are accepted as a
   // widening conversion so `double x` can read an int32/int64 field.
   template <std::floating_point T>
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(T& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag == bson::type::double_) {
            double d{};
            if (!bson_detail::read_le_double(ctx, it, end, d)) return;
            value = static_cast<T>(d);
            return;
         }
         if (tag == bson::type::int32) {
            int32_t v{};
            if (!bson_detail::read_le<int32_t>(ctx, it, end, v)) return;
            value = static_cast<T>(v);
            return;
         }
         if (tag == bson::type::int64) {
            int64_t v{};
            if (!bson_detail::read_le<int64_t>(ctx, it, end, v)) return;
            value = static_cast<T>(v);
            return;
         }
         ctx.error = error_code::syntax_error;
      }
   };

   // --- Enums ----------------------------------------------------------------
   template <class T>
      requires(std::is_enum_v<T>)
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(T& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         using U = std::underlying_type_t<T>;
         U u{};
         from<BSON, U>::template op<Opts>(u, tag, ctx, it, end);
         if (bool(ctx.error)) return;
         value = static_cast<T>(u);
      }
   };

   // --- Strings --------------------------------------------------------------
   template <str_t T>
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(auto& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::string) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         std::string_view sv{};
         if (!bson_detail::read_bson_string(ctx, it, end, sv)) return;
         if constexpr (requires { value.assign(sv.data(), sv.size()); }) {
            value.assign(sv.data(), sv.size());
         }
         else {
            value = sv;
         }
      }
   };

   // --- Nullable / optional --------------------------------------------------
   template <nullable_like T>
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(auto& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag == bson::type::null) {
            if constexpr (requires { value.reset(); }) {
               value.reset();
            }
            else {
               value = T{};
            }
            return;
         }
         if (!value) {
            if constexpr (requires { value.emplace(); }) {
               value.emplace();
            }
            else if constexpr (std::is_constructible_v<typename T::value_type>) {
               value = typename T::value_type{};
            }
            else {
               ctx.error = error_code::invalid_nullable_read;
               return;
            }
         }
         using Inner = std::remove_cvref_t<decltype(*value)>;
         from<BSON, Inner>::template op<Opts>(*value, tag, ctx, it, end);
      }
   };

   // --- always_null_t (std::monostate, std::nullptr_t, std::nullopt_t) -------
   //
   // These carry no state and never appear as top-level BSON values, but are
   // reachable as variant alternatives. The tag is always bson::type::null
   // when the variant reader routes here; anything else is a wire error.
   template <always_null_t T>
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(auto&&, uint8_t tag, is_context auto& ctx, It&, const End&) noexcept
      {
         if (tag != bson::type::null) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // --- BSON-native helper types --------------------------------------------

   template <>
   struct from<BSON, bson::object_id>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(bson::object_id& value, uint8_t tag, is_context auto& ctx, It& it,
                                       const End& end) noexcept
      {
         if (tag != bson::type::object_id) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<size_t>(end - it) < 12) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         std::memcpy(value.bytes.data(), it, 12);
         it += 12;
      }
   };

   template <>
   struct from<BSON, bson::datetime>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(bson::datetime& value, uint8_t tag, is_context auto& ctx, It& it,
                                       const End& end) noexcept
      {
         if (tag != bson::type::datetime) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         (void)bson_detail::read_le<int64_t>(ctx, it, end, value.ms_since_epoch);
      }
   };

   template <>
   struct from<BSON, bson::timestamp>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(bson::timestamp& value, uint8_t tag, is_context auto& ctx, It& it,
                                       const End& end) noexcept
      {
         if (tag != bson::type::timestamp) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!bson_detail::read_le<uint32_t>(ctx, it, end, value.increment)) return;
         (void)bson_detail::read_le<uint32_t>(ctx, it, end, value.seconds);
      }
   };

   template <>
   struct from<BSON, bson::regex>
   {
      template <auto Opts, class It, class End>
      static void op(bson::regex& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::regex) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         std::string_view pat{};
         std::string_view opt{};
         if (!bson_detail::read_cstring(ctx, it, end, pat)) return;
         if (!bson_detail::read_cstring(ctx, it, end, opt)) return;
         value.pattern.assign(pat);
         value.options.assign(opt);
      }
   };

   template <>
   struct from<BSON, bson::javascript>
   {
      template <auto Opts, class It, class End>
      static void op(bson::javascript& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::javascript) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         std::string_view sv{};
         if (!bson_detail::read_bson_string(ctx, it, end, sv)) return;
         value.code.assign(sv);
      }
   };

   template <>
   struct from<BSON, bson::decimal128>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(bson::decimal128& value, uint8_t tag, is_context auto& ctx, It& it,
                                       const End& end) noexcept
      {
         if (tag != bson::type::decimal128) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<size_t>(end - it) < 16) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         std::memcpy(value.bytes.data(), it, 16);
         it += 16;
      }
   };

   template <>
   struct from<BSON, bson::min_key>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(bson::min_key&, uint8_t tag, is_context auto& ctx, It&, const End&) noexcept
      {
         if (tag != bson::type::min_key) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   template <>
   struct from<BSON, bson::max_key>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(bson::max_key&, uint8_t tag, is_context auto& ctx, It&, const End&) noexcept
      {
         if (tag != bson::type::max_key) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   template <class Bytes>
   struct from<BSON, bson::binary<Bytes>>
   {
      template <auto Opts, class It, class End>
      static void op(bson::binary<Bytes>& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::binary) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         int32_t outer_len{};
         if (!bson_detail::read_le<int32_t>(ctx, it, end, outer_len)) return;
         if (outer_len < 0) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         value.subtype = static_cast<uint8_t>(*it++);
         int32_t payload_len = outer_len;
         if (value.subtype == bson::binary_subtype::binary_old) [[unlikely]] {
            // Spec: 0x02 wraps the payload in a redundant inner int32 length.
            // Outer length must equal 4 + inner.
            if (outer_len < 4) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            int32_t inner_len{};
            if (!bson_detail::read_le<int32_t>(ctx, it, end, inner_len)) return;
            if (inner_len < 0 || inner_len != outer_len - 4) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            payload_len = inner_len;
         }
         if (static_cast<size_t>(end - it) < static_cast<size_t>(payload_len)) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         value.data.resize(static_cast<size_t>(payload_len));
         if (payload_len) {
            std::memcpy(value.data.data(), it, static_cast<size_t>(payload_len));
            it += payload_len;
         }
      }
   };

   // --- glz::uuid ← binary subtype 0x04 -------------------------------------
   template <>
   struct from<BSON, uuid>
   {
      template <auto Opts, class It, class End>
      static void op(uuid& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::binary) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         int32_t len{};
         if (!bson_detail::read_le<int32_t>(ctx, it, end, len)) return;
         if (len != 16) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const uint8_t subtype = static_cast<uint8_t>(*it++);
         // Only accept canonical subtype 0x04 (RFC 9562 byte order). Legacy
         // 0x03 (uuid_old) is rejected because its bytes are ambiguous: Java,
         // C#, and Python drivers each laid them out differently, so decoding
         // without the origin flavor silently yields scrambled bytes.
         if (subtype != bson::binary_subtype::uuid) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<size_t>(end - it) < 16) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         std::memcpy(value.bytes.data(), it, 16);
         it += 16;
      }
   };

   // --- std::chrono::system_clock::time_point ← datetime --------------------
   template <>
   struct from<BSON, std::chrono::system_clock::time_point>
   {
      template <auto Opts, class It, class End>
      GLZ_ALWAYS_INLINE static void op(std::chrono::system_clock::time_point& value, uint8_t tag, is_context auto& ctx,
                                       It& it, const End& end) noexcept
      {
         if (tag != bson::type::datetime) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         int64_t ms{};
         if (!bson_detail::read_le<int64_t>(ctx, it, end, ms)) return;
         value = std::chrono::system_clock::time_point{std::chrono::milliseconds{ms}};
      }
   };

   // ==========================================================================
   // Variant — BSON-style auto-deduction from the element tag byte.
   //
   // Mirrors the JSONB pattern: each BSON wire type maps to a category trait,
   // and for each category the writer picks the first variant alternative
   // matching that trait. This requires at most one alternative per category,
   // enforced by a static_assert. Users needing multiple alternatives sharing
   // a BSON type (e.g., two distinct document shapes) must drop one or switch
   // to an explicit tag-based variant convention (not yet supported here).
   //
   // The writer serializes the active alternative directly (its type byte and
   // key are already on the wire from the enclosing document), so round-trip
   // works as long as the alternative category is unique.
   // ==========================================================================

   namespace bson_detail
   {
      // --- Category traits ---------------------------------------------------

      template <class T>
      struct is_bson_variant_null : std::bool_constant<null_t<T>>
      {};

      template <class T>
      struct is_bson_variant_bool : std::bool_constant<bool_t<T>>
      {};

      template <class T>
      struct is_bson_variant_int : std::bool_constant<std::integral<std::remove_cvref_t<T>> && !bool_t<T>>
      {};

      template <class T>
      struct is_bson_variant_float : std::bool_constant<std::floating_point<std::remove_cvref_t<T>>>
      {};

      template <class T>
      struct is_bson_variant_string : std::bool_constant<str_t<T>>
      {};

      // BSON helper types: these are reflectable aggregates but have dedicated
      // wire tags, so they must NOT be classified as generic documents. Listed
      // ahead of the document/array traits so those can exclude them.
      template <class T>
      struct is_bson_variant_binary : std::false_type
      {};
      template <class Bytes>
      struct is_bson_variant_binary<bson::binary<Bytes>> : std::true_type
      {};

      template <class T>
      struct is_bson_variant_uuid : std::bool_constant<std::same_as<std::remove_cvref_t<T>, uuid>>
      {};

      template <class T>
      struct is_bson_variant_object_id : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::object_id>>
      {};

      template <class T>
      struct is_bson_variant_datetime
         : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::datetime> ||
                              std::same_as<std::remove_cvref_t<T>, std::chrono::system_clock::time_point>>
      {};

      template <class T>
      struct is_bson_variant_timestamp : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::timestamp>>
      {};

      template <class T>
      struct is_bson_variant_regex : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::regex>>
      {};

      template <class T>
      struct is_bson_variant_javascript : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::javascript>>
      {};

      template <class T>
      struct is_bson_variant_decimal128 : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::decimal128>>
      {};

      template <class T>
      struct is_bson_variant_min_key : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::min_key>>
      {};

      template <class T>
      struct is_bson_variant_max_key : std::bool_constant<std::same_as<std::remove_cvref_t<T>, bson::max_key>>
      {};

      // True if T is one of the BSON helper types above. Used to exclude them
      // from the generic document / array categories.
      template <class T>
      struct is_bson_helper_type
         : std::bool_constant<is_bson_variant_binary<T>::value || is_bson_variant_uuid<T>::value ||
                              is_bson_variant_object_id<T>::value || is_bson_variant_datetime<T>::value ||
                              is_bson_variant_timestamp<T>::value || is_bson_variant_regex<T>::value ||
                              is_bson_variant_javascript<T>::value || is_bson_variant_decimal128<T>::value ||
                              is_bson_variant_min_key<T>::value || is_bson_variant_max_key<T>::value>
      {};

      // Matches reflected structs, reflectable aggregates, and string-keyed maps.
      // Excludes arrays (which land in their own category), strings, and the BSON
      // helper types (which are reflectable aggregates but have dedicated tags).
      template <class T>
      struct is_bson_variant_document
         : std::bool_constant<(glaze_object_t<T> || reflectable<T> || writable_map_t<T>) && !writable_array_t<T> &&
                              !str_t<T> && !is_bson_helper_type<T>::value>
      {};

      template <class T>
      struct is_bson_variant_array : std::bool_constant<(writable_array_t<T> || glaze_array_t<T>) && !str_t<T> &&
                                                        !writable_map_t<T> && !is_bson_helper_type<T>::value>
      {};

      // --- first_matching_index<V, Trait> : finds the first alternative index
      // whose alternative type satisfies Trait, or std::variant_npos.
      template <class Variant, template <class> class Trait>
      struct first_matching_index_impl;

      template <template <class> class Trait, class... Ts>
      struct first_matching_index_impl<std::variant<Ts...>, Trait>
      {
         static constexpr size_t find()
         {
            size_t result = std::variant_npos;
            size_t idx = 0;
            ((Trait<std::remove_cvref_t<Ts>>::value && result == std::variant_npos ? (result = idx, ++idx) : ++idx),
             ...);
            return result;
         }
         static constexpr size_t value = find();
      };

      template <class Variant, template <class> class Trait>
      constexpr size_t first_matching_index_v = first_matching_index_impl<Variant, Trait>::value;

      // --- auto-deducibility check: at most one alternative per category ----
      template <is_variant T>
      consteval bool variant_bson_auto_deducible()
      {
         constexpr auto N = std::variant_size_v<T>;
         size_t nulls = 0, bools = 0, ints = 0, floats = 0, strings = 0, documents = 0, arrays = 0, binaries = 0,
                uuids = 0, object_ids = 0, datetimes = 0, timestamps = 0, regexes = 0, javascripts = 0,
                decimal128s = 0, min_keys = 0, max_keys = 0;
         [&]<size_t... I>(std::index_sequence<I...>) {
            (([&]<size_t Idx>() {
                using V = std::remove_cvref_t<std::variant_alternative_t<Idx, T>>;
                nulls += is_bson_variant_null<V>::value;
                bools += is_bson_variant_bool<V>::value;
                ints += is_bson_variant_int<V>::value;
                floats += is_bson_variant_float<V>::value;
                strings += is_bson_variant_string<V>::value;
                documents += is_bson_variant_document<V>::value;
                arrays += is_bson_variant_array<V>::value;
                binaries += is_bson_variant_binary<V>::value;
                uuids += is_bson_variant_uuid<V>::value;
                object_ids += is_bson_variant_object_id<V>::value;
                datetimes += is_bson_variant_datetime<V>::value;
                timestamps += is_bson_variant_timestamp<V>::value;
                regexes += is_bson_variant_regex<V>::value;
                javascripts += is_bson_variant_javascript<V>::value;
                decimal128s += is_bson_variant_decimal128<V>::value;
                min_keys += is_bson_variant_min_key<V>::value;
                max_keys += is_bson_variant_max_key<V>::value;
             }.template operator()<I>()),
             ...);
         }(std::make_index_sequence<N>{});
         // Binary wire tag 0x05 is shared between `binary<...>` and `uuid`;
         // allow both in the same variant — the reader prefers `binary` when
         // both are present because its reader accepts any subtype.
         return nulls <= 1 && bools <= 1 && ints <= 1 && floats <= 1 && strings <= 1 && documents <= 1 &&
                arrays <= 1 && binaries <= 1 && uuids <= 1 && object_ids <= 1 && datetimes <= 1 && timestamps <= 1 &&
                regexes <= 1 && javascripts <= 1 && decimal128s <= 1 && min_keys <= 1 && max_keys <= 1;
      }
   } // namespace bson_detail

   template <is_variant T>
   struct from<BSON, T>
   {
      static_assert(bson_detail::variant_bson_auto_deducible<T>(),
                    "BSON variant alternatives must be unique per BSON element category "
                    "(at most one each of: null, bool, int, float, string, document, array, binary, "
                    "uuid, object_id, datetime, timestamp, regex, javascript, decimal128, min_key, max_key).");

      template <auto Opts, class It, class End>
      static void op(T& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         using V = std::remove_cvref_t<T>;
         constexpr size_t null_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_null>;
         constexpr size_t bool_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_bool>;
         constexpr size_t int_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_int>;
         constexpr size_t float_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_float>;
         constexpr size_t string_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_string>;
         constexpr size_t document_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_document>;
         constexpr size_t array_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_array>;
         constexpr size_t binary_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_binary>;
         constexpr size_t uuid_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_uuid>;
         constexpr size_t object_id_idx =
            bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_object_id>;
         constexpr size_t datetime_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_datetime>;
         constexpr size_t timestamp_idx =
            bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_timestamp>;
         constexpr size_t regex_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_regex>;
         constexpr size_t javascript_idx =
            bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_javascript>;
         constexpr size_t decimal128_idx =
            bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_decimal128>;
         constexpr size_t min_key_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_min_key>;
         constexpr size_t max_key_idx = bson_detail::first_matching_index_v<V, bson_detail::is_bson_variant_max_key>;

         auto dispatch = [&]<size_t Idx>() {
            if constexpr (Idx == std::variant_npos) {
               ctx.error = error_code::no_matching_variant_type;
            }
            else {
               using Alt = std::remove_cvref_t<std::variant_alternative_t<Idx, V>>;
               if (value.index() != Idx) {
                  emplace_runtime_variant(value, Idx);
               }
               from<BSON, Alt>::template op<Opts>(std::get<Idx>(value), tag, ctx, it, end);
            }
         };

         switch (tag) {
         case bson::type::null:
            dispatch.template operator()<null_idx>();
            return;
         case bson::type::boolean:
            dispatch.template operator()<bool_idx>();
            return;
         case bson::type::int32:
         case bson::type::int64:
            // Prefer int alternative; widen to float if the variant has only
            // float, since the float reader accepts int tags.
            if constexpr (int_idx != std::variant_npos) {
               dispatch.template operator()<int_idx>();
            }
            else {
               dispatch.template operator()<float_idx>();
            }
            return;
         case bson::type::double_:
            // No narrowing fallback: the int reader rejects double tags, so
            // falling back would just yield syntax_error. A clean
            // no_matching_variant_type is more informative.
            dispatch.template operator()<float_idx>();
            return;
         case bson::type::string:
            dispatch.template operator()<string_idx>();
            return;
         case bson::type::document:
            dispatch.template operator()<document_idx>();
            return;
         case bson::type::array:
            dispatch.template operator()<array_idx>();
            return;
         case bson::type::binary:
            // `binary<...>` accepts any subtype and so takes priority when both
            // are present. A lone `uuid` alternative still works because
            // binary_idx is npos and we fall through.
            if constexpr (binary_idx != std::variant_npos) {
               dispatch.template operator()<binary_idx>();
            }
            else {
               dispatch.template operator()<uuid_idx>();
            }
            return;
         case bson::type::object_id:
            dispatch.template operator()<object_id_idx>();
            return;
         case bson::type::datetime:
            dispatch.template operator()<datetime_idx>();
            return;
         case bson::type::regex:
            dispatch.template operator()<regex_idx>();
            return;
         case bson::type::javascript:
            dispatch.template operator()<javascript_idx>();
            return;
         case bson::type::timestamp:
            dispatch.template operator()<timestamp_idx>();
            return;
         case bson::type::decimal128:
            dispatch.template operator()<decimal128_idx>();
            return;
         case bson::type::min_key:
            dispatch.template operator()<min_key_idx>();
            return;
         case bson::type::max_key:
            dispatch.template operator()<max_key_idx>();
            return;
         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   };

   // ==========================================================================
   // Document-family readers: struct, map, array, glaze_array_t.
   //
   // All four share the same wire framing — [int32 length][elements][0x00] —
   // and differ only in how they route each element's value by key/index.
   // ==========================================================================

   namespace bson_detail
   {
      // Read the body of a BSON document whose outer [int32 length] has
      // already been consumed. Calls `on_element(tag, key)` for each
      // (tag, key) pair; the callback must consume the value bytes itself.
      //
      // A correctly-framed document ends with exactly one 0x00 byte at
      // `stop - 1`. Two ways that can fail:
      //   - terminator appears early (length field is larger than the body) →
      //     we exit the loop via `tag == 0` with `it < stop`.
      //   - terminator missing (declared length undercounts, or body is
      //     corrupted so the terminator is consumed as part of an element) →
      //     the loop exits via `it >= stop` without ever seeing a zero tag.
      // Both surface as `syntax_error` with a specific custom message.
      template <class It, class F>
      bool read_document_elements(is_context auto& ctx, It& it, const It& stop, F&& on_element) noexcept
      {
         while (it < stop) {
            const uint8_t tag = static_cast<uint8_t>(*it++);
            if (tag == 0) {
               if (it != stop) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  ctx.custom_error_message = "document terminator before end of declared length";
                  return false;
               }
               return true;
            }
            std::string_view key{};
            if (!read_cstring(ctx, it, stop, key)) {
               // Most plausible cause: no 0x00 terminator in the expected slot,
               // so what we read as a tag was garbage and read_cstring ran off
               // the end of the document looking for the key's null byte.
               if (ctx.error == error_code::unexpected_end) {
                  ctx.custom_error_message = "missing document terminator";
               }
               return false;
            }
            on_element(tag, key);
            if (bool(ctx.error)) return false;
         }
         ctx.error = error_code::syntax_error;
         ctx.custom_error_message = "missing document terminator";
         return false;
      }

      template <auto Opts, class T, class It>
      void read_struct_body(T& value, is_context auto& ctx, It& it, const It& stop)
      {
         using DT = std::remove_cvref_t<T>;
         static constexpr auto N = reflect<DT>::size;

         auto fields = [&]() -> decltype(auto) {
            if constexpr (Opts.error_on_missing_keys) {
               return bit_array<N>{};
            }
            else {
               return nullptr;
            }
         }();

         read_document_elements(ctx, it, stop, [&](uint8_t tag, std::string_view key) {
            bool matched = false;
            if constexpr (N > 0) {
               static constexpr auto HashInfo = hash_info<DT>;
               const auto index =
                  decode_hash_with_size<BSON, DT, HashInfo, HashInfo.type>::op(key.data(), key.data() + key.size(),
                                                                                key.size());
               if (index < N) {
                  visit<N>(
                     [&]<size_t I>() {
                        static constexpr auto TargetKey = get<I>(reflect<DT>::keys);
                        static constexpr auto Length = TargetKey.size();
                        if ((Length == key.size()) && compare<Length>(TargetKey.data(), key.data())) [[likely]] {
                           matched = true;
                           using MemberT = std::remove_cvref_t<field_t<DT, I>>;
                           if constexpr (reflectable<DT>) {
                              from<BSON, MemberT>::template op<Opts>(get_member(value, get<I>(to_tie(value))), tag, ctx,
                                                                     it, stop);
                           }
                           else {
                              from<BSON, MemberT>::template op<Opts>(
                                 get_member(value, get<I>(reflect<DT>::values)), tag, ctx, it, stop);
                           }
                           if constexpr (Opts.error_on_missing_keys) {
                              fields[I] = true;
                           }
                        }
                     },
                     index);
               }
            }
            if (!matched) {
               if constexpr (Opts.error_on_unknown_keys) {
                  ctx.error = error_code::unknown_key;
                  ctx.custom_error_message = key;
                  return;
               }
               skip_value<BSON>::template op<Opts>(tag, ctx, it, stop);
            }
         });

         if (bool(ctx.error)) return;

         if constexpr (Opts.error_on_missing_keys) {
            static constexpr auto req_fields = required_fields<DT, Opts>();
            if ((req_fields & fields) != req_fields) {
               for (size_t i = 0; i < N; ++i) {
                  if (!fields[i] && req_fields[i]) {
                     ctx.custom_error_message = reflect<DT>::keys[i];
                     break;
                  }
               }
               ctx.error = error_code::missing_key;
            }
         }
      }
   } // namespace bson_detail

   // --- Reflected struct / glaze_object_t -----------------------------------
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_read<T>)
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(auto& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::document) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         bson_detail::depth_guard guard{ctx};
         if (!guard) return;
         It stop{};
         if (!bson_detail::read_document_stop(ctx, it, end, stop)) return;
         bson_detail::read_struct_body<Opts, T>(value, ctx, it, stop);
      }
   };

   // --- Map with string keys ------------------------------------------------
   //
   // BSON keys are cstrings; non-string map keys are unreachable on the wire,
   // so reading into such a map is rejected at compile time to match the
   // writer's static assertion.
   template <writable_map_t T>
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(T& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::document) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         using key_t = typename T::key_type;
         static_assert(str_t<key_t> || std::is_constructible_v<key_t, std::string_view>,
                       "BSON map keys must be string-like");

         bson_detail::depth_guard guard{ctx};
         if (!guard) return;
         It stop{};
         if (!bson_detail::read_document_stop(ctx, it, end, stop)) return;

         value.clear();

         bson_detail::read_document_elements(ctx, it, stop, [&](uint8_t element_tag, std::string_view key) {
            using V = typename T::mapped_type;
            auto [iter, inserted] = value.try_emplace(key_t{key});
            (void)inserted;
            from<BSON, V>::template op<Opts>(iter->second, element_tag, ctx, it, stop);
         });
      }
   };

   // --- Arrays / vectors / ranges -------------------------------------------
   //
   // Encoded on the wire as a document with integer-string keys; the keys are
   // not validated against the sequence order — matching MongoDB's tolerance
   // for out-of-order arrays on the wire. Elements are appended in the order
   // they appear in the document.
   template <writable_array_t T>
      requires(emplace_backable<T> || resizable<T> || has_size<T>)
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(T& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         bson_detail::depth_guard guard{ctx};
         if (!guard) return;
         It stop{};
         if (!bson_detail::read_document_stop(ctx, it, end, stop)) return;

         using V = range_value_t<T>;
         if constexpr (emplace_backable<T>) {
            value.clear();
            bson_detail::read_document_elements(ctx, it, stop, [&](uint8_t element_tag, std::string_view /*key*/) {
               value.emplace_back();
               from<BSON, V>::template op<Opts>(value.back(), element_tag, ctx, it, stop);
            });
         }
         else {
            // Fixed-capacity container (e.g., std::array): fill by index.
            size_t i = 0;
            const size_t cap = value.size();
            bson_detail::read_document_elements(ctx, it, stop, [&](uint8_t element_tag, std::string_view /*key*/) {
               if (i >= cap) [[unlikely]] {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }
               from<BSON, V>::template op<Opts>(value[i], element_tag, ctx, it, stop);
               ++i;
            });
         }
      }
   };

   // --- glaze_array_t (tuple-like) ------------------------------------------
   template <class T>
      requires glaze_array_t<T>
   struct from<BSON, T>
   {
      template <auto Opts, class It, class End>
      static void op(auto& value, uint8_t tag, is_context auto& ctx, It& it, const End& end) noexcept
      {
         if (tag != bson::type::array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         bson_detail::depth_guard guard{ctx};
         if (!guard) return;
         It stop{};
         if (!bson_detail::read_document_stop(ctx, it, end, stop)) return;

         static constexpr auto N = reflect<T>::size;
         size_t i = 0;
         bson_detail::read_document_elements(ctx, it, stop, [&](uint8_t element_tag, std::string_view /*key*/) {
            if (i >= N) [[unlikely]] {
               ctx.error = error_code::exceeded_static_array_size;
               return;
            }
            visit<N>(
               [&]<size_t I>() {
                  using MemberT = std::remove_cvref_t<field_t<T, I>>;
                  from<BSON, MemberT>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)), element_tag, ctx,
                                                        it, stop);
               },
               i);
            ++i;
         });
      }
   };

   // ==========================================================================
   // Public entry points
   // ==========================================================================

   template <class T>
   concept bson_top_level_read_t = glaze_object_t<std::remove_cvref_t<T>> || reflectable<std::remove_cvref_t<T>> ||
                                   writable_map_t<std::remove_cvref_t<T>> ||
                                   writable_array_t<std::remove_cvref_t<T>> ||
                                   glaze_array_t<std::remove_cvref_t<T>>;

   // BSON documents are length-prefixed (int32 header = total bytes). The core
   // reader consumes exactly that many bytes, but would otherwise silently
   // ignore anything trailing the top-level document. Enforce exact-fill at the
   // public boundary so truncation/concatenation bugs surface instead of being
   // masked.
   namespace bson_detail
   {
      template <class Buffer>
      GLZ_ALWAYS_INLINE error_ctx enforce_exact_fill(const Buffer& buffer, error_ctx ec) noexcept
      {
         if (!ec && ec.count != buffer.size()) [[unlikely]] {
            return {ec.count, error_code::syntax_error};
         }
         return ec;
      }
   }

   template <auto Opts = opts{}, bson_top_level_read_t T, class Buffer>
   [[nodiscard]] inline error_ctx read_bson(T&& value, Buffer&& buffer)
   {
      auto ec = read<set_bson<Opts>()>(std::forward<T>(value), buffer);
      return bson_detail::enforce_exact_fill(buffer, ec);
   }

   template <class T, auto Opts = opts{}, class Buffer>
      requires bson_top_level_read_t<T>
   [[nodiscard]] inline expected<T, error_ctx> read_bson(Buffer&& buffer)
   {
      T value{};
      auto ec = read<set_bson<Opts>()>(value, buffer);
      ec = bson_detail::enforce_exact_fill(buffer, ec);
      if (ec) [[unlikely]] {
         return unexpected(ec);
      }
      return value;
   }

   template <auto Opts = opts{}, bson_top_level_read_t T>
   [[nodiscard]] inline error_ctx read_file_bson(T& value, const sv file_name, auto&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;
      const auto file_error = file_to_buffer(buffer, ctx.current_file);
      if (bool(file_error)) [[unlikely]] {
         return error_ctx{0, file_error};
      }
      auto ec = read<set_bson<Opts>()>(value, buffer, ctx);
      return bson_detail::enforce_exact_fill(buffer, ec);
   }
}
