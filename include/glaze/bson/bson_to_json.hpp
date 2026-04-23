// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "glaze/base64/base64.hpp"
#include "glaze/bson/header.hpp"
#include "glaze/bson/read.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/json/write.hpp"

// BSON → JSON converter.
//
// Output uses MongoDB Canonical Extended JSON v2 conventions for types that
// have no native JSON representation:
//   - object_id  → {"$oid":"<24 hex chars>"}
//   - datetime   → {"$date":{"$numberLong":"<int64 ms as string>"}}
//   - timestamp  → {"$timestamp":{"t":<sec>,"i":<inc>}}
//   - binary     → {"$binary":{"base64":"...","subType":"XX"}}
//   - regex      → {"$regularExpression":{"pattern":"...","options":"..."}}
//   - javascript → {"$code":"..."}
//   - min_key / max_key → {"$minKey":1} / {"$maxKey":1}
//   - symbol     → {"$symbol":"..."}
//   - undefined  → {"$undefined":true}
//   - db_pointer / code_w_scope → syntax_error (deprecated; rare in practice)
//
// Asymmetry with the struct-read skip path: when reading BSON into a C++
// struct, unknown or unused fields (including db_pointer and code_w_scope) are
// skipped — the caller didn't ask for them, so silently advancing past the
// bytes is correct. The converter cannot do the same: callers expect the
// output JSON to reflect the entire document, so silently dropping fields
// would misrepresent the contents. These deprecated types therefore fail
// loudly here rather than being elided.
//
// decimal128 has no native parser in Glaze, so the 16-byte payload is emitted
// under a non-`$` wrapper — {"decimal128Hex":"<32 hex chars>"} — to preserve
// the value for round trip without masquerading as a real Extended JSON
// $numberDecimal (which requires a decimal string, not hex bytes).

namespace glz
{
   namespace bson_detail
   {
      // --- small emit helpers ------------------------------------------------

      template <class B, size_t N>
      GLZ_ALWAYS_INLINE void emit_literal(is_context auto& ctx, B& out, size_t& ix, const char (&lit)[N]) noexcept
      {
         constexpr size_t n = N - 1; // exclude the trailing NUL
         if (!ensure_space(ctx, out, ix + n + write_padding_bytes)) return;
         std::memcpy(&out[ix], lit, n);
         ix += n;
      }

      template <class B>
      GLZ_ALWAYS_INLINE void emit_char(is_context auto& ctx, B& out, size_t& ix, char c) noexcept
      {
         using V = typename std::decay_t<B>::value_type;
         if (!ensure_space(ctx, out, ix + 1 + write_padding_bytes)) return;
         out[ix++] = static_cast<V>(c);
      }

      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE void emit_json_string(is_context auto& ctx, std::string_view s, B& out, size_t& ix) noexcept
      {
         to<JSON, std::string_view>::template op<Opts>(s, ctx, out, ix);
      }

      // Emit `n` payload bytes as a lowercase hex literal (no quotes, no prefix).
      template <class B>
      inline void emit_hex_bytes(is_context auto& ctx, const uint8_t* bytes, size_t n, B& out, size_t& ix) noexcept
      {
         using V = typename std::decay_t<B>::value_type;
         static constexpr char digits[] = "0123456789abcdef";
         if (!ensure_space(ctx, out, ix + 2 * n + write_padding_bytes)) return;
         for (size_t i = 0; i < n; ++i) {
            const uint8_t b = bytes[i];
            out[ix++] = static_cast<V>(digits[b >> 4]);
            out[ix++] = static_cast<V>(digits[b & 0x0F]);
         }
      }

      // --- value / document / array dispatchers ------------------------------

      template <auto Opts, class It, class End, class B>
      void bson_to_json_value(uint8_t tag, is_context auto& ctx, It& it, const End& end, B& out, size_t& ix) noexcept;

      template <auto Opts, class It, class B>
      void bson_to_json_document_body(is_context auto& ctx, It& it, const It& stop, B& out, size_t& ix) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) return;

         emit_char(ctx, out, ix, '{');
         if (bool(ctx.error)) return;

         bool first = true;
         read_document_elements(ctx, it, stop, [&](uint8_t element_tag, std::string_view key) {
            if (!first) {
               emit_char(ctx, out, ix, ',');
               if (bool(ctx.error)) return;
            }
            first = false;

            emit_json_string<Opts>(ctx, key, out, ix);
            if (bool(ctx.error)) return;
            emit_char(ctx, out, ix, ':');
            if (bool(ctx.error)) return;

            bson_to_json_value<Opts>(element_tag, ctx, it, stop, out, ix);
         });
         if (bool(ctx.error)) return;

