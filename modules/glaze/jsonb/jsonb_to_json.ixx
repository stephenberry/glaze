// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/jsonb/jsonb_to_json.hpp"
// glz:header std=<charconv>
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<cstring>
// glz:header std=<limits>
// glz:header std=<string>
// glz:header std=<type_traits>
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
import glaze.util.parse;
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
      // Emit a JSON string literal from a raw UTF-8 byte payload, sharing the option pinning in
      // detail::emit_untrusted_string with every other binary-to-JSON converter.
      //
      // JSONB differs from those converters in one respect: a control character in a payload is
      // expected rather than anomalous, because a JSONB blob is the storage form of a JSON
      // document that could itself have carried \uXXXX. Escaping is pinned on here so the
      // converter keeps round-tripping such a document instead of refusing it, which is also the
      // behavior this converter shipped with.
      template <auto Opts, class B>
      inline void emit_raw_string_as_json(is_context auto& ctx, const char* data, size_t size, B& out, size_t& ix)
      {
         // RFC 8259 section 8.1 requires JSON text to be UTF-8, and these bytes came out of a
         // blob rather than out of the program, so they get the same treatment the JSON reader
         // gives its input. Honors the `validate_utf8` option, which compiles the check away.
         // No accumulator is available here: nothing has scanned this payload yet.
         if (validate_utf8_span<Opts>(ctx, data, data + size)) [[unlikely]] {
            return;
         }
         detail::emit_untrusted_string<opt_true<Opts, escape_control_characters_opt_tag{}>>(ctx, sv{data, size}, out,
                                                                                            ix);
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
      //
      // Every byte is visited here anyway, so each one is OR'd into `ascii_acc` on the way
      // past. A caller hands that to validate_utf8_span, which then skips the UTF-8 pass
      // outright for a pure ASCII payload — the case TEXT exists to make cheap.
      //
      // Note that validate_utf8_span documents the opposite bias for its accumulator: covering
      // extra bytes only costs a needless pass, whereas covering too few would skip a needed
      // one. So every byte is OR'd in unconditionally, including the ones consumed inside an
      // escape. Those are all checked to be ASCII anyway, but making the accumulator depend on
      // that would leave UTF-8 validation silently disabled if the escape set ever widened.
      [[nodiscard]] inline bool is_json_string_body(const char* data, size_t size, uint64_t& ascii_acc) noexcept
      {
         // Reads the four hex digits at [pos, pos+4), or -1 if they are not all hex.
         auto hex4 = [&](size_t pos) -> std::int32_t {
            if (size - pos < 4) {
               return -1;
            }
            uint32_t v = 0;
            for (size_t k = 0; k < 4; ++k) {
               const auto h = static_cast<uint8_t>(data[pos + k]);
               ascii_acc |= h;
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
            return static_cast<std::int32_t>(v);
         };

         for (size_t i = 0; i < size; ++i) {
            const auto c = static_cast<uint8_t>(data[i]);
            ascii_acc |= c;
            if (c < 0x20 || c == '"') {
               return false;
            }
            if (c == '\\') {
               if (++i >= size) {
                  return false;
               }
               ascii_acc |= static_cast<uint8_t>(data[i]);
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
                  const std::int32_t cp = hex4(i + 1);
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
                     ascii_acc |= static_cast<uint8_t>(data[i + 1]) | static_cast<uint8_t>(data[i + 2]);
                     const std::int32_t low = hex4(i + 3);
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
      // RFC 8259 escapes already in the payload). We just wrap in quotes. `ascii_acc` is the
      // OR of the payload's bytes, as produced by is_json_string_body.
      template <auto Opts, class B>
      inline void emit_json_escaped_body(is_context auto& ctx, const char* data, size_t size, uint64_t ascii_acc,
                                         B& out, size_t& ix)
      {
         // The escapes in the payload are ASCII, so only the raw bytes need the UTF-8 check;
         // validate_utf8_span skips it entirely when the accumulator shows pure ASCII.
         if (validate_utf8_span<Opts>(ctx, data, data + size, ascii_acc)) [[unlikely]] {
            return;
         }
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
            // What is left is JSON5 float syntax, which strict JSON does not share: a leading
            // '+' is legal here, and so is a '.' with no digit on one side of it. Normalize
            // those textually — supplying the digit the payload omits, the way SQLite's json()
            // does — rather than parsing to a double and re-rendering. Re-rendering would round
            // a payload carrying more precision than binary64 holds and would reject one whose
            // magnitude is outside its range, even though both are already valid JSON once the
            // JSON5-only spelling is gone. Whatever normalization produces still has to satisfy
            // the JSON grammar before it is copied out: the blob is untrusted, and this arm is
            // otherwise unconstrained.
            sv body = s;
            // Drop a leading '+' only when a number could actually follow it, so that a
            // payload like "+-1.5" is left intact and rejected below rather than quietly
            // becoming "-1.5".
            if (body.size() > 1 && body.front() == '+' && ((body[1] >= '0' && body[1] <= '9') || body[1] == '.')) {
               body.remove_prefix(1);
            }

            std::string scratch;
            if (const size_t dot = body.find('.'); dot != sv::npos) {
               const auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
               const bool has_int_digit = dot > 0 && is_digit(body[dot - 1]);
               const bool has_frac_digit = dot + 1 < body.size() && is_digit(body[dot + 1]);
               // Supply the missing digit only when the other side of the dot has one, so that
               // a payload which is not JSON5 either ("." or ".e5") is left alone and rejected
               // below rather than normalized into a number it never denoted.
               if (has_int_digit && !has_frac_digit) {
                  // "5." and "5.e3" omit the fraction digit.
                  scratch.reserve(body.size() + 1);
                  scratch.append(body.substr(0, dot + 1));
                  scratch.push_back('0');
                  scratch.append(body.substr(dot + 1));
                  body = scratch;
               }
               else if (has_frac_digit && !has_int_digit) {
                  // ".5" and "-.5" omit the integer digit.
                  scratch.reserve(body.size() + 1);
                  scratch.append(body.substr(0, dot));
                  scratch.push_back('0');
                  scratch.append(body.substr(dot));
                  body = scratch;
               }
            }

            if (!is_json_number(body.data(), body.size())) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            if (!ensure_space(ctx, out, ix + body.size() + write_padding_bytes)) return;
            if (!body.empty()) std::memcpy(&out[ix], body.data(), body.size());
            ix += body.size();
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
            uint64_t ascii_acc{};
            if (!is_json_string_body(body, n, ascii_acc)) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            emit_json_escaped_body<Opts>(ctx, body, n, ascii_acc, out, ix);
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
         return {ix, ctx.error};
      }
      if (it != end) {
         return {ix, error_code::syntax_error};
      }

      if constexpr (resizable<JSONBuffer>) {
         out.resize(ix);
      }
      // count is the number of bytes written. A resizable buffer carries its own size, but a
      // fixed-size one has no other way to learn how much of it now holds JSON.
      return {ix};
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
