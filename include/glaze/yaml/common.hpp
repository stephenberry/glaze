// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "glaze/core/common.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/yaml/opts.hpp"

namespace glz::yaml
{
   // Lookup table for characters that can start a plain scalar in flow context
   // In flow context, these are NOT allowed: [ ] { } , : # ' " | > @ ` \n \r
   inline constexpr std::array<bool, 256> can_start_plain_flow_table = [] {
      std::array<bool, 256> t{};
      // Initialize all printable ASCII to true, then disable specific chars
      for (int i = 0x20; i <= 0x7E; ++i) {
         t[i] = true;
      }
      // Also allow high bytes (UTF-8 continuation/start)
      for (int i = 0x80; i <= 0xFF; ++i) {
         t[i] = true;
      }
      // Disable forbidden characters
      t['['] = false;
      t[']'] = false;
      t['{'] = false;
      t['}'] = false;
      t[','] = false;
      t[':'] = false;
      t['#'] = false;
      t['\''] = false;
      t['"'] = false;
      t['|'] = false;
      t['>'] = false;
      t['@'] = false;
      t['`'] = false;
      // Control characters already false by default
      return t;
   }();

   // Lookup table for characters that can start a plain scalar in block context
   // In block context, fewer restrictions: # ' " | > @ ` [ { \n \r
   inline constexpr std::array<bool, 256> can_start_plain_block_table = [] {
      std::array<bool, 256> t{};
      // Initialize all printable ASCII to true
      for (int i = 0x20; i <= 0x7E; ++i) {
         t[i] = true;
      }
      // Also allow high bytes (UTF-8)
      for (int i = 0x80; i <= 0xFF; ++i) {
         t[i] = true;
      }
      // Disable forbidden characters
      t['#'] = false;
      t['\''] = false;
      t['"'] = false;
      t['|'] = false;
      t['>'] = false;
      t['@'] = false;
      t['`'] = false;
      t['['] = false;
      t['{'] = false;
      // Control characters already false by default
      return t;
   }();

   // Lookup table for YAML indicator characters that need quoting
   inline constexpr std::array<bool, 256> yaml_indicator_table = [] {
      std::array<bool, 256> t{};
      t['-'] = true;
      t['?'] = true;
      t[':'] = true;
      t[','] = true;
      t['['] = true;
      t[']'] = true;
      t['{'] = true;
      t['}'] = true;
      t['#'] = true;
      t['&'] = true;
      t['*'] = true;
      t['!'] = true;
      t['|'] = true;
      t['>'] = true;
      t['\''] = true;
      t['"'] = true;
      t['%'] = true;
      t['@'] = true;
      t['`'] = true;
      return t;
   }();

   // YAML escape character table for double-quoted strings
   // Maps escape char to its actual value (0 means invalid/special handling needed)
   inline constexpr std::array<char, 256> yaml_unescape_table = [] {
      std::array<char, 256> t{};
      t['"'] = '"';
      t['\\'] = '\\';
      t['/'] = '/';
      t['a'] = '\a'; // bell
      t['b'] = '\b'; // backspace
      t['t'] = '\t'; // tab
      t['n'] = '\n'; // newline
      t['v'] = '\v'; // vertical tab
      t['f'] = '\f'; // form feed
      t['r'] = '\r'; // carriage return
      t['e'] = '\x1B'; // escape
      t[' '] = ' '; // space
      t['0'] = '\0'; // null
      // Note: x, u, U require special handling (hex parsing)
      // Note: N, _, L, P require special handling (multi-byte UTF-8)
      return t;
   }();

   // Table indicating which YAML escapes need special multi-byte handling
   // N (U+0085), _ (U+00A0), L (U+2028), P (U+2029)
   inline constexpr std::array<bool, 256> yaml_escape_needs_special = [] {
      std::array<bool, 256> t{};
      t['x'] = true; // \xXX
      t['u'] = true; // \uXXXX
      t['U'] = true; // \UXXXXXXXX
      t['N'] = true; // next line U+0085
      t['_'] = true; // non-breaking space U+00A0
      t['L'] = true; // line separator U+2028
      t['P'] = true; // paragraph separator U+2029
      return t;
   }();

