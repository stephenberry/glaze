// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.jsonb.jsonb_to_json;

import std;

import glaze.jsonb.header;
import glaze.jsonb.text_decode;

import glaze.core.buffer_traits;
import glaze.core.common;
import glaze.core.context;
import glaze.core.opts;

import glaze.json.write;

import glaze.util.expected;
import glaze.util.string_literal;

import glaze.concepts.container_concepts;

using std::int64_t;
using std::uint8_t;
using std::uint32_t;
using std::uint64_t;
using std::size_t;

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
            // Already valid JSON number text.
            if (!ensure_space(ctx, out, ix + sz + write_padding_bytes)) return;
            if (sz) std::memcpy(&out[ix], payload, sz);
            ix += sz;
            return;
         }
         case jsonb::type::int5: {
            // Parse and re-emit as decimal so the output is strict JSON.
            sv s{reinterpret_cast<const char*>(payload), static_cast<size_t>(sz)};
            // Strip leading '+'.
            const char* p = s.data();
            const char* e = p + s.size();
            bool neg = false;
            if (p < e && *p == '+') ++p;
            if (p < e && *p == '-') {
               neg = true;
               ++p;
            }
            int64_t value = 0;
            if (e - p >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
               uint64_t tmp = 0;
               auto [ptr, ec] = std::from_chars(p + 2, e, tmp, 16);
               if (ec != std::errc{} || ptr != e) {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               value = neg ? -static_cast<int64_t>(tmp) : static_cast<int64_t>(tmp);
            }
            else {
               int64_t tmp = 0;
               auto [ptr, ec] = std::from_chars(p, e, tmp, 10);
               if (ec != std::errc{} || ptr != e) {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               value = neg ? -tmp : tmp;
            }
            to<JSON, int64_t>::template op<Opts>(value, ctx, out, ix);
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
            // Otherwise, best effort: strip leading '+'.
            const char* p = s.data();
            const char* e = p + s.size();
            if (p < e && *p == '+') ++p;
            const size_t n = static_cast<size_t>(e - p);
            if (!ensure_space(ctx, out, ix + n + write_padding_bytes)) return;
            if (n) std::memcpy(&out[ix], p, n);
            ix += n;
            return;
         }
         case jsonb::type::text:
            // Spec: TEXT payload is already a valid JSON string body (no control chars,
            // no unescaped " or \), so wrap in quotes and memcpy — no scan needed.
            emit_json_escaped_body(ctx, reinterpret_cast<const char*>(payload), static_cast<size_t>(sz), out, ix);
            return;
         case jsonb::type::textraw:
            // Raw bytes that may require escaping — run through the JSON string writer.
            emit_raw_string_as_json<Opts>(ctx, reinterpret_cast<const char*>(payload), static_cast<size_t>(sz), out,
                                          ix);
            return;
         case jsonb::type::textj: {
            // TEXTJ payload is already a valid JSON string body by spec. Emitting it
            // verbatim wrapped in quotes is correct.
            emit_json_escaped_body(ctx, reinterpret_cast<const char*>(payload), static_cast<size_t>(sz), out, ix);
            return;
         }
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
   export template <auto Opts = glz::opts{}, class JSONBBuffer, class JSONBuffer>
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

   export template <auto Opts = glz::opts{}, class JSONBBuffer>
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
