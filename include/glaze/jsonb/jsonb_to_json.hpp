// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

#include "glaze/core/opts.hpp"
#include "glaze/json/write.hpp"
#include "glaze/jsonb/header.hpp"
#include "glaze/jsonb/text_decode.hpp"
#include "glaze/util/fast_float.hpp"

namespace glz
{
   namespace jsonb_detail
   {
      // Emit a JSON string literal from a raw UTF-8 byte payload. Uses the JSON writer so all
      // control characters and structural JSON chars are correctly escaped.
      template <auto Opts, class B>
      inline void emit_raw_string_as_json(is_context auto& ctx, const char* data, size_t size, B& out, size_t& ix)
      {
         const sv s{data, size};
         to<JSON, sv>::template op<Opts>(s, ctx, out, ix);
      }

      // The spec marks several payloads as "already valid JSON text" so that a converter can
      // copy them into the output document rather than re-encoding them. A blob from a
      // foreign producer need not honor that, and a payload that does not is not merely
      // rendered wrong: it is spliced into the document as structure, so a single scalar can
      // introduce object members that were never in the blob. The two predicates below check
      // the claim before any such copy.

      // True if [data, data+size) can stand verbatim as the body of a JSON string literal
      // (RFC 8259 section 7): no raw control character, no unescaped quote, and every
      // backslash introduces a well-formed escape. Surrogate escapes have to be properly
      // paired, matching what decode_json_escape enforces on the reader side.
      [[nodiscard]] inline bool is_json_string_body(const char* data, size_t size) noexcept
      {
         // Reads the four hex digits at [pos, pos+4), or -1 if they are not all hex.
         auto hex4 = [&](size_t pos) -> int32_t {
            if (size - pos < 4) {
               return -1;
            }
            uint32_t v = 0;
            for (size_t k = 0; k < 4; ++k) {
               const auto h = static_cast<uint8_t>(data[pos + k]);
               uint32_t d;
               if (h >= '0' && h <= '9')
                  d = h - '0';
               else if (h >= 'a' && h <= 'f')
                  d = h - 'a' + 10;
               else if (h >= 'A' && h <= 'F')
                  d = h - 'A' + 10;
               else
                  return -1;
               v = (v << 4) | d;
            }
            return static_cast<int32_t>(v);
         };

         for (size_t i = 0; i < size; ++i) {
            const auto c = static_cast<uint8_t>(data[i]);
            if (c < 0x20 || c == '"') {
               return false;
            }
            if (c == '\\') {
               if (++i >= size) {
                  return false;
               }
               switch (data[i]) {
               case '"':
               case '\\':
               case '/':
               case 'b':
               case 'f':
               case 'n':
               case 'r':
               case 't':
                  break;
               case 'u': {
                  const int32_t cp = hex4(i + 1);
                  if (cp < 0) {
                     return false;
                  }
                  i += 4;
                  if (cp >= 0xDC00 && cp <= 0xDFFF) {
                     return false; // low surrogate without a preceding high one
                  }
                  if (cp >= 0xD800 && cp <= 0xDBFF) {
                     // A high surrogate has to be followed by \uDC00..\uDFFF.
                     if (size - i < 7 || data[i + 1] != '\\' || data[i + 2] != 'u') {
                        return false;
                     }
                     const int32_t low = hex4(i + 3);
                     if (low < 0xDC00 || low > 0xDFFF) {
                        return false;
                     }
                     i += 6;
                  }
                  break;
               }
               default:
                  return false;
               }
            }
         }
         return true;
      }