   // Scalar style detection
   enum struct scalar_style : uint8_t {
      plain, // unquoted
      single_quoted, // 'string'
      double_quoted, // "string"
      literal_block, // |
      folded_block, // >
   };

   // Skip inline whitespace (spaces and tabs only - NOT newlines)
   template <class It, class End>
   GLZ_ALWAYS_INLINE void skip_inline_ws(It&& it, End end) noexcept
   {
      while (it != end && (*it == ' ' || *it == '\t')) {
         ++it;
      }
   }

   // Skip a comment to end of line (does not consume the newline)
   template <class It, class End>
   GLZ_ALWAYS_INLINE void skip_comment(It&& it, End end) noexcept
   {
      if (it != end && *it == '#') {
         while (it != end && *it != '\n' && *it != '\r') {
            ++it;
         }
      }
   }

   // Skip inline whitespace and any trailing comment
   template <class It, class End>
   GLZ_ALWAYS_INLINE void skip_ws_and_comment(It&& it, End end) noexcept
   {
      skip_inline_ws(it, end);
      skip_comment(it, end);
   }

   // Skip a newline sequence (handles \n, \r, \r\n)
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool skip_newline(It&& it, End end) noexcept
   {
      if (it == end) return false;

      if (*it == '\r') {
         ++it;
         if (it != end && *it == '\n') {
            ++it;
         }
         return true;
      }
      else if (*it == '\n') {
         ++it;
         return true;
      }
      return false;
   }

   // Check if at newline or end
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool at_newline_or_end(It&& it, End end) noexcept
   {
      return it == end || *it == '\n' || *it == '\r';
   }