         emit_char(ctx, out, ix, '}');
      }

      template <auto Opts, class It, class B>
      void bson_to_json_array_body(is_context auto& ctx, It& it, const It& stop, B& out, size_t& ix) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) return;

         emit_char(ctx, out, ix, '[');
         if (bool(ctx.error)) return;

         bool first = true;
         read_document_elements(ctx, it, stop, [&](uint8_t element_tag, std::string_view /*key*/) {
            if (!first) {
               emit_char(ctx, out, ix, ',');
               if (bool(ctx.error)) return;
            }
            first = false;
            bson_to_json_value<Opts>(element_tag, ctx, it, stop, out, ix);
         });
         if (bool(ctx.error)) return;

         emit_char(ctx, out, ix, ']');
      }

      template <auto Opts, class It, class End, class B>
      void bson_to_json_value(uint8_t tag, is_context auto& ctx, It& it, const End& end, B& out, size_t& ix) noexcept
      {
         using namespace bson;
         switch (tag) {
         case type::double_: {
            double d{};
            if (!read_le_double(ctx, it, end, d)) return;
            to<JSON, double>::template op<Opts>(d, ctx, out, ix);
            return;
         }
         case type::string: {
            int32_t len{};
            if (!read_le<int32_t>(ctx, it, end, len)) return;
            if (len < 1) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (static_cast<int64_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            // len includes the trailing null byte.
            const auto n = static_cast<size_t>(len) - 1;
            std::string_view s{reinterpret_cast<const char*>(&*it), n};
            emit_json_string<Opts>(ctx, s, out, ix);
            it += len;
            return;
         }
         case type::document: {
            It stop{};
            if (!read_document_stop(ctx, it, end, stop)) return;
            bson_to_json_document_body<Opts>(ctx, it, stop, out, ix);
            return;
         }
         case type::array: {
            It stop{};
            if (!read_document_stop(ctx, it, end, stop)) return;
            bson_to_json_array_body<Opts>(ctx, it, stop, out, ix);
            return;
         }
         case type::binary: {
            int32_t outer_len{};
            if (!read_le<int32_t>(ctx, it, end, outer_len)) return;
            if (outer_len < 0) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (static_cast<int64_t>(end - it) < static_cast<int64_t>(1) + outer_len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint8_t subtype = static_cast<uint8_t>(*it++);
            int32_t payload_len = outer_len;
            if (subtype == bson::binary_subtype::binary_old) [[unlikely]] {
               // Spec: 0x02 wraps the payload in a redundant inner int32
               // length. Strip it so $binary base64 reflects only the real
               // data bytes.
               if (outer_len < 4) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               int32_t inner_len{};
               if (!read_le<int32_t>(ctx, it, end, inner_len)) return;
               if (inner_len < 0 || inner_len != outer_len - 4) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               payload_len = inner_len;
            }
            const uint8_t* payload = reinterpret_cast<const uint8_t*>(&*it);
            it += payload_len;

            emit_literal(ctx, out, ix, R"({"$binary":{"base64":")");
            if (bool(ctx.error)) return;
            write_base64_to(ctx, payload, static_cast<size_t>(payload_len), out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, R"(","subType":")");
            if (bool(ctx.error)) return;
            emit_hex_bytes(ctx, &subtype, 1, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, R"("}})");
            return;
         }
         case type::undefined:
            emit_literal(ctx, out, ix, R"({"$undefined":true})");
            return;
         case type::object_id: {
            if (static_cast<int64_t>(end - it) < 12) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint8_t* payload = reinterpret_cast<const uint8_t*>(&*it);
            emit_literal(ctx, out, ix, R"({"$oid":")");
            if (bool(ctx.error)) return;
            emit_hex_bytes(ctx, payload, 12, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, R"("})");
            it += 12;
            return;
         }
         case type::boolean: {
            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint8_t b = static_cast<uint8_t>(*it++);
            if (b == 0) {
               emit_literal(ctx, out, ix, "false");
            }
            else {
               emit_literal(ctx, out, ix, "true");
            }
            return;
         }
         case type::datetime: {
            int64_t ms{};
            if (!read_le<int64_t>(ctx, it, end, ms)) return;
            // Canonical Extended JSON v2: always wrap in {"$numberLong":"..."}
            // (Relaxed uses ISO-8601 for in-range values, which Glaze has no
            // formatter for; canonical form is valid in both modes.)
            emit_literal(ctx, out, ix, R"({"$date":{"$numberLong":")");
            if (bool(ctx.error)) return;
            to<JSON, int64_t>::template op<Opts>(ms, ctx, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, R"("}})");
            return;
         }
         case type::null:
            emit_literal(ctx, out, ix, "null");
            return;
         case type::regex: {
            std::string_view pattern{};
            if (!read_cstring(ctx, it, end, pattern)) return;
            std::string_view options{};
            if (!read_cstring(ctx, it, end, options)) return;
            emit_literal(ctx, out, ix, R"({"$regularExpression":{"pattern":)");
            if (bool(ctx.error)) return;
            emit_json_string<Opts>(ctx, pattern, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, R"(,"options":)");
            if (bool(ctx.error)) return;
            emit_json_string<Opts>(ctx, options, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, "}}");
            return;
         }
         case type::javascript: {
            int32_t len{};
            if (!read_le<int32_t>(ctx, it, end, len)) return;
            if (len < 1) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (static_cast<int64_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const auto n = static_cast<size_t>(len) - 1;
            std::string_view s{reinterpret_cast<const char*>(&*it), n};
            emit_literal(ctx, out, ix, R"({"$code":)");
            if (bool(ctx.error)) return;
            emit_json_string<Opts>(ctx, s, out, ix);
            if (bool(ctx.error)) return;
            emit_char(ctx, out, ix, '}');
            it += len;
            return;
         }
         case type::symbol: {
            int32_t len{};
            if (!read_le<int32_t>(ctx, it, end, len)) return;
            if (len < 1) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (static_cast<int64_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const auto n = static_cast<size_t>(len) - 1;
            std::string_view s{reinterpret_cast<const char*>(&*it), n};
            emit_literal(ctx, out, ix, R"({"$symbol":)");
            if (bool(ctx.error)) return;
            emit_json_string<Opts>(ctx, s, out, ix);
            if (bool(ctx.error)) return;
            emit_char(ctx, out, ix, '}');
            it += len;
            return;
         }
         case type::int32: {
            int32_t v{};
            if (!read_le<int32_t>(ctx, it, end, v)) return;
            to<JSON, int32_t>::template op<Opts>(v, ctx, out, ix);
            return;
         }
         case type::timestamp: {
            uint32_t increment{};
            uint32_t seconds{};
            if (!read_le<uint32_t>(ctx, it, end, increment)) return;
            if (!read_le<uint32_t>(ctx, it, end, seconds)) return;
            emit_literal(ctx, out, ix, R"({"$timestamp":{"t":)");
            if (bool(ctx.error)) return;
            to<JSON, uint32_t>::template op<Opts>(seconds, ctx, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, R"(,"i":)");
            if (bool(ctx.error)) return;
            to<JSON, uint32_t>::template op<Opts>(increment, ctx, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, "}}");
            return;
         }
         case type::int64: {
            int64_t v{};
            if (!read_le<int64_t>(ctx, it, end, v)) return;
            to<JSON, int64_t>::template op<Opts>(v, ctx, out, ix);
            return;
         }
         case type::decimal128: {
            if (static_cast<int64_t>(end - it) < 16) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint8_t* payload = reinterpret_cast<const uint8_t*>(&*it);
            // Extended JSON v2 $numberDecimal requires a decimal string, which
            // Glaze has no decimal128 formatter for. Emit under a non-`$`
            // wrapper so this doesn't masquerade as real $numberDecimal.
            emit_literal(ctx, out, ix, R"({"decimal128Hex":")");
            if (bool(ctx.error)) return;
            emit_hex_bytes(ctx, payload, 16, out, ix);
            if (bool(ctx.error)) return;
            emit_literal(ctx, out, ix, R"("})");
            it += 16;
            return;
         }
         case type::min_key:
            emit_literal(ctx, out, ix, R"({"$minKey":1})");
            return;
         case type::max_key:
            emit_literal(ctx, out, ix, R"({"$maxKey":1})");
            return;
         case type::db_pointer:
         case type::code_w_scope:
         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   } // namespace bson_detail

   // Convert a BSON document (top-level) to JSON text.
   template <auto Opts = glz::opts{}, class BSONBuffer, class JSONBuffer>
   [[nodiscard]] inline error_ctx bson_to_json(const BSONBuffer& input, JSONBuffer& out)
   {
      size_t ix{};
      context ctx{};

      const char* it = reinterpret_cast<const char*>(std::data(input));
      const char* const end = it + std::size(input);

      if (end - it < 5) {
         return {0, error_code::unexpected_end};
      }

      const char* stop{};
      if (!bson_detail::read_document_stop(ctx, it, end, stop)) {
         return {0, ctx.error};
      }

      bson_detail::bson_to_json_document_body<Opts>(ctx, it, stop, out, ix);
      if (bool(ctx.error)) {
         return {ix, ctx.error, ctx.custom_error_message};
      }
      if (it != stop) {
         return {ix, error_code::syntax_error};
      }
      // Trailing bytes beyond the advertised document length are silently
      // permitted by a literal read of `stop`, but a well-formed input must
      // consume the whole buffer (matches read_bson's exact-fill behavior).
      if (stop != end) {
         return {ix, error_code::syntax_error, "trailing bytes after document terminator"};
      }

      if constexpr (resizable<JSONBuffer>) {
         out.resize(ix);
      }
      return {};
   }

   template <auto Opts = glz::opts{}, class BSONBuffer>
   [[nodiscard]] inline expected<std::string, error_ctx> bson_to_json(const BSONBuffer& input)
   {
      std::string out;
      auto ec = bson_to_json<Opts>(input, out);
      if (ec) {
         return unexpected(ec);
      }
      return out;
   }
}
