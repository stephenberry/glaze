// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "glaze/core/opts.hpp"
#include "glaze/json/write.hpp"
#include "glaze/jsonb/header.hpp"

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
      inline void jsonb_to_json_value(is_context auto& ctx, const uint8_t*& it, const uint8_t* end, B& out, size_t& ix);

      template <auto Opts, class B>
      inline void jsonb_to_json_container(is_context auto& ctx, const uint8_t* it, const uint8_t* stop, B& out,
                                          size_t& ix, char open, char close)
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
            jsonb_to_json_value<Opts>(ctx, it, stop, out, ix);
            if (bool(ctx.error)) return;

            // For objects, the next child is a key's matching value: emit ':' between key and value.
            if (close == '}') {
               if (it >= stop) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if (!ensure_space(ctx, out, ix + 1 + write_padding_bytes)) return;
               out[ix++] = static_cast<typename std::decay_t<B>::value_type>(':');
               jsonb_to_json_value<Opts>(ctx, it, stop, out, ix);
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
      inline void jsonb_to_json_value(is_context auto& ctx, const uint8_t*& it, const uint8_t* end, B& out, size_t& ix)
      {
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
         case jsonb::type::null_:
            if (sz != 0) {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (!ensure_space(ctx, out, ix + 4 + write_padding_bytes)) return;
            std::memcpy(&out[ix], "null", 4);
            ix += 4;
            return;
         case jsonb::type::true_:
            if (sz != 0) {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (!ensure_space(ctx, out, ix + 4 + write_padding_bytes)) return;
            std::memcpy(&out[ix], "true", 4);
            ix += 4;
            return;
         case jsonb::type::false_:
            if (sz != 0) {
               ctx.error = error_code::syntax_error;
               return;
            }
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
            long long value = 0;
            if (e - p >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
               unsigned long long tmp = 0;
               auto [ptr, ec] = std::from_chars(p + 2, e, tmp, 16);
               if (ec != std::errc{} || ptr != e) {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               value = neg ? -static_cast<long long>(tmp) : static_cast<long long>(tmp);
            }
            else {
               long long tmp = 0;
               auto [ptr, ec] = std::from_chars(p, e, tmp, 10);
               if (ec != std::errc{} || ptr != e) {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               value = neg ? -tmp : tmp;
            }
            to<JSON, long long>::template op<Opts>(value, ctx, out, ix);
            return;
         }
         case jsonb::type::float5: {
            sv s{reinterpret_cast<const char*>(payload), static_cast<size_t>(sz)};
            if (s == "NaN" || s == "Infinity" || s == "+Infinity" || s == "-Infinity") {
               // Strict JSON has no representation; emit null.
               if (!ensure_space(ctx, out, ix + 4 + write_padding_bytes)) return;
               std::memcpy(&out[ix], "null", 4);
               ix += 4;
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
         case jsonb::type::textraw:
            // Raw bytes — run through the JSON string escape writer.
            emit_raw_string_as_json<Opts>(ctx, reinterpret_cast<const char*>(payload), static_cast<size_t>(sz), out, ix);
            return;
         case jsonb::type::textj:
            // Already valid JSON string body — emit wrapped in quotes.
            emit_json_escaped_body(ctx, reinterpret_cast<const char*>(payload), static_cast<size_t>(sz), out, ix);
            return;
         case jsonb::type::text5:
            // JSON5 escapes are a superset. Best effort: emit as a JSON string body. Callers
            // that know the payload is strictly JSON-compatible can rely on this; otherwise
            // output may include JSON5 escapes that don't round-trip as strict JSON.
            emit_json_escaped_body(ctx, reinterpret_cast<const char*>(payload), static_cast<size_t>(sz), out, ix);
            return;
         case jsonb::type::array: {
            const uint8_t* arr_stop = payload + sz;
            const uint8_t* ait = payload;
            jsonb_to_json_container<Opts>(ctx, ait, arr_stop, out, ix, '[', ']');
            return;
         }
         case jsonb::type::object: {
            const uint8_t* obj_stop = payload + sz;
            const uint8_t* oit = payload;
            jsonb_to_json_container<Opts>(ctx, oit, obj_stop, out, ix, '{', '}');
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

      jsonb_detail::jsonb_to_json_value<Opts>(ctx, it, end, out, ix);
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