      // True if [data, data+size) is a complete RFC 8259 section 6 number.
      [[nodiscard]] inline bool is_json_number(const char* data, size_t size) noexcept
      {
         size_t i = 0;
         auto digits = [&] {
            const size_t start = i;
            while (i < size && data[i] >= '0' && data[i] <= '9') {
               ++i;
            }
            return i > start;
         };

         if (i < size && data[i] == '-') {
            ++i;
         }
         if (i < size && data[i] == '0') {
            ++i; // a leading zero may not be followed by more integer digits
         }
         else if (!digits()) {
            return false;
         }
         if (i < size && data[i] == '.') {
            ++i;
            if (!digits()) {
               return false;
            }
         }
         if (i < size && (data[i] == 'e' || data[i] == 'E')) {
            ++i;
            if (i < size && (data[i] == '+' || data[i] == '-')) {
               ++i;
            }
            if (!digits()) {
               return false;
            }
         }
         return i == size;
      }

      // Emit a string whose bytes are already a valid JSON string literal body (i.e. with
      // RFC 8259 escapes already in the payload). We just wrap in quotes.
      template <class B>
      inline void emit_json_escaped_body(is_context auto& ctx, const char* data, size_t size, B& out, size_t& ix)
      {
         if (!ensure_space(ctx, out, ix + size + 2 + write_padding_bytes)) return;
         out[ix++] = static_cast<typename std::decay_t<B>::value_type>('"');
         if (size) {
            std::memcpy(&out[ix], data, size);
            ix += size;
         }
         out[ix++] = static_cast<typename std::decay_t<B>::value_type>('"');
      }

      template <auto Opts, class B>
      inline void jsonb_to_json_value(is_context auto& ctx, const uint8_t*& it, const uint8_t* end, B& out, size_t& ix,
                                      uint32_t depth);