   // Skip document start marker (---) if present at beginning
   // Handles: ---, ---\n, --- comment\n, etc.
   template <class It, class End>
   GLZ_ALWAYS_INLINE void skip_document_start(It&& it, End end) noexcept
   {
      // Skip any leading whitespace and newlines
      while (it != end && (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r')) {
         if (*it == '\n' || *it == '\r') {
            skip_newline(it, end);
         }
         else {
            ++it;
         }
      }

      // Check for ---
      if (end - it >= 3 && it[0] == '-' && it[1] == '-' && it[2] == '-') {
         auto after = it + 3;
         // Must be followed by whitespace, newline, or end
         if (after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r' || *after == '#') {
            it = after;
            // Skip rest of line (whitespace and optional comment)
            skip_ws_and_comment(it, end);
            skip_newline(it, end);
         }
      }
   }

   // Check if at document end marker (...)
   // Returns true if at ... followed by whitespace/newline/end
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool at_document_end(It&& it, End end) noexcept
   {
      if (end - it >= 3 && it[0] == '.' && it[1] == '.' && it[2] == '.') {
         auto after = it + 3;
         // Must be followed by whitespace, newline, or end
         if (after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r' || *after == '#') {
            return true;
         }
      }
      return false;
   }

   // Measure indentation at current position (assumes at start of line)
   // Returns number of spaces. Sets error if tabs are encountered (YAML forbids tabs in indentation)
   template <class It, class End, class Ctx>
   GLZ_ALWAYS_INLINE int32_t measure_indent(It&& it, End end, Ctx& ctx) noexcept
   {
      int32_t indent = 0;
      while (it != end && *it == ' ') {
         ++indent;
         ++it;
      }
      // YAML spec: "Tab characters must not be used for indentation"
      if (it != end && *it == '\t') {
         ctx.error = error_code::syntax_error;
      }
      return indent;
   }

   // Skip to next line and return new indentation level
   template <class It, class End, class Ctx>
   GLZ_ALWAYS_INLINE int32_t skip_to_next_content_line(It&& it, End end, Ctx& ctx) noexcept
   {
      while (it != end) {
         // Skip to end of current line
         while (it != end && *it != '\n' && *it != '\r') {
            ++it;
         }

         // Skip newline
         if (!skip_newline(it, end)) {
            return -1; // EOF
         }

         // Measure indent of new line
         auto start = it;
         int32_t indent = measure_indent(it, end, ctx);
         if (bool(ctx.error)) return -1;

         // Check if this is a content line (not blank, not comment-only)
         skip_inline_ws(it, end);
         if (it != end && *it != '\n' && *it != '\r' && *it != '#') {
            it = start; // Reset to start of line
            return indent;
         }

         // Skip blank/comment lines
         skip_comment(it, end);
      }
      return -1; // EOF
   }

   // Check for unsupported YAML features (anchors & and aliases *)
   // Returns true if an unsupported feature is detected and sets error
   template <class Ctx>
   GLZ_ALWAYS_INLINE bool check_unsupported_feature(char c, Ctx& ctx) noexcept
   {
      if (c == '&' || c == '*') {
         ctx.error = error_code::feature_not_supported;
         return true;
      }
      return false;
   }

   // Detect scalar style from first character
   GLZ_ALWAYS_INLINE constexpr scalar_style detect_scalar_style(char c) noexcept
   {
      switch (c) {
      case '"':
         return scalar_style::double_quoted;
      case '\'':
         return scalar_style::single_quoted;
      case '|':
         return scalar_style::literal_block;
      case '>':
         return scalar_style::folded_block;
      default:
         return scalar_style::plain;
      }
   }

   // Check if character can start a plain scalar in flow context
   GLZ_ALWAYS_INLINE constexpr bool can_start_plain_flow(char c) noexcept
   {
      return can_start_plain_flow_table[static_cast<uint8_t>(c)];
   }

   // Check if character can start a plain scalar in block context
   GLZ_ALWAYS_INLINE constexpr bool can_start_plain_block(char c) noexcept
   {
      return can_start_plain_block_table[static_cast<uint8_t>(c)];
   }

   // Check if string looks like a boolean
   GLZ_ALWAYS_INLINE bool is_yaml_bool(std::string_view s) noexcept
   {
      return s == "true" || s == "false" || s == "True" || s == "False" || s == "TRUE" || s == "FALSE";
   }

   // Check if string looks like null
   GLZ_ALWAYS_INLINE bool is_yaml_null(std::string_view s) noexcept
   {
      return s == "null" || s == "Null" || s == "NULL" || s == "~" || s.empty();
   }

   // Check if character is a YAML indicator that needs quoting
   GLZ_ALWAYS_INLINE constexpr bool is_yaml_indicator(char c) noexcept
   {
      return yaml_indicator_table[static_cast<uint8_t>(c)];
   }

   // Check if string needs quoting when written
   GLZ_ALWAYS_INLINE bool needs_quoting(std::string_view s) noexcept
   {
      if (s.empty()) return true;

      // Check first character
      char first = s[0];
      if (is_yaml_indicator(first) || first == ' ' || first == '\t') {
         return true;
      }

      // Check if it looks like a special value
      if (is_yaml_bool(s) || is_yaml_null(s)) {
         return true;
      }

      // Check for characters that require quoting
      for (char c : s) {
         if (c == ':' || c == '#' || c == '\n' || c == '\r' || c == '\t') {
            return true;
         }
      }

      // Check if it looks like a number
      if (std::isdigit(first) || first == '-' || first == '+' || first == '.') {
         return true;
      }

      return false;
   }

   // Write indentation
   template <class B>
   GLZ_ALWAYS_INLINE void write_indent(B&& b, auto& ix, int32_t level, uint8_t width = 2)
   {
      const int32_t spaces = level * width;
      for (int32_t i = 0; i < spaces; ++i) {
         b[ix++] = ' ';
      }
   }

} // namespace glz::yaml
