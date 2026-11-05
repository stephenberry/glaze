#pragma once

#include <cctype>

#include "glaze/core/common.hpp"

namespace glz::toml
{
   // TOML v1.0.0 excludes raw control characters from strings, keys and comments. A
   // single-line basic or literal string and a comment admit tab and nothing else in
   // that range; the multi-line forms additionally admit line feed, and carriage
   // return as the start of a CRLF line ending. DEL (0x7F) is excluded from all of
   // them. Anything else in the C0 range has to arrive as an escape (\u00XX) or not
   // at all.
   //
   // A carriage return is only legal when a line feed follows it, which a per-byte
   // test cannot see, so it is not in the multi-line forbidden set. The multi-line
   // scans stop on it instead (see the dispatch tables) and check the next byte.
   inline constexpr bool is_toml_forbidden_control(char c) noexcept
   {
      const auto u = uint8_t(c);
      return (u < 0x20 && u != 0x09) || u == 0x7f;
   }

   inline constexpr bool is_toml_forbidden_control_multiline(char c) noexcept
   {
      const auto u = uint8_t(c);
      return (u < 0x20 && u != 0x09 && u != 0x0a && u != 0x0d) || u == 0x7f;
   }

   // Control characters the writer must escape to keep its output readable: everything
   // TOML forbids raw, minus the bytes that already have a two-character escape.
   inline constexpr bool is_toml_control(char c) noexcept
   {
      const auto u = uint8_t(c);
      return u < 0x20 || u == 0x7f;
   }

   // Each table folds a scan's terminators together with the control bytes that scan
   // must reject, so the reader pays one lookup per byte instead of a compare chain
   // plus a separate validity test.
   inline constexpr std::array<bool, 256> forbidden_control_table = [] {
      std::array<bool, 256> t{};
      for (size_t i = 0; i < 256; ++i) {
         t[i] = is_toml_forbidden_control(char(i));
      }
      return t;
   }();

   inline constexpr std::array<bool, 256> forbidden_control_multiline_table = [] {
      std::array<bool, 256> t{};
      for (size_t i = 0; i < 256; ++i) {
         t[i] = is_toml_forbidden_control_multiline(char(i));
      }
      return t;
   }();

   // A comment runs to the end of the line, and every control byte it must reject is
   // already forbidden, so its terminators (line feed, carriage return) are in the set.
   inline constexpr auto& comment_end_or_control_table = forbidden_control_table;

   inline constexpr std::array<bool, 256> basic_string_dispatch_table = [] {
      auto t = forbidden_control_table;
      t[uint8_t('"')] = true;
      t[uint8_t('\\')] = true;
      return t;
   }();

   inline constexpr std::array<bool, 256> literal_string_dispatch_table = [] {
      auto t = forbidden_control_table;
      t[uint8_t('\'')] = true;
      return t;
   }();

   inline constexpr std::array<bool, 256> multiline_basic_dispatch_table = [] {
      auto t = forbidden_control_multiline_table;
      t[uint8_t('"')] = true;
      t[uint8_t('\\')] = true;
      t[uint8_t('\r')] = true;
      return t;
   }();

   inline constexpr std::array<bool, 256> multiline_literal_dispatch_table = [] {
      auto t = forbidden_control_multiline_table;
      t[uint8_t('\'')] = true;
      t[uint8_t('\r')] = true;
      return t;
   }();

   // In a multi-line string a carriage return must open a CRLF line ending. `it` points
   // at a byte inside the string, so a carriage return there is bare when it is the
   // last byte or the next byte is not a line feed.
   template <class It, class End>
   inline constexpr bool is_bare_carriage_return(It it, End end) noexcept
   {
      return *it == '\r' && (it + 1 == end || *(it + 1) != '\n');
   }
} // namespace glz::toml

namespace glz
{
   // Skip whitespace and comments
   template <class It, class End>
   inline void skip_ws_and_comments(It&& it, End end) noexcept
   {
      while (it != end) {
         if (*it == ' ' || *it == '\t') {
            ++it;
         }
         else if (*it == '#') {
            // Stop on the bytes TOML forbids in a comment as well as on the line end.
            // These helpers take no ctx, so leaving the byte in place is what reports
            // it: the caller sees an unexpected character and errors.
            while (it != end && !toml::comment_end_or_control_table[uint8_t(*it)]) {
               ++it;
            }
         }
         else {
            break;
         }
      }
   }

   // Skip whitespace, newlines and comments. A newline is LF or CRLF; a carriage return
   // without a line feed after it is left in place for the caller to reject, the same
   // way a forbidden byte in a comment is.
   template <class It, class End>
   inline void skip_ws_newlines_and_comments(It&& it, End end) noexcept
   {
      while (it != end) {
         if (*it == ' ' || *it == '\t' || *it == '\n' || (*it == '\r' && !toml::is_bare_carriage_return(it, end))) {
            ++it;
         }
         else if (*it == '#') {
            // Stop on the bytes TOML forbids in a comment as well as on the line end.
            // These helpers take no ctx, so leaving the byte in place is what reports
            // it: the caller sees an unexpected character and errors.
            while (it != end && !toml::comment_end_or_control_table[uint8_t(*it)]) {
               ++it;
            }
         }
         else {
            break;
         }
      }
   }

   // Skip to next line. A line ends in LF or CRLF; a carriage return without a line feed
   // after it is a syntax error. Returns false at the end of input or on that error.
   template <class Ctx, class It, class End>
   inline bool skip_to_next_line(Ctx& ctx, It&& it, End end) noexcept
   {
      while (it != end && *it != '\n' && *it != '\r') {
         ++it;
      }

      if (it == end) {
         return false;
      }

      if (toml::is_bare_carriage_return(it, end)) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return false;
      }

      it += (*it == '\r') ? 2 : 1;
      return true;
   }
} // namespace glz
