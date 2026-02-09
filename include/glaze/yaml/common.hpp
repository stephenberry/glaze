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
   // YAML-specific context extending the base context
   // Adds indent tracking needed for block-style parsing
   struct yaml_context : context
   {
      // Indent stack for block-style parsing.
      // Empty stack == top level (equivalent to indent of -1).
      // back() gives the current block indent level.
      std::vector<int16_t> indent_stack = [] {
         std::vector<int16_t> v;
         v.reserve(max_recursive_depth_limit);
         return v;
      }();

      int32_t current_indent() const noexcept
      {
         return indent_stack.empty() ? int32_t(-1) : indent_stack.back();
      }

      bool push_indent(int32_t indent) noexcept
      {
         if (indent_stack.size() >= max_recursive_depth_limit) [[unlikely]] {
            error = error_code::exceeded_max_recursive_depth;
            return false;
         }
         indent_stack.push_back(static_cast<int16_t>(indent));
         return true;
      }

      void pop_indent() noexcept
      {
         if (!indent_stack.empty()) {
            indent_stack.pop_back();
         }
      }
   };

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
   // Maps escape char to its actual value
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

   // Table indicating which YAML escapes are simple (single-byte output)
   // Separate from unescape_table because \0 maps to '\0' which is falsy
   inline constexpr std::array<bool, 256> yaml_escape_is_simple = [] {
      std::array<bool, 256> t{};
      t['"'] = true;
      t['\\'] = true;
      t['/'] = true;
      t['a'] = true;
      t['b'] = true;
      t['t'] = true;
      t['n'] = true;
      t['v'] = true;
      t['f'] = true;
      t['r'] = true;
      t['e'] = true;
      t[' '] = true;
      t['0'] = true;
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

   // Table for characters that terminate a plain scalar in flow context
   // Terminators: space, tab, newline, carriage return, colon, comma, [ ] { } #
   inline constexpr std::array<bool, 256> plain_scalar_end_table = [] {
      std::array<bool, 256> t{};
      t[' '] = true;
      t['\t'] = true;
      t['\n'] = true;
      t['\r'] = true;
      t[':'] = true;
      t[','] = true;
      t['['] = true;
      t[']'] = true;
      t['{'] = true;
      t['}'] = true;
      t['#'] = true;
      return t;
   }();

   // Table for whitespace and line ending characters
   // Characters: space, tab, newline, carriage return
   inline constexpr std::array<bool, 256> whitespace_or_line_end_table = [] {
      std::array<bool, 256> t{};
      t[' '] = true;
      t['\t'] = true;
      t['\n'] = true;
      t['\r'] = true;
      return t;
   }();

   // Table for line ending or comment characters
   // Characters: newline, carriage return, #
   inline constexpr std::array<bool, 256> line_end_or_comment_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\r'] = true;
      t['#'] = true;
      return t;
   }();

   // Table for flow context ending characters (line end + flow terminators)
   // Characters: newline, carriage return, comma, ] }
   inline constexpr std::array<bool, 256> flow_context_end_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\r'] = true;
      t[','] = true;
      t[']'] = true;
      t['}'] = true;
      return t;
   }();

   // Table for characters that terminate block mapping key scanning
   // Characters: newline, carriage return, flow indicators { [ ] } ,
   inline constexpr std::array<bool, 256> block_mapping_end_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\r'] = true;
      t['{'] = true;
      t['['] = true;
      t[']'] = true;
      t['}'] = true;
      t[','] = true;
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

   // YAML core schema tags
   enum struct yaml_tag : uint8_t {
      none, // No tag present
      str, // !!str
      int_tag, // !!int
      float_tag, // !!float
      bool_tag, // !!bool
      null_tag, // !!null
      map, // !!map
      seq, // !!seq
      unknown // Unknown/custom tag
   };

   // Parse a YAML tag if present
   // Tags start with ! and can be:
   // - Verbatim: !<tag:yaml.org,2002:str>
   // - Shorthand: !!str (equivalent to !<tag:yaml.org,2002:str>)
   // - Named: !mytag
   // Returns the tag type and advances iterator past the tag
   template <class It, class End>
   GLZ_ALWAYS_INLINE yaml_tag parse_yaml_tag(It& it, End end) noexcept
   {
      if (it == end || *it != '!') {
         return yaml_tag::none;
      }

      auto start = it;
      ++it; // skip first !

      if (it == end) {
         it = start;
         return yaml_tag::none;
      }

      // Check for !! (shorthand tag)
      if (*it == '!') {
         ++it; // skip second !

         // Read tag name
         auto tag_start = it;
         while (it != end && !plain_scalar_end_table[static_cast<uint8_t>(*it)]) {
            ++it;
         }

         std::string_view tag_name(tag_start, static_cast<size_t>(it - tag_start));

         // Skip whitespace after tag
         while (it != end && (*it == ' ' || *it == '\t')) {
            ++it;
         }

         // Match known tags
         if (tag_name == "str") return yaml_tag::str;
         if (tag_name == "int") return yaml_tag::int_tag;
         if (tag_name == "float") return yaml_tag::float_tag;
         if (tag_name == "bool") return yaml_tag::bool_tag;
         if (tag_name == "null") return yaml_tag::null_tag;
         if (tag_name == "map") return yaml_tag::map;
         if (tag_name == "seq") return yaml_tag::seq;

         return yaml_tag::unknown;
      }

      // Check for verbatim tag !<...>
      if (*it == '<') {
         ++it;
         auto tag_start = it;
         while (it != end && *it != '>') {
            ++it;
         }
         if (it == end) {
            it = start;
            return yaml_tag::none;
         }

         std::string_view tag_uri(tag_start, static_cast<size_t>(it - tag_start));
         ++it; // skip >

         // Skip whitespace after tag
         while (it != end && (*it == ' ' || *it == '\t')) {
            ++it;
         }

         // Match known YAML 1.2 URIs
         if (tag_uri == "tag:yaml.org,2002:str") return yaml_tag::str;
         if (tag_uri == "tag:yaml.org,2002:int") return yaml_tag::int_tag;
         if (tag_uri == "tag:yaml.org,2002:float") return yaml_tag::float_tag;
         if (tag_uri == "tag:yaml.org,2002:bool") return yaml_tag::bool_tag;
         if (tag_uri == "tag:yaml.org,2002:null") return yaml_tag::null_tag;
         if (tag_uri == "tag:yaml.org,2002:map") return yaml_tag::map;
         if (tag_uri == "tag:yaml.org,2002:seq") return yaml_tag::seq;

         return yaml_tag::unknown;
      }

      // Named tag !name - skip it and read name
      while (it != end && !plain_scalar_end_table[static_cast<uint8_t>(*it)]) {
         ++it;
      }

      // Skip whitespace after tag
      while (it != end && (*it == ' ' || *it == '\t')) {
         ++it;
      }

      return yaml_tag::unknown;
   }

   // Check if a tag is valid for string types
   GLZ_ALWAYS_INLINE constexpr bool tag_valid_for_string(yaml_tag tag) noexcept
   {
      return tag == yaml_tag::none || tag == yaml_tag::str;
   }

   // Check if a tag is valid for integer types
   GLZ_ALWAYS_INLINE constexpr bool tag_valid_for_int(yaml_tag tag) noexcept
   {
      return tag == yaml_tag::none || tag == yaml_tag::int_tag;
   }

   // Check if a tag is valid for floating-point types
   GLZ_ALWAYS_INLINE constexpr bool tag_valid_for_float(yaml_tag tag) noexcept
   {
      return tag == yaml_tag::none || tag == yaml_tag::float_tag || tag == yaml_tag::int_tag;
   }

   // Check if a tag is valid for boolean types
   GLZ_ALWAYS_INLINE constexpr bool tag_valid_for_bool(yaml_tag tag) noexcept
   {
      return tag == yaml_tag::none || tag == yaml_tag::bool_tag;
   }

   // Check if a tag is valid for null/nullable types
   GLZ_ALWAYS_INLINE constexpr bool tag_valid_for_null(yaml_tag tag) noexcept
   {
      return tag == yaml_tag::none || tag == yaml_tag::null_tag;
   }

   // Check if a tag is valid for sequence types
   GLZ_ALWAYS_INLINE constexpr bool tag_valid_for_seq(yaml_tag tag) noexcept
   {
      return tag == yaml_tag::none || tag == yaml_tag::seq;
   }

   // Check if a tag is valid for mapping types
   GLZ_ALWAYS_INLINE constexpr bool tag_valid_for_map(yaml_tag tag) noexcept
   {
      return tag == yaml_tag::none || tag == yaml_tag::map;
   }

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

   // Skip all whitespace including newlines (spaces, tabs, \n, \r)
   template <class It, class End>
   GLZ_ALWAYS_INLINE void skip_ws_and_newlines(It&& it, End end) noexcept
   {
      while (it != end && (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r')) {
         if (*it == '\n' || *it == '\r') {
            skip_newline(it, end);
         }
         else {
            ++it;
         }
      }
   }

   // Skip all whitespace, newlines, and comments until reaching actual content
   // This is used at the start of parsing and between top-level elements
   template <class It, class End>
   GLZ_ALWAYS_INLINE void skip_ws_newlines_comments(It&& it, End end) noexcept
   {
      while (it != end) {
         if (*it == ' ' || *it == '\t') {
            ++it;
         }
         else if (*it == '\n' || *it == '\r') {
            skip_newline(it, end);
         }
         else if (*it == '#') {
            skip_comment(it, end);
         }
         else {
            break;
         }
      }
   }

   // Check if at newline or end
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool at_newline_or_end(It&& it, End end) noexcept
   {
      return it == end || *it == '\n' || *it == '\r';
   }

   // Skip YAML directives (%YAML, %TAG, etc.) and document start marker (---)
   // YAML directives are lines starting with % that appear before ---
   // Handles: %YAML 1.2\n---\ndata, %TAG...\n---\ndata, ---, ---\n, --- comment\n, etc.
   //
   // Per YAML 1.2.2 spec:
   // - It is an error to specify more than one %YAML directive for the same document
   // - Documents with %YAML major version > 1 should be rejected
   // - Unknown directives should be ignored (with warning, but we silently skip)
   template <class It, class End, class Ctx>
   GLZ_ALWAYS_INLINE void skip_document_start(It&& it, End end, Ctx& ctx) noexcept
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

      // Track if we've seen a %YAML directive (duplicates are an error per spec)
      bool seen_yaml_directive = false;

      // Process YAML directives (lines starting with %) until we hit --- or content
      // Directives must appear at the start of a line and before ---
      while (it != end && *it == '%') {
         ++it; // Skip the '%'

         // Parse directive name
         auto name_start = it;
         while (it != end && *it != ' ' && *it != '\t' && *it != '\n' && *it != '\r') {
            ++it;
         }
         std::string_view directive_name(name_start, static_cast<size_t>(it - name_start));

         // Check for %YAML directive
         if (directive_name == "YAML") {
            // Error on duplicate %YAML directive (per spec: "It is an error to specify
            // more than one YAML directive for the same document")
            if (seen_yaml_directive) {
               ctx.error = error_code::syntax_error;
               return;
            }
            seen_yaml_directive = true;

            // Skip whitespace before version
            while (it != end && (*it == ' ' || *it == '\t')) {
               ++it;
            }

            // Parse major version number
            if (it != end && *it >= '0' && *it <= '9') {
               int major_version = 0;
               while (it != end && *it >= '0' && *it <= '9') {
                  major_version = major_version * 10 + (*it - '0');
                  ++it;
               }

               // Check for major version > 1 (per spec: should be rejected)
               if (major_version > 1) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
         // %TAG and other directives are silently skipped (per spec: should be ignored)

         // Skip to end of directive line
         while (it != end && *it != '\n' && *it != '\r') {
            ++it;
         }
         // Skip the newline
         skip_newline(it, end);
         // Skip any blank lines or whitespace between directives
         while (it != end && (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r')) {
            if (*it == '\n' || *it == '\r') {
               skip_newline(it, end);
            }
            else {
               ++it;
            }
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

   // Overload without context for backwards compatibility (no error checking)
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

      // Skip YAML directives (lines starting with %) until we hit --- or content
      while (it != end && *it == '%') {
         // Skip to end of directive line
         while (it != end && *it != '\n' && *it != '\r') {
            ++it;
         }
         skip_newline(it, end);
         // Skip any blank lines
         while (it != end && (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r')) {
            if (*it == '\n' || *it == '\r') {
               skip_newline(it, end);
            }
            else {
               ++it;
            }
         }
      }

      // Check for ---
      if (end - it >= 3 && it[0] == '-' && it[1] == '-' && it[2] == '-') {
         auto after = it + 3;
         if (after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r' || *after == '#') {
            it = after;
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
         if (it != end && !line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
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