      template <auto Opts, class B>
      inline void jsonb_to_json_container(is_context auto& ctx, const uint8_t* it, const uint8_t* stop, B& out,
                                          size_t& ix, char open, char close, uint32_t depth)
      {
         if (!ensure_space(ctx, out, ix + 2 + write_padding_bytes)) return;
         out[ix++] = static_cast<typename std::decay_t<B>::value_type>(open);
         bool first = true;
         while (it < stop) {
            if (!first) {
               if (!ensure_space(ctx, out, ix + 1 + write_padding_bytes)) return;
               out[ix++] = static_cast<typename std::decay_t<B>::value_type>(',');
            }
            first = false;
            if (close == '}') {
               // An object key has to be one of the text types (7..10). Any other type here
               // would be rendered as a bare unquoted key, e.g. `{{}:false}`.
               const uint8_t key_type = jsonb::get_type(static_cast<uint8_t>(*it));
               if (key_type < jsonb::type::text || key_type > jsonb::type::textraw) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            jsonb_to_json_value<Opts>(ctx, it, stop, out, ix, depth);
            if (bool(ctx.error)) return;

            // For objects, the next child is a key's matching value: emit ':' between key and value.
            if (close == '}') {
               if (it >= stop) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if (!ensure_space(ctx, out, ix + 1 + write_padding_bytes)) return;
               out[ix++] = static_cast<typename std::decay_t<B>::value_type>(':');
               jsonb_to_json_value<Opts>(ctx, it, stop, out, ix, depth);
               if (bool(ctx.error)) return;
            }
         }
         if (it != stop) {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!ensure_space(ctx, out, ix + 1 + write_padding_bytes)) return;
         out[ix++] = static_cast<typename std::decay_t<B>::value_type>(close);
      }

      template <auto Opts, class B>
      inline void jsonb_to_json_value(is_context auto& ctx, const uint8_t*& it, const uint8_t* end, B& out, size_t& ix,
                                      uint32_t depth)
      {
         // DoS protection: cap recursion on pathologically nested blobs so untrusted input
         // can't blow the stack. Only containers bump depth below; scalar emission leaves
         // it alone. Matches the limit used by CBOR's converter.
         if (depth >= max_recursive_depth_limit) [[unlikely]] {
            ctx.error = error_code::exceeded_max_recursive_depth;
            return;
         }
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (static_cast<uint64_t>(end - it) < sz) {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const uint8_t* payload = it;
         it += sz;

         switch (tc) {
         // Spec: legacy implementations must interpret type 0/1/2 as null/true/false
         // even when the payload size is non-zero (forward-compatibility for future spec
         // extensions). Payload bytes are already skipped by `it += sz` above.
         case jsonb::type::null_:
            if (!ensure_space(ctx, out, ix + 4 + write_padding_bytes)) return;
            std::memcpy(&out[ix], "null", 4);
            ix += 4;
            return;
         case jsonb::type::true_:
            if (!ensure_space(ctx, out, ix + 4 + write_padding_bytes)) return;
            std::memcpy(&out[ix], "true", 4);
            ix += 4;
            return;
         case jsonb::type::false_:
            if (!ensure_space(ctx, out, ix + 5 + write_padding_bytes)) return;
            std::memcpy(&out[ix], "false", 5);
            ix += 5;
            return;
         case jsonb::type::int_:
         case jsonb::type::float_: {
            // Already valid JSON number text, so the payload is copied rather than parsed
            // and re-rendered, which keeps the blob's exact decimal representation. Confirm
            // the claim first: the blob is untrusted.
            if (!is_json_number(reinterpret_cast<const char*>(payload), static_cast<size_t>(sz))) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            if (!ensure_space(ctx, out, ix + sz + write_padding_bytes)) return;
            if (sz) std::memcpy(&out[ix], payload, sz);
            ix += sz;
            return;
         }
         case jsonb::type::int5: {
            // Parse and re-emit as strict JSON. The magnitude is accumulated as an
            // unsigned value so a negative literal whose magnitude has the sign bit
            // set (e.g. "-0x8000000000000000") negates without the signed-overflow
            // UB, and a positive literal above INT64_MAX ("0xFFFFFFFFFFFFFFFF") keeps
            // its value instead of wrapping to a negative int. Mirrors the reader's
            // parse_int_payload in jsonb/read.hpp.
            sv s{reinterpret_cast<const char*>(payload), static_cast<size_t>(sz)};
            const char* p = s.data();
            const char* e = p + s.size();
            bool neg = false;
            if (p < e && *p == '+') ++p;
            if (p < e && *p == '-') {
               neg = true;
               ++p;
            }
            const int base = (e - p >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) ? 16 : 10;
            const char* digits = (base == 16) ? p + 2 : p;
            uint64_t mag = 0;
            auto [ptr, ec] = std::from_chars(digits, e, mag, base);
            if (ec != std::errc{} || ptr != e) {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            if (neg) {
               // |INT64_MIN| == INT64_MAX + 1, so that magnitude is still representable.
               constexpr uint64_t max_neg_mag = static_cast<uint64_t>((std::numeric_limits<int64_t>::max)()) + 1u;
               if (mag > max_neg_mag) {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               to<JSON, int64_t>::template op<Opts>(static_cast<int64_t>(uint64_t{0} - mag), ctx, out, ix);
            }
            else {
               to<JSON, uint64_t>::template op<Opts>(mag, ctx, out, ix);
            }
            return;
         }
         case jsonb::type::float5: {
            sv s{reinterpret_cast<const char*>(payload), static_cast<size_t>(sz)};
            if (s == "NaN") {
               // Strict JSON has no NaN representation; emit null (matches SQLite json()).
               if (!ensure_space(ctx, out, ix + 4 + write_padding_bytes)) return;
               std::memcpy(&out[ix], "null", 4);
               ix += 4;
               return;
            }
            if (s == "Infinity" || s == "+Infinity") {
               // Match SQLite json(): emit 9e999, which parses as +infinity in IEEE-754 binary64.
               if (!ensure_space(ctx, out, ix + 5 + write_padding_bytes)) return;
               std::memcpy(&out[ix], "9e999", 5);
               ix += 5;
               return;
            }
            if (s == "-Infinity") {
               if (!ensure_space(ctx, out, ix + 6 + write_padding_bytes)) return;
               std::memcpy(&out[ix], "-9e999", 6);
               ix += 6;
               return;
            }
            // What is left is JSON5 float syntax that strict JSON does not share, so parse it
            // and re-render rather than copying it out. A leading '+' or '.' and a trailing
            // '.' are all legal here and none of them are legal JSON. Mirrors the reader's
            // parse_float_payload in jsonb/read.hpp, including its use of fast_float over
            // std::from_chars for the floating-point overload.
            const char* p = s.data();
            const char* e = p + s.size();
            if (p < e && *p == '+') ++p;
            double d{};
            auto [ptr, ec] = glz::fast_float::from_chars(p, e, d);
            if (ec != std::errc{} || ptr != e) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            to<JSON, double>::template op<Opts>(d, ctx, out, ix);
            return;
         }
         case jsonb::type::text:
         case jsonb::type::textj: {
            // Spec: a TEXT or TEXTJ payload is already a valid JSON string body (no control
            // chars, no unescaped " or \), so wrap in quotes and memcpy — no unescaping
            // needed. The blob is untrusted, so confirm it holds to that before copying it
            // out. Decoding the escapes would not be enough on its own: an unescaped quote
            // is not an escape error, it just ends the string a character early.
            const auto* body = reinterpret_cast<const char*>(payload);
            const auto n = static_cast<size_t>(sz);
            if (!is_json_string_body(body, n)) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            emit_json_escaped_body(ctx, body, n, out, ix);
            return;
         }
         case jsonb::type::textraw:
            // Raw bytes that may require escaping — run through the JSON string writer.
            emit_raw_string_as_json<Opts>(ctx, reinterpret_cast<const char*>(payload), static_cast<size_t>(sz), out,
                                          ix);
            return;
         case jsonb::type::text5: {
            // TEXT5 payloads may contain JSON5-only escapes (\xNN, \', \v, \0, line
            // continuations) that are not valid JSON. Decode to raw UTF-8 and re-emit via
            // the JSON string writer so the output is always strict JSON.
            std::string scratch;
            jsonb_detail::decode_text(ctx, jsonb::type::text5, payload, payload + sz, static_cast<size_t>(sz), scratch);
            if (bool(ctx.error)) return;
            emit_raw_string_as_json<Opts>(ctx, scratch.data(), scratch.size(), out, ix);
            return;
         }
         case jsonb::type::array: {
            const uint8_t* arr_stop = payload + sz;
            const uint8_t* ait = payload;
            jsonb_to_json_container<Opts>(ctx, ait, arr_stop, out, ix, '[', ']', depth + 1);
            return;
         }
         case jsonb::type::object: {
            const uint8_t* obj_stop = payload + sz;
            const uint8_t* oit = payload;
            jsonb_to_json_container<Opts>(ctx, oit, obj_stop, out, ix, '{', '}', depth + 1);
            return;
         }
         default:
            // Reserved types 13..15.
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   }

   // Convert a JSONB blob to JSON text.
   template <auto Opts = glz::opts{}, class JSONBBuffer, class JSONBuffer>
   [[nodiscard]] inline error_ctx jsonb_to_json(const JSONBBuffer& input, JSONBuffer& out)
   {
      size_t ix{};
      context ctx{};

      const uint8_t* it = reinterpret_cast<const uint8_t*>(input.data());
      const uint8_t* end = it + input.size();

      if (it >= end) {
         return {0, error_code::unexpected_end};
      }

      jsonb_detail::jsonb_to_json_value<Opts>(ctx, it, end, out, ix, 0);
      if (bool(ctx.error)) {
         return {0, ctx.error};
      }
      if (it != end) {
         return {0, error_code::syntax_error};
      }

      if constexpr (resizable<JSONBuffer>) {
         out.resize(ix);
      }
      return {};
   }

   template <auto Opts = glz::opts{}, class JSONBBuffer>
   [[nodiscard]] inline expected<std::string, error_ctx> jsonb_to_json(const JSONBBuffer& input)
   {
      std::string out;
      auto ec = jsonb_to_json<Opts>(input, out);
      if (ec) {
         return unexpected(ec);
      }
      return out;
   }
}
