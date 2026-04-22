// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.jsonb.text_decode;

import std;

import glaze.core.context;
import glaze.jsonb.header;
import glaze.util.parse;

// Text-variant escape decoders shared by the JSONB reader (from<JSONB, string>) and the
// JSONB→JSON converter (jsonb_to_json). TEXTJ carries RFC 8259 JSON escapes, TEXT5 carries
// JSON5 escapes (a superset with \xNN, \', line continuations, \v, \0); both must be
// unescape-decoded to raw UTF-8 before being used as a C++ string or re-emitted as a strict
// JSON string literal.

using std::int32_t;
using std::uint8_t;
using std::uint32_t;
using std::size_t;

namespace glz::jsonb_detail
{
   // Decode a JSON-style escape (TEXTJ) starting at `it` (pointing at the char AFTER the
   // backslash). Appends the decoded byte(s) to `out` and advances `it` past the escape.
   // Returns false on malformed escape.
   template <class It>
   [[nodiscard]] inline bool decode_json_escape(It& it, It end, std::string& out) noexcept
   {
      if (it >= end) return false;
      const char c = static_cast<char>(*it);
      ++it;
      switch (c) {
      case '"':
         out.push_back('"');
         return true;
      case '\\':
         out.push_back('\\');
         return true;
      case '/':
         out.push_back('/');
         return true;
      case 'b':
         out.push_back('\b');
         return true;
      case 'f':
         out.push_back('\f');
         return true;
      case 'n':
         out.push_back('\n');
         return true;
      case 'r':
         out.push_back('\r');
         return true;
      case 't':
         out.push_back('\t');
         return true;
      case 'u': {
         // 4 hex digits, possibly followed by a `\uXXXX` surrogate pair.
         auto hex4 = [](It p) -> int32_t {
            uint32_t v = 0;
            for (int i = 0; i < 4; ++i) {
               const char ch = static_cast<char>(p[i]);
               uint32_t d;
               if (ch >= '0' && ch <= '9')
                  d = ch - '0';
               else if (ch >= 'a' && ch <= 'f')
                  d = ch - 'a' + 10;
               else if (ch >= 'A' && ch <= 'F')
                  d = ch - 'A' + 10;
               else
                  return -1;
               v = (v << 4) | d;
            }
            return static_cast<int32_t>(v);
         };
         if ((end - it) < 4) return false;
         int32_t high = hex4(it);
         if (high < 0) return false;
         it += 4;
         uint32_t code_point = static_cast<uint32_t>(high);

         using namespace glz::unicode;
         if ((code_point & generic_surrogate_mask) == generic_surrogate_value) {
            if ((code_point & surrogate_mask) != high_surrogate_value) return false;
            if ((end - it) < 6) return false;
            if (static_cast<char>(it[0]) != '\\' || static_cast<char>(it[1]) != 'u') return false;
            it += 2;
            const int32_t low = hex4(it);
            if (low < 0) return false;
            it += 4;
            if ((static_cast<uint32_t>(low) & surrogate_mask) != low_surrogate_value) return false;
            code_point = ((code_point & surrogate_codepoint_mask) << surrogate_codepoint_bits) |
                         (static_cast<uint32_t>(low) & surrogate_codepoint_mask);
            code_point += surrogate_codepoint_offset;
         }

         char utf8[4];
         const uint32_t n = glz::code_point_to_utf8(code_point, utf8);
         if (n == 0) return false;
         out.append(utf8, n);
         return true;
      }
      default:
         return false;
      }
   }

   // Decode a JSON5-style escape. Handles all JSON escapes plus \x, \', line continuations,
   // \0, and \v.
   template <class It>
   [[nodiscard]] inline bool decode_json5_escape(It& it, It end, std::string& out) noexcept
   {
      if (it >= end) return false;
      const char c = static_cast<char>(*it);
      switch (c) {
      case '\'':
         ++it;
         out.push_back('\'');
         return true;
      case 'v':
         ++it;
         out.push_back('\v');
         return true;
      case '0':
         ++it;
         out.push_back('\0');
         return true;
      case 'x': {
         ++it;
         if ((end - it) < 2) return false;
         auto hex = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            return -1;
         };
         const int hi = hex(static_cast<char>(it[0]));
         const int lo = hex(static_cast<char>(it[1]));
         if (hi < 0 || lo < 0) return false;
         out.push_back(static_cast<char>((hi << 4) | lo));
         it += 2;
         return true;
      }
      case '\n':
         // Line continuation: \<LF> — consume, emit nothing.
         ++it;
         return true;
      case '\r':
         ++it;
         if (it < end && static_cast<char>(*it) == '\n') ++it; // eat CRLF
         return true;
      default:
         return decode_json_escape(it, end, out);
      }
   }

   // Read string bytes [it, it+size) into an out-string, applying the appropriate unescape
   // rule for the type code. TEXT and TEXTRAW pass through as raw UTF-8; TEXTJ decodes JSON
   // escapes; TEXT5 decodes JSON5 escapes.
   export template <class It>
   inline void decode_text(is_context auto& ctx, uint8_t type_code, It it, It end, size_t size,
                           std::string& out) noexcept
   {
      if (static_cast<size_t>(end - it) < size) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }
      const It stop = it + size;

      if (type_code == jsonb::type::text || type_code == jsonb::type::textraw) {
         out.assign(reinterpret_cast<const char*>(it), size);
         return;
      }

      out.clear();
      out.reserve(size);
      while (it < stop) {
         const char c = static_cast<char>(*it);
         if (c == '\\') {
            ++it;
            if (it >= stop) {
               ctx.error = error_code::syntax_error;
               return;
            }
            const bool ok = (type_code == jsonb::type::textj) ? decode_json_escape(it, stop, out)
                                                              : decode_json5_escape(it, stop, out);
            if (!ok) {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
         else {
            out.push_back(c);
            ++it;
         }
      }
   }
}
