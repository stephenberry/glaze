// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cctype>
#include <charconv>

#include "glaze/core/common.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/json/generic.hpp"
#include "glaze/util/glaze_fast_float.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/variant.hpp"
#include "glaze/yaml/common.hpp"
#include "glaze/yaml/opts.hpp"
#include "glaze/yaml/skip.hpp"

namespace glz
{
   template <>
   struct parse<YAML>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         if constexpr (requires { ctx.stream_begin; }) {
            if (!ctx.stream_begin && it != end) {
               ctx.stream_begin = &*it;
            }
         }
         // Skip YAML directives and document start marker (---) if present
         yaml::skip_document_start(it, end, ctx);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         using V = std::remove_cvref_t<T>;
         if (it == end) {
            // An empty document is a valid YAML null document.
            if constexpr (requires { value = nullptr; }) {
               value = nullptr;
               return;
            }
            else if constexpr (nullable_like<V>) {
               value = {};
               return;
            }
            else {
               ctx.error = error_code::unexpected_end;
               return;
            }
         }

         // A bare document boundary marker at root denotes an empty document.
         // Examples: "---\n---\n", "# comment\n...\n"
         if (yaml::at_document_start(it, end) || yaml::at_document_end(it, end)) {
            if constexpr (requires { value = nullptr; }) {
               value = nullptr;
               return;
            }
            else if constexpr (nullable_like<V>) {
               value = {};
               return;
            }
            else {
               ctx.error = error_code::unexpected_end;
               return;
            }
         }

         from<YAML, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), end);

         // A directive line (%YAML/%TAG/...) is only valid in the document prefix.
         // If parsing stopped before the end and we encounter a directive in the
         // remaining tail, treat it as malformed stream structure.
         if constexpr (!check_partial_read(Opts)) {
            if (!bool(ctx.error)) {
               auto is_document_start = [&](auto pos) {
                  if (end - pos >= 3 && pos[0] == '-' && pos[1] == '-' && pos[2] == '-') {
                     auto after = pos + 3;
                     return after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r' ||
                            *after == '#';
                  }
                  return false;
               };

               auto tail_scan = it;
               bool seen_document_end_marker = false;
               while (tail_scan != end) {
                  auto line = tail_scan;
                  while (line != end && (*line == ' ' || *line == '\t')) ++line;
                  if (line != tail_scan && line != end && *line != '\n' && *line != '\r' && *line != '#') {
                     // Indented tail usually belongs to continuation content.
                     // But a plain "key: value" pattern here indicates malformed
                     // trailing mapping content after a completed root node.
                     const char first = *line;
                     const bool explicit_or_structural_start =
                        (first == ':' || first == '?' || first == '!' || first == '&' || first == '*' ||
                         first == '[' || first == '{' || first == '"' || first == '\'' || first == '-');
                     if (!explicit_or_structural_start) {
                        auto scan = line;
                        while (scan != end && *scan != '\n' && *scan != '\r') {
                           if (*scan == ':') {
                              auto after = scan + 1;
                              if (after == end || *after == ' ' || *after == '\t' || *after == '\n' ||
                                  *after == '\r') {
                                 ctx.error = error_code::syntax_error;
                                 return;
                              }
                           }
                           ++scan;
                        }
                     }
                     return;
                  }
                  if (line == end) {
                     return;
                  }
                  if (*line == ':' || *line == '?') {
                     return; // Explicit key/value continuation.
                  }
                  if (*line == '\n' || *line == '\r') {
                     tail_scan = line;
                     yaml::skip_newline(tail_scan, end);
                     continue;
                  }
                  if (*line == '#') {
                     while (tail_scan != end && *tail_scan != '\n' && *tail_scan != '\r') ++tail_scan;
                     yaml::skip_newline(tail_scan, end);
                     continue;
                  }
                  if (yaml::at_document_end(line, end)) {
                     seen_document_end_marker = true;
                     while (tail_scan != end && *tail_scan != '\n' && *tail_scan != '\r') ++tail_scan;
                     yaml::skip_newline(tail_scan, end);
                     continue;
                  }
                  if (is_document_start(line)) {
                     return; // Additional documents are allowed in the stream tail.
                  }
                  if (line != end && *line == '%') {
                     // YAML directives are only valid in document prefixes. A directive
                     // encountered mid-document (without a prior ...) is malformed.
                     if (!seen_document_end_marker) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     return;
                  }
                  if (seen_document_end_marker) {
                     return; // Implicit next document after explicit document end marker.
                  }
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
      }
   };

   // glaze_value_t - unwrap custom value types
   template <class T>
      requires(glaze_value_t<T> && !custom_read<T>)
   struct from<YAML, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         from<YAML, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                          std::forward<Ctx>(ctx), std::forward<It0>(it), end);
      }
   };

   namespace yaml
   {
      // Handle YAML alias (*name) by replaying the stored anchor span.
      // Returns true if alias was handled (caller should return).
      // Returns false if current char is not '*' (caller continues normally).
      template <auto Opts, class T, class Ctx, class It, class End>
      bool handle_alias(T&& value, Ctx& ctx, It& it, [[maybe_unused]] End end) noexcept
      {
         if (it == end || *it != '*') return false;

         ++it; // skip '*'
         auto name = parse_anchor_name(it, end);
         if (name.empty()) {
            ctx.error = error_code::syntax_error;
            return true;
         }

         auto anchor_it = ctx.anchors.find(name);
         if (anchor_it == ctx.anchors.end()) {
            ctx.error = error_code::syntax_error; // undefined alias
            return true;
         }

         auto& span = anchor_it->second;

         // Empty anchor span (anchor on null/empty node) — leave value as default
         if (span.begin == span.end) {
            return true;
         }

         auto replay_it = span.begin;
         auto replay_end = span.end;

         // Save indent context and set up for replay
         auto saved_indent_stack = std::move(ctx.indent_stack);
         ctx.indent_stack.clear();
         if (span.base_indent > 0) {
            ctx.push_indent(span.base_indent - 1);
         }

         using V = std::remove_cvref_t<T>;
         from<YAML, V>::template op<Opts>(std::forward<T>(value), ctx, replay_it, replay_end);

         // Restore indent context
         ctx.indent_stack = std::move(saved_indent_stack);
         return true;
      }

      template <class It, class End>
      GLZ_ALWAYS_INLINE bool alias_token_is_mapping_key(It it, End end) noexcept
      {
         if (it == end || *it != '*') return false;
         ++it; // skip '*'
         auto name = parse_anchor_name(it, end);
         if (name.empty()) return false;
         skip_inline_ws(it, end);
         return (it != end && *it == ':') &&
                ((it + 1) == end || whitespace_or_line_end_table[static_cast<uint8_t>(*(it + 1))]);
      }

      struct node_property_state
      {
         bool has_anchor = false;
         std::string anchor_name{};
         const char* anchor_start{};
         int32_t anchor_indent = 0;
      };

      struct node_property_policy
      {
         bool allow_alias = true;
         bool alias_can_be_mapping_key = false;
         bool allow_empty_after_anchor = false;
         bool disallow_anchor_on_alias = false;
      };

      // Parse alias/anchor node properties shared across YAML value parsers.
      // Returns true when caller should stop (alias consumed, tolerated empty anchor, or syntax/error).
      template <auto Opts, node_property_policy Policy = node_property_policy{}, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE bool parse_node_properties(T&& value, Ctx& ctx, It& it, End end, node_property_state& state) noexcept
      {
         state.has_anchor = false;
         state.anchor_name.clear();
         state.anchor_start = nullptr;
         state.anchor_indent = ctx.current_indent();

         if constexpr (Policy.allow_alias) {
            if constexpr (Policy.alias_can_be_mapping_key) {
               if (!alias_token_is_mapping_key(it, end)) {
                  if (handle_alias<Opts>(std::forward<T>(value), ctx, it, end)) return true;
               }
            }
            else {
               if (handle_alias<Opts>(std::forward<T>(value), ctx, it, end)) return true;
            }
         }

         if (it != end && *it == '&') {
            ++it;
            auto name = parse_anchor_name(it, end);
            if (name.empty()) {
               ctx.error = error_code::syntax_error;
               return true;
            }
            skip_inline_ws(it, end);
            if (it == end) {
               if constexpr (!Policy.allow_empty_after_anchor) {
                  ctx.error = error_code::unexpected_end;
               }
               return true;
            }
            if constexpr (Policy.disallow_anchor_on_alias) {
               if (*it == '*') {
                  ctx.error = error_code::syntax_error;
                  return true;
               }
            }
            state.has_anchor = true;
            state.anchor_name = std::string(name);
            state.anchor_start = &*it;
         }

         return false;
      }

      template <class Ctx, class It>
      GLZ_ALWAYS_INLINE void finalize_node_anchor(node_property_state& state, Ctx& ctx, It it) noexcept
      {
         if (state.has_anchor && !bool(ctx.error)) {
            ctx.anchors[std::move(state.anchor_name)] = {state.anchor_start, &*it, state.anchor_indent};
         }
      }

      // SWAR-optimized double-quoted string parsing
      // Uses 8-byte chunks for fast scanning and copying
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_double_quoted_string(std::string& value, Ctx& ctx, It& it, End end)
      {
         static constexpr size_t string_padding_bytes = 8;
         auto skip_folded_line_indent = [&](const char*& src, const char* src_end) -> bool {
            bool saw_space = false;
            int indent_count = 0;
            while (src < src_end && (*src == ' ' || *src == '\t')) {
               // In nested block contexts, a tab at indentation column 0 is invalid.
               if (*src == '\t' && !saw_space && ctx.current_indent() >= 0) {
                  ctx.error = error_code::syntax_error;
                  return false;
               }
               if (*src == ' ') saw_space = true;
               ++indent_count;
               ++src;
            }
            if (ctx.current_indent() >= 0 && src < src_end && *src != '\n' && *src != '\r' &&
                indent_count < ctx.current_indent()) {
               ctx.error = error_code::syntax_error;
               return false;
            }
            return true;
         };

         if (it == end || *it != '"') [[unlikely]] {
            ctx.error = error_code::expected_quote;
            return;
         }

         ++it; // skip opening quote
         auto start = it;

         // Pass 1: Find closing quote byte-by-byte (need to handle newlines and escapes)
         while (it != end && *it != '"') {
            if (*it == '\\') {
               ++it;
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }
            ++it;
         }

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Allocate buffer with room for potential expansion and SWAR padding
         // Some YAML escapes expand: \L, \P -> 3 bytes UTF-8
         const auto input_len = static_cast<size_t>(it - start);
         value.resize(input_len + (input_len / 2) + string_padding_bytes);
         auto* dst = value.data();
         auto* const dst_start = dst;

         // Pass 2: Copy and process escapes and line folding
         auto src = start;
         const auto* const src_end = &*it;

         while (src < src_end) {
            // Check for newline - needs line folding
            if (*src == '\n' || *src == '\r') {
               // Trim trailing whitespace from output before processing newline
               while (dst > dst_start && (*(dst - 1) == ' ' || *(dst - 1) == '\t')) {
                  --dst;
               }

               // Skip the newline
               if (*src == '\r' && (src + 1) < src_end && *(src + 1) == '\n') {
                  src += 2; // CRLF
               }
               else {
                  ++src;
               }

               // Skip leading indentation on the next line.
               if (!skip_folded_line_indent(src, src_end)) return;

               // Check if this is a blank line (another newline follows)
               if (src < src_end && (*src == '\n' || *src == '\r')) {
                  // Blank line(s) - output newlines for each blank line
                  while (src < src_end && (*src == '\n' || *src == '\r')) {
                     *dst++ = '\n';
                     // Skip the newline
                     if (*src == '\r' && (src + 1) < src_end && *(src + 1) == '\n') {
                        src += 2; // CRLF
                     }
                     else {
                        ++src;
                     }
                     if (!skip_folded_line_indent(src, src_end)) return;
                  }
                  // Don't add space - we're now at content after blank line(s)
               }
               else {
                  // Single newline - fold to space
                  *dst++ = ' ';
               }
               continue;
            }

            if (*src == '\\') {
               ++src;
               if (src >= src_end) [[unlikely]] {
                  // Shouldn't happen - we validated in pass 1
                  ctx.error = error_code::syntax_error;
                  return;
               }

               const unsigned char esc = static_cast<unsigned char>(*src);

               // Check for escaped newline (line continuation - no space)
               if (esc == '\n' || esc == '\r') {
                  // Skip the newline
                  if (esc == '\r' && (src + 1) < src_end && *(src + 1) == '\n') {
                     src += 2; // CRLF
                  }
                  else {
                     ++src;
                  }

                  if (!skip_folded_line_indent(src, src_end)) return;
                  // No output - this is line continuation without space
                  continue;
               }

               // Check simple escape table first
               if (yaml_escape_is_simple[esc]) {
                  *dst++ = yaml_unescape_table[esc];
                  ++src;
               }
               // Handle escapes requiring special processing
               else if (yaml_escape_needs_special[esc]) {
                  ++src; // skip escape char
                  switch (esc) {
                  case 'x': {
                     // \xXX - 2 hex digits
                     if (static_cast<size_t>(src_end - src) < 2) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     const uint32_t hi = digit_hex_table[static_cast<unsigned char>(src[0])];
                     const uint32_t lo = digit_hex_table[static_cast<unsigned char>(src[1])];
                     if ((hi | lo) & 0xF0) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     *dst++ = static_cast<char>((hi << 4) | lo);
                     src += 2;
                     break;
                  }
                  case 'u': {
                     // \uXXXX - 4 hex digits
                     if (static_cast<size_t>(src_end - src) < 4) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     const uint32_t codepoint = hex_to_u32(src);
                     if (codepoint == 0xFFFFFFFFu) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     src += 4;
                     dst += code_point_to_utf8(codepoint, dst);
                     break;
                  }
                  case 'U': {
                     // \UXXXXXXXX - 8 hex digits
                     if (static_cast<size_t>(src_end - src) < 8) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     const uint32_t hi = hex_to_u32(src);
                     const uint32_t lo = hex_to_u32(src + 4);
                     if ((hi | lo) == 0xFFFFFFFFu) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     const uint32_t codepoint = (hi << 16) | lo;
                     src += 8;
                     if (codepoint <= 0x10FFFF) {
                        dst += code_point_to_utf8(codepoint, dst);
                     }
                     break;
                  }
                  case 'N': {
                     // Next line U+0085
                     dst += code_point_to_utf8(0x0085, dst);
                     break;
                  }
                  case '_': {
                     // Non-breaking space U+00A0
                     dst += code_point_to_utf8(0x00A0, dst);
                     break;
                  }
                  case 'L': {
                     // Line separator U+2028
                     dst += code_point_to_utf8(0x2028, dst);
                     break;
                  }
                  case 'P': {
                     // Paragraph separator U+2029
                     dst += code_point_to_utf8(0x2029, dst);
                     break;
                  }
                  default:
                     break;
                  }
               }
               else {
                  // Preserve unknown escapes for compatibility, but reject a subset that
                  // YAML test-suite marks as malformed in double-quoted scalars.
                  if (esc == '.' || esc == '\'') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  *dst++ = '\\';
                  *dst++ = static_cast<char>(esc);
                  ++src;
               }
            }
            else {
               *dst++ = *src++;
            }
         }

         // Resize to actual length
         value.resize(static_cast<size_t>(dst - dst_start));
         ++it; // skip closing quote
      }

      // Single-quoted string parsing with line folding
      // Only escape is '' -> ' (doubled single quote)
      // Line breaks are folded: single newline -> space, blank line -> newline
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_single_quoted_string(std::string& value, Ctx& ctx, It& it, End end)
      {
         static constexpr size_t string_padding_bytes = 8;
         auto skip_folded_line_indent = [&](const char*& src, const char* src_end) -> bool {
            bool saw_space = false;
            int indent_count = 0;
            while (src < src_end && (*src == ' ' || *src == '\t')) {
               // In nested block contexts, a tab at indentation column 0 is invalid.
               if (*src == '\t' && !saw_space && ctx.current_indent() >= 0) {
                  ctx.error = error_code::syntax_error;
                  return false;
               }
               if (*src == ' ') saw_space = true;
               ++indent_count;
               ++src;
            }
            if (ctx.current_indent() >= 0 && src < src_end && *src != '\n' && *src != '\r' &&
                indent_count < ctx.current_indent()) {
               ctx.error = error_code::syntax_error;
               return false;
            }
            return true;
         };

         if (it == end || *it != '\'') [[unlikely]] {
            ctx.error = error_code::expected_quote;
            return;
         }

         ++it; // skip opening quote
         auto start = it;

         // Pass 1: Find closing quote (handling '' escapes)
         while (it != end) {
            if (*it == '\'') {
               if ((it + 1) != end && *(it + 1) == '\'') {
                  it += 2; // Skip escaped quote
               }
               else {
                  break; // Found closing quote
               }
            }
            else {
               ++it;
            }
         }

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const auto input_len = static_cast<size_t>(it - start);
         value.resize(input_len + string_padding_bytes);
         auto* dst = value.data();
         auto* const dst_start = dst;
         auto src = start;
         const auto* const src_end = &*it;

         // Pass 2: Process content with line folding and '' escapes
         while (src < src_end) {
            // Check for newline - needs line folding
            if (*src == '\n' || *src == '\r') {
               // Trim trailing whitespace from output before processing newline
               while (dst > dst_start && (*(dst - 1) == ' ' || *(dst - 1) == '\t')) {
                  --dst;
               }

               // Skip the newline
               if (*src == '\r' && (src + 1) < src_end && *(src + 1) == '\n') {
                  src += 2; // CRLF
               }
               else {
                  ++src;
               }

               // Skip leading indentation on the next line.
               if (!skip_folded_line_indent(src, src_end)) return;

               // Check if this is a blank line (another newline follows)
               if (src < src_end && (*src == '\n' || *src == '\r')) {
                  // Blank line(s) - output newlines for each blank line
                  while (src < src_end && (*src == '\n' || *src == '\r')) {
                     *dst++ = '\n';
                     // Skip the newline
                     if (*src == '\r' && (src + 1) < src_end && *(src + 1) == '\n') {
                        src += 2; // CRLF
                     }
                     else {
                        ++src;
                     }
                     if (!skip_folded_line_indent(src, src_end)) return;
                  }
                  // Don't add space - we're now at content after blank line(s)
               }
               else {
                  // Single newline - fold to space
                  *dst++ = ' ';
               }
               continue;
            }

            if (*src == '\'') {
               // Must be '' (escaped quote) - we validated this in pass 1
               *dst++ = '\'';
               src += 2;
            }
            else {
               *dst++ = *src++;
            }
         }

         value.resize(static_cast<size_t>(dst - dst_start));
         ++it; // skip closing quote
      }

      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void skip_flow_ws_and_newlines(Ctx& ctx, It& it, End end, bool* saw_line_break = nullptr)
      {
         bool at_line_start = false;
         bool saw_separation_ws = false;
         bool line_has_indent = false;
         while (it != end) {
            if (*it == ' ') {
               ++it;
               saw_separation_ws = true;
               if (at_line_start) line_has_indent = true;
               continue;
            }
            if (*it == '\t') {
               if (at_line_start && !line_has_indent) {
                  auto probe = it;
                  while (probe != end && (*probe == ' ' || *probe == '\t')) ++probe;
                  if (probe != end && *probe != '\n' && *probe != '\r' && *probe != '#') {
                     const char c = *probe;
                     const bool allowed_flow_delim = (c == ']' || c == '}' || c == '[' || c == '{' || c == ',');
                     if (!allowed_flow_delim) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }
               }
               ++it;
               saw_separation_ws = true;
               if (at_line_start) line_has_indent = true;
               continue;
            }
            if (*it == '\n' || *it == '\r') {
               skip_newline(it, end);
               if (saw_line_break) *saw_line_break = true;
               at_line_start = true;
               saw_separation_ws = true;
               line_has_indent = false;
               continue;
            }
            if (*it == '#') {
               // In flow style, comments require separation from the previous token.
               // Allow comment-only lines and comments after inline whitespace, but
               // reject adjacency forms like ",#comment" (CVW2).
               bool separated_by_prev_ws = false;
               if constexpr (std::is_pointer_v<std::decay_t<It>>) {
                  if (ctx.stream_begin && it > ctx.stream_begin) {
                     const char prev = *(it - 1);
                     separated_by_prev_ws = (prev == ' ' || prev == '\t');
                  }
               }
               if (!(at_line_start || saw_separation_ws || separated_by_prev_ws)) {
                  break;
               }
               skip_comment(it, end);
               if (it != end && (*it == '\n' || *it == '\r')) {
                  skip_newline(it, end);
                  if (saw_line_break) *saw_line_break = true;
                  at_line_start = true;
                  saw_separation_ws = true;
                  continue;
               }
               continue;
            }

            // In block context, multiline flow nodes require indentation on content
            // continuation lines (except structural tokens).
            if (at_line_start && ctx.current_indent() >= 0 && !line_has_indent) {
               const char c = *it;
               const bool structural = (c == ']' || c == '}' || c == ',');
               if (!structural) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            break;
         }
      }

      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void validate_flow_node_adjacent_tail(Ctx& ctx, It& it, End end)
      {
         if (it == end) return;
         const char c = *it;
         // After a flow collection closes, the next character must be a structural
         // separator or whitespace/comment. Adjacent plain content is malformed.
         if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#' || c == ',' || c == ']' || c == '}' || c == ':') {
            return;
         }
         ctx.error = error_code::syntax_error;
      }

      // At root level, a closed flow collection must be followed only by:
      // inline spaces, optional inline comment, newline-separated comments/blank lines,
      // or stream separators (--- / ...). This catches malformed trailing content such as:
      // "[a] ]", "[a]#comment" (without separation), or "[a]\ntrailing".
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void validate_root_flow_tail_after_close(Ctx& ctx, It& it, End end)
      {
         auto is_document_start = [&](auto pos) {
            if (end - pos >= 3 && pos[0] == '-' && pos[1] == '-' && pos[2] == '-') {
               auto after = pos + 3;
               return after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r' ||
                      *after == '#';
            }
            return false;
         };

         bool at_line_start = false;
         auto tail = it;
         while (tail != end) {
            auto line = tail;
            skip_inline_ws(line, end);

            if (line == end) {
               return;
            }

            if (*line == '\n' || *line == '\r') {
               tail = line;
               skip_newline(tail, end);
               at_line_start = true;
               continue;
            }

            if (*line == '#') {
               if (!at_line_start && line == it) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               tail = line;
               skip_comment(tail, end);
               if (tail != end && (*tail == '\n' || *tail == '\r')) {
                  skip_newline(tail, end);
                  at_line_start = true;
                  continue;
               }
               return;
            }

            if (at_line_start && (at_document_end(line, end) || is_document_start(line))) {
               return;
            }

            ctx.error = error_code::syntax_error;
            return;
         }
      }

      // Parse a plain scalar (unquoted)
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_plain_scalar(std::string& value, Ctx& ctx, It& it, End end, bool in_flow)
      {
         value.clear();

         while (it != end) {
            const char c = *it;

            // End conditions
            if (c == '\n' || c == '\r') {
               if (in_flow) {
                  auto continuation = it;
                  skip_newline(continuation, end);
                  bool continuation_is_comment = false;

                  while (continuation != end) {
                     while (continuation != end && (*continuation == ' ' || *continuation == '\t')) {
                        ++continuation;
                     }
                     if (continuation != end && *continuation == '#') {
                        // Comment lines terminate plain flow scalars; the caller
                        // will consume comments/separators outside this scalar.
                        continuation_is_comment = true;
                        break;
                     }
                     break;
                  }

                  if (continuation_is_comment || continuation == end || *continuation == ',' || *continuation == ']' ||
                      *continuation == '}' || *continuation == ':') {
                     break;
                  }

                  while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                     value.pop_back();
                  }
                  if (!value.empty()) {
                     value.push_back(' ');
                  }
                  it = continuation;
                  continue;
               }
               break;
            }

            // Per YAML spec: # only starts a comment when preceded by whitespace
            // "foo#bar" is a valid plain scalar, but "foo #bar" has a comment
            if (c == '#') {
               // Check if preceded by whitespace (comment indicator)
               if (value.empty() || value.back() == ' ' || value.back() == '\t') {
                  break; // This is a comment
               }
               // Otherwise # is part of the scalar value
            }

            // Flow indicators end plain scalars in flow context
            if (in_flow && (c == ',' || c == ']' || c == '}')) break;

            // Colon followed by space/newline ends plain scalar
            if (c == ':') {
               if ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n' || *(it + 1) == '\r') {
                  break;
               }
               if (in_flow && (*(it + 1) == ',' || *(it + 1) == ']' || *(it + 1) == '}')) {
                  break;
               }
            }

            value.push_back(c);
            ++it;
         }

         // Trim trailing whitespace
         while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.pop_back();
         }

         if (in_flow && (value == "---" || value == "...")) {
            ctx.error = error_code::syntax_error;
         }
      }

      // Parse a multiline plain scalar with folding (for block context)
      // This handles plain scalars that span multiple lines, where continuation
      // lines are indented more than the base indent level.
      // Per YAML spec:
      // - Continuation lines must be more indented than the base indent
      // - A single newline between lines becomes a single space
      // - Blank lines are preserved as literal newlines
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_plain_scalar_multiline(std::string& value, Ctx& ctx, It& it, End end,
                                                          int32_t base_indent)
      {
         value.clear();

         while (it != end) {
            const char c = *it;

            // Check for newline - potential continuation
            if (c == '\n' || c == '\r') {
               // Trim trailing whitespace from current line
               while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                  value.pop_back();
               }

               // Count consecutive blank lines
               int blank_lines = 0;
               auto lookahead = it;

               while (lookahead != end) {
                  // Skip the newline
                  if (*lookahead == '\r' && (lookahead + 1) != end && *(lookahead + 1) == '\n') {
                     ++lookahead;
                  }
                  if (lookahead != end && (*lookahead == '\n' || *lookahead == '\r')) {
                     ++lookahead;
                  }
                  else {
                     break;
                  }

                  // Check if this is a blank line or comment-only line
                  int32_t line_indent = 0;

                  while (lookahead != end && *lookahead == ' ') {
                     ++line_indent;
                     ++lookahead;
                  }

                  if (lookahead == end || *lookahead == '\n' || *lookahead == '\r') {
                     // Blank line
                     ++blank_lines;
                     continue;
                  }

                  if (*lookahead == '#') {
                     // Comments end plain scalars per YAML spec
                     return;
                  }

                  // Found content - check indentation
                  // Continuation lines must not dedent below the parent block.
                  if (line_indent < base_indent) {
                     // Dedented - end of scalar
                     return;
                  }

                  // Document boundary markers at column 0 start/stop documents and
                  // must terminate a top-level plain scalar continuation.
                  if (line_indent == 0 && (at_document_start(lookahead, end) || at_document_end(lookahead, end))) {
                     return;
                  }

                  // Check for structural indicators that end the scalar
                  // Sequence indicator (- followed by space/newline/end)
                  if (*lookahead == '-') {
                     auto after_dash = lookahead + 1;
                     if (after_dash == end || *after_dash == ' ' || *after_dash == '\t' || *after_dash == '\n' ||
                         *after_dash == '\r') {
                        // This is a new sequence item, not a continuation
                        return;
                     }
                  }

                  if (ctx.explicit_mapping_key_context) {
                     // In explicit mapping keys ("? key"), a following explicit
                     // key/value indicator starts a new mapping entry.
                     if (*lookahead == '?' || *lookahead == ':') {
                        auto after = lookahead + 1;
                        if (after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') {
                           return;
                        }
                     }

                     // Node-property indicators at the start of a continuation
                     // line begin a new key node in explicit-key context.
                     if (*lookahead == '&' || *lookahead == '*' || *lookahead == '!') {
                        return;
                     }
                  }

                  // In a "- item" value context, an anchor/tag/alias at the start
                  // of a continuation candidate starts a new node, not scalar content.
                  if (ctx.sequence_item_value_context && (*lookahead == '&' || *lookahead == '*' || *lookahead == '!')) {
                     return;
                  }

                  // Check for mapping key indicator (content followed by : and space)
                  // This is a heuristic - we look for ": " pattern which indicates a mapping key
                  {
                     auto scan = lookahead;
                     while (scan != end && *scan != '\n' && *scan != '\r') {
                        if (*scan == ':') {
                           auto after_colon = scan + 1;
                           if (after_colon == end || *after_colon == ' ' || *after_colon == '\t' ||
                               *after_colon == '\n' || *after_colon == '\r') {
                              // This looks like a mapping key - end the scalar
                              return;
                           }
                        }
                        ++scan;
                     }
                  }

                  // This is a continuation line
                  // Add appropriate line breaks for blank lines
                  for (int i = 0; i < blank_lines; ++i) {
                     value.push_back('\n');
                  }

                  // Add a space to fold the newline (unless we added literal newlines for blank lines)
                  if (blank_lines == 0 && !value.empty()) {
                     value.push_back(' ');
                  }

                  // Move iterator to the content position
                  it = lookahead;
                  break;
               }

               // If we exhausted the lookahead, we're done
               if (lookahead == end) {
                  return;
               }

               continue; // Process the character at the new position
            }

            // Per YAML spec: # only starts a comment when preceded by whitespace
            if (c == '#') {
               if (value.empty() || value.back() == ' ' || value.back() == '\t') {
                  break; // This is a comment
               }
            }

            // Colon followed by space/newline ends plain scalar (potential nested mapping)
            if (c == ':') {
               if ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n' || *(it + 1) == '\r') {
                  break;
               }
            }

            value.push_back(c);
            ++it;
         }

         // Trim trailing whitespace
         while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.pop_back();
         }
      }

      // Parse a block scalar (| or >)
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_block_scalar(std::string& value, Ctx& ctx, It& it, End end, int32_t base_indent)
      {
         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const char indicator = *it;
         ++it;
         const auto header_start = it;

         // Chomping indicator: - (strip), + (keep), or none (clip)
         char chomping = ' '; // default: clip
         int explicit_indent = 0;
         bool seen_chomping = false;
         bool seen_indent = false;

         while (it != end && *it != '\n' && *it != '\r') {
            if (*it == '-' || *it == '+') {
               if (seen_chomping) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               seen_chomping = true;
               chomping = *it;
            }
            else if (*it >= '1' && *it <= '9') {
               if (seen_indent) {
                  // YAML indentation indicator is a single digit [1-9].
                  ctx.error = error_code::syntax_error;
                  return;
               }
               seen_indent = true;
               explicit_indent = *it - '0';
            }
            else if (*it == '0') {
               // Indentation indicator 0 is invalid by spec.
               ctx.error = error_code::syntax_error;
               return;
            }
            else if (*it == ' ' || *it == '\t') {
               // Skip whitespace
            }
            else if (*it == '#') {
               // Block scalar comments require separation whitespace.
               if (it == header_start || (*(it - 1) != ' ' && *(it - 1) != '\t')) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               // Skip comment
               while (it != end && *it != '\n' && *it != '\r') {
                  ++it;
               }
               break;
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
         }

         // Skip newline after indicator
         if (!skip_newline(it, end)) {
            // Empty block scalar
            value.clear();
            return;
         }

         value.clear();

         // Determine content indentation
         int32_t content_indent = -1;
         int32_t leading_blank_indent_max = -1;
         bool first_line = true;
         bool previous_line_starts_with_tab = false;
         std::string trailing_newlines;

         while (it != end) {
            auto line_start = it;
            int32_t line_indent = measure_indent<false>(it, end, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;

            // Check for blank line
            if (it == end || *it == '\n' || *it == '\r') {
               if (content_indent < 0) {
                  leading_blank_indent_max = (std::max)(leading_blank_indent_max, line_indent);
               }
               trailing_newlines.push_back('\n');
               skip_newline(it, end);
               continue;
            }

            // Top-level zero-indented block scalars must stop at document boundary markers.
            if (base_indent < 0 && line_indent == 0 && (at_document_start(it, end) || at_document_end(it, end))) {
               it = line_start;
               break;
            }

            // First content line determines indentation
            if (content_indent < 0) {
               if (explicit_indent > 0) {
                  content_indent = base_indent + explicit_indent;
               }
               else {
                  content_indent = line_indent;
               }

               if (content_indent <= base_indent) {
                  it = line_start;
                  break;
               }
               if (leading_blank_indent_max > content_indent) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            // Check if we've dedented
            if (line_indent < content_indent) {
               it = line_start;
               break;
            }

            // Skip to content_indent level
            it = line_start;
            for (int32_t i = 0; i < content_indent && it != end && *it == ' '; ++i) {
               ++it;
            }

            const bool current_line_starts_with_tab = (it != end && *it == '\t');

            // Add previous newlines (unless this is the first line)
            if (!first_line) {
               if (indicator == '|') {
                  // Literal: preserve newlines
                  value += trailing_newlines;
               }
               else {
                  // Folded: single newline becomes space, paragraph breaks keep one newline.
                  // When a paragraph break is adjacent to a tab-leading line, preserve it fully.
                  const size_t break_count = trailing_newlines.size();
                  if (break_count == 1) {
                     value.push_back(' ');
                  }
                  else if (break_count > 1) {
                     const bool preserve_all = previous_line_starts_with_tab || current_line_starts_with_tab;
                     const size_t preserve_count = preserve_all ? break_count : (break_count - 1);
                     value.append(preserve_count, '\n');
                  }
               }
            }
            trailing_newlines.clear();
            first_line = false;
            previous_line_starts_with_tab = current_line_starts_with_tab;

            // Read line content
            while (it != end && *it != '\n' && *it != '\r') {
               value.push_back(*it);
               ++it;
            }

            trailing_newlines.push_back('\n');
            skip_newline(it, end);
         }

         // Apply chomping
         if (chomping == '-') {
            // Strip: remove all trailing newlines
         }
         else if (chomping == '+') {
            // Keep: preserve all trailing newlines
            value += trailing_newlines;
         }
         else {
            // Clip: single trailing newline
            if (!value.empty() || !trailing_newlines.empty()) {
               value.push_back('\n');
            }
         }
      }

      // Parse a YAML key (unquoted or quoted)
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE bool parse_yaml_key(std::string& key, Ctx& ctx, It& it, End end, bool in_flow)
      {
         key.clear();
         skip_inline_ws(it, end);

         if (it == end) {
            ctx.error = error_code::unexpected_end;
            return false;
         }

         // Tags on keys (e.g. "!!str : value")
         const auto tag = parse_yaml_tag(it, end);
         if (tag == yaml_tag::unknown) {
            ctx.error = error_code::syntax_error;
            return false;
         }
         if (!tag_valid_for_string(tag)) {
            ctx.error = error_code::syntax_error;
            return false;
         }
         skip_inline_ws(it, end);
         if (it == end) {
            ctx.error = error_code::unexpected_end;
            return false;
         }

         // Handle alias as key (*name resolves to anchor value)
         if (*it == '*') {
            ++it;
            auto name = parse_anchor_name(it, end);
            if (name.empty()) {
               ctx.error = error_code::syntax_error;
               return false;
            }
            auto anchor_it = ctx.anchors.find(name);
            if (anchor_it == ctx.anchors.end()) {
               ctx.error = error_code::syntax_error; // undefined alias
               return false;
            }
            auto& span = anchor_it->second;
            if (span.begin == span.end) {
               key.clear(); // empty anchor
            }
            else {
               // Replay the anchor span to extract the key string
               auto replay_it = span.begin;
               auto replay_end = span.end;
               if (*replay_it == '"') {
                  parse_double_quoted_string(key, ctx, replay_it, replay_end);
               }
               else if (*replay_it == '\'') {
                  parse_single_quoted_string(key, ctx, replay_it, replay_end);
               }
               else if (*replay_it == '[' || *replay_it == '{') {
                  // Canonicalize complex alias keys (flow seq/map) to a stable string form.
                  glz::generic key_node{};
                  yaml_context temp_ctx{};
                  temp_ctx.indent_stack = ctx.indent_stack;
                  temp_ctx.anchors = ctx.anchors;
                  temp_ctx.stream_begin = ctx.stream_begin;
                  from<YAML, glz::generic>::template op<flow_context_on<opts{.error_on_unknown_keys = false}>()>(
                     key_node, temp_ctx, replay_it, replay_end);
                  if (bool(temp_ctx.error)) [[unlikely]] {
                     ctx.error = temp_ctx.error;
                     return false;
                  }
                  ctx.anchors = std::move(temp_ctx.anchors);

                  if (auto* s = key_node.template get_if<std::string>()) {
                     key = *s;
                  }
                  else {
                     (void)glz::write_json(key_node, key);
                  }
               }
               else {
                  parse_plain_scalar(key, ctx, replay_it, replay_end, false);
               }
            }
            return !bool(ctx.error);
         }

         // Handle anchor on key (&name before key text)
         bool has_key_anchor = false;
         std::string key_anchor_name;
         if (*it == '&') {
            ++it;
            auto name = parse_anchor_name(it, end);
            if (name.empty()) {
               ctx.error = error_code::syntax_error;
               return false;
            }
            skip_inline_ws(it, end);
            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            // Anchor on alias is invalid per YAML spec
            if (*it == '*') {
               ctx.error = error_code::syntax_error;
               return false;
            }
            has_key_anchor = true;
            key_anchor_name = std::string(name);
         }

         const char* key_start = &*it;
         bool quoted_key_spans_lines = false;

         if (*it == '"') {
            auto quoted_start = it;
            parse_double_quoted_string(key, ctx, it, end);
            if (!bool(ctx.error)) {
               for (auto p = quoted_start; p != it; ++p) {
                  if (*p == '\n' || *p == '\r') {
                     quoted_key_spans_lines = true;
                     break;
                  }
               }
            }
         }
         else if (*it == '\'') {
            auto quoted_start = it;
            parse_single_quoted_string(key, ctx, it, end);
            if (!bool(ctx.error)) {
               for (auto p = quoted_start; p != it; ++p) {
                  if (*p == '\n' || *p == '\r') {
                     quoted_key_spans_lines = true;
                     break;
                  }
               }
            }
         }
         else {
            // Plain key - read until colon
            while (it != end) {
               const char c = *it;
               if (c == ':') {
                  // Check if this ends the key
                  if ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n' ||
                      *(it + 1) == '\r') {
                     break;
                  }
                  if (in_flow && (*(it + 1) == ',' || *(it + 1) == ']' || *(it + 1) == '}')) {
                     break;
                  }
               }
               if (c == '\n' || c == '\r') {
                  if (!in_flow) break;

                  auto continuation = it;
                  skip_newline(continuation, end);
                  int32_t continuation_indent = 0;
                  while (continuation != end && (*continuation == ' ' || *continuation == '\t')) {
                     if (*continuation == ' ') ++continuation_indent;
                     ++continuation;
                  }

                  if (continuation == end) {
                     break;
                  }

                  if (*continuation == '#') {
                     it = continuation;
                     skip_comment(it, end);
                     if (it != end && (*it == '\n' || *it == '\r')) {
                        skip_newline(it, end);
                        continue;
                     }
                     break;
                  }

                  // A continuation line starting with flow terminators or ':' does
                  // not belong to the key content.
                  if (*continuation == ',' || *continuation == ']' || *continuation == '}' || *continuation == ':') {
                     break;
                  }

                  if (ctx.explicit_mapping_key_context) {
                     if (*continuation == '?' || *continuation == ':') {
                        auto after = continuation + 1;
                        if (after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') {
                           break;
                        }
                     }

                     if (*continuation == '&' || *continuation == '*' || *continuation == '!') {
                        break;
                     }

                     if (*continuation == '-') {
                        auto after = continuation + 1;
                        if (after == end || *after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') {
                           break;
                        }
                     }

                     if (ctx.current_indent() >= 0 && continuation_indent <= ctx.current_indent()) {
                        break;
                     }

                     // A continuation line that contains an implicit mapping-key
                     // indicator (": " / ":\n") starts a new entry, not key text.
                     {
                        auto scan = continuation;
                        while (scan != end && *scan != '\n' && *scan != '\r') {
                           if (*scan == ':') {
                              auto after_colon = scan + 1;
                              const bool tight_key_colon =
                                 (scan == continuation) || (*(scan - 1) != ' ' && *(scan - 1) != '\t');
                              if (tight_key_colon &&
                                  (after_colon == end || *after_colon == ' ' || *after_colon == '\t' ||
                                   *after_colon == '\n' || *after_colon == '\r')) {
                                 break;
                              }
                           }
                           ++scan;
                        }
                        if (scan != end && *scan == ':') {
                           break;
                        }
                     }
                  }

                  if (!key.empty() && key.back() != ' ') {
                     key.push_back(' ');
                  }
                  it = continuation;
                  continue;
               }
               if (in_flow && (c == ',' || c == ']' || c == '}')) break;
               if (c == '#') break;

               key.push_back(c);
               ++it;
            }

            // Trim trailing whitespace from key
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
               key.pop_back();
            }

            // Empty keys are valid YAML (":" in block or flow mappings).
         }

         if (bool(ctx.error)) return false;

         // Implicit mapping keys must be single-line in both block and flow styles.
         if (quoted_key_spans_lines && !in_flow) {
            ctx.error = error_code::syntax_error;
            return false;
         }

         // Store anchor on key if present
         if (has_key_anchor) {
            ctx.anchors[std::move(key_anchor_name)] = {key_start, &*it, ctx.current_indent()};
         }

         return true;
      }

   } // namespace yaml

   // String types
   template <str_t T>
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_string(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts, yaml::node_property_policy{true, false, false, true}>(
                value, ctx, it, end, node_props))
            return;

         std::string str;
         const auto style = yaml::detect_scalar_style(*it);

         switch (style) {
         case yaml::scalar_style::double_quoted:
            yaml::parse_double_quoted_string(str, ctx, it, end);
            break;
         case yaml::scalar_style::single_quoted:
            yaml::parse_single_quoted_string(str, ctx, it, end);
            break;
         case yaml::scalar_style::literal_block:
         case yaml::scalar_style::folded_block:
            // For same-line mapping/sequence values, parsing context is typically pushed
            // one column past the key/item indicator; block scalar indentation is relative
            // to the parent line indent.
            yaml::parse_block_scalar(str, ctx, it, end,
                                     ctx.current_indent() > 0 ? (ctx.current_indent() - 1) : ctx.current_indent());
            break;
         case yaml::scalar_style::plain:
            // In block context with known indent, use multiline parsing to support
            // plain scalars that span multiple lines (folding behavior)
            if constexpr (!yaml::check_flow_context(Opts)) {
               if (ctx.current_indent() >= 0) {
                  yaml::parse_plain_scalar_multiline(str, ctx, it, end, ctx.current_indent());
               }
               else {
                  // Top-level plain scalars may continue on same-indent or indented lines.
                  yaml::parse_plain_scalar_multiline(str, ctx, it, end, int32_t(0));
               }
            }
            else {
               yaml::parse_plain_scalar(str, ctx, it, end, true);
            }
            break;
         }

         if (!bool(ctx.error)) {
            value = std::move(str);
            yaml::finalize_node_anchor(node_props, ctx, it);
         }
      }
   };

   // Boolean types
   template <bool_t T>
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_bool(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts>(value, ctx, it, end, node_props)) return;

         // Parse as plain scalar first
         std::string str;
         yaml::parse_plain_scalar(str, ctx, it, end, yaml::check_flow_context(Opts));

         if (str == "true" || str == "True" || str == "TRUE" || str == "yes" || str == "Yes" || str == "YES" ||
             str == "on" || str == "On" || str == "ON") {
            value = true;
         }
         else if (str == "false" || str == "False" || str == "FALSE" || str == "no" || str == "No" || str == "NO" ||
                  str == "off" || str == "Off" || str == "OFF") {
            value = false;
         }
         else {
            ctx.error = error_code::expected_true_or_false;
         }

         yaml::finalize_node_anchor(node_props, ctx, it);
      }
   };

   // Numeric types
   template <num_t T>
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         // Validate tag for numeric types
         if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
            if (!yaml::tag_valid_for_float(tag)) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
         else {
            // Integer types
            if (!yaml::tag_valid_for_int(tag)) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts>(value, ctx, it, end, node_props)) return;

         auto finalize = [&] {
            yaml::finalize_node_anchor(node_props, ctx, it);
         };

         // Check for special float values
         if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
            auto start = it;

            // Check for .inf, .nan, -.inf, etc.
            if (*it == '.') {
               ++it;
               if (it + 3 <= end && (std::string_view(it, 3) == "inf" || std::string_view(it, 3) == "Inf" ||
                                     std::string_view(it, 3) == "INF")) {
                  value = std::numeric_limits<std::remove_cvref_t<T>>::infinity();
                  it += 3;
                  finalize();
                  return;
               }
               if (it + 3 <= end && (std::string_view(it, 3) == "nan" || std::string_view(it, 3) == "NaN" ||
                                     std::string_view(it, 3) == "NAN")) {
                  value = std::numeric_limits<std::remove_cvref_t<T>>::quiet_NaN();
                  it += 3;
                  finalize();
                  return;
               }
               it = start;
            }

            if (*it == '-' || *it == '+') {
               const char sign = *it;
               ++it;
               if (it != end && *it == '.') {
                  ++it;
                  if (it + 3 <= end && (std::string_view(it, 3) == "inf" || std::string_view(it, 3) == "Inf" ||
                                        std::string_view(it, 3) == "INF")) {
                     if (sign == '-') {
                        value = -std::numeric_limits<std::remove_cvref_t<T>>::infinity();
                     }
                     else {
                        value = std::numeric_limits<std::remove_cvref_t<T>>::infinity();
                     }
                     it += 3;
                     finalize();
                     return;
                  }
               }
               it = start;
            }
         }

         // Parse as plain scalar and convert
         auto start = it;

         // Find end of number - use same rules as plain scalar for colons
         while (it != end) {
            const char c = *it;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ']' || c == '}' || c == '#') {
               break;
            }
            // Colon only ends number if followed by space/tab/newline/end (YAML mapping indicator)
            if (c == ':') {
               if ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n' || *(it + 1) == '\r') {
                  break;
               }
            }
            ++it;
         }

         const std::string_view num_str(&*start, static_cast<size_t>(it - start));

         if (num_str.empty()) {
            ctx.error = error_code::parse_number_failure;
            return;
         }

         // Handle hex, octal, binary
         if constexpr (std::integral<std::remove_cvref_t<T>>) {
            if (num_str.size() > 2 && num_str[0] == '0') {
               int base = 10;
               size_t offset = 0;

               if (num_str[1] == 'x' || num_str[1] == 'X') {
                  base = 16;
                  offset = 2;
               }
               else if (num_str[1] == 'o' || num_str[1] == 'O') {
                  base = 8;
                  offset = 2;
               }
               else if (num_str[1] == 'b' || num_str[1] == 'B') {
                  base = 2;
                  offset = 2;
               }

               if (base != 10) {
                  const auto digits = num_str.substr(offset);
                  const bool has_underscores = (digits.find('_') != std::string_view::npos);

                  std::string clean_storage;
                  std::string_view clean_view;

                  if (has_underscores) {
                     clean_storage.reserve(digits.size());
                     for (char c : digits) {
                        if (c != '_') {
                           clean_storage.push_back(c);
                        }
                     }
                     clean_view = clean_storage;
                  }
                  else {
                     clean_view = digits;
                  }

                  auto [ptr, ec] =
                     std::from_chars(clean_view.data(), clean_view.data() + clean_view.size(), value, base);
                  if (ec != std::errc{}) {
                     ctx.error = error_code::parse_number_failure;
                  }
                  finalize();
                  return;
               }
            }
         }

         // Standard number parsing - only allocate if underscores present
         const bool has_underscores = (num_str.find('_') != std::string_view::npos);

         std::string clean_storage;
         std::string_view clean_view;

         if (has_underscores) {
            clean_storage.reserve(num_str.size());
            for (char c : num_str) {
               if (c != '_') {
                  clean_storage.push_back(c);
               }
            }
            clean_view = clean_storage;
         }
         else {
            clean_view = num_str;
         }

         // YAML allows leading '+' for positive numbers, but C++ from_chars doesn't
         // Strip leading '+' if present
         auto parse_view = clean_view;
         if (!parse_view.empty() && parse_view.front() == '+') {
            parse_view.remove_prefix(1);
            if (parse_view.empty()) {
               ctx.error = error_code::parse_number_failure;
               return;
            }
         }

         if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
            auto result = glz::fast_float::from_chars(parse_view.data(), parse_view.data() + parse_view.size(), value);
            // Check both for errors and that all input was consumed (e.g., "12:30" is not a valid number)
            if (result.ec != std::errc{} || result.ptr != parse_view.data() + parse_view.size()) {
               ctx.error = error_code::parse_number_failure;
            }
         }
         else {
            auto [ptr, ec] = std::from_chars(parse_view.data(), parse_view.data() + parse_view.size(), value);
            // Check both for errors and that all input was consumed
            if (ec != std::errc{} || ptr != parse_view.data() + parse_view.size()) {
               ctx.error = error_code::parse_number_failure;
            }
         }

         finalize();
      }
   };

   // Nullable types (std::optional, pointers)
   template <nullable_like T>
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) {
            value = {};
            return;
         }

         // Check for tag - but don't consume it yet if it's not a null tag
         auto tag_start = it;
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         // If it's explicitly a null tag, set to null
         if (tag == yaml::yaml_tag::null_tag) {
            yaml::skip_inline_ws(it, end);
            // Skip the null value if present
            if (it != end && !yaml::flow_context_end_table[static_cast<uint8_t>(*it)]) {
               std::string str;
               yaml::parse_plain_scalar(str, ctx, it, end, yaml::check_flow_context(Opts));
            }
            value = {};
            return;
         }

         // If no tag or a non-null tag, reset and check value
         if (tag == yaml::yaml_tag::none) {
            it = tag_start;
         }

         yaml::skip_inline_ws(it, end);

         // Handle alias for the whole nullable value
         if (yaml::handle_alias<Opts>(value, ctx, it, end)) return;
         // Anchors (&name) pass through to the inner type handler

         // Check for null value (without tag)
         if (tag == yaml::yaml_tag::none) {
            auto start = it;
            std::string str;
            yaml::parse_plain_scalar(str, ctx, it, end, yaml::check_flow_context(Opts));

            if (yaml::is_yaml_null(str)) {
               value = {};
               return;
            }

            // Not null - reset and parse the actual value
            it = start;
         }

         if (!value) {
            if constexpr (requires { value.emplace(); }) {
               value.emplace();
            }
            else {
               ctx.error = error_code::invalid_nullable_read;
               return;
            }
         }

         using V = std::remove_cvref_t<decltype(*value)>;
         from<YAML, V>::template op<Opts>(*value, ctx, it, end);
      }
   };

   // std::nullptr_t - null literal type
   template <>
   struct from<YAML, std::nullptr_t>
   {
      template <auto Opts, class It>
      static void op(std::nullptr_t&, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for null values: null, Null, NULL, ~
         if (*it == '~') {
            ++it;
            return;
         }

         // Parse as plain scalar and check if it's a null keyword
         auto start = it;
         while (it != end && !yaml::plain_scalar_end_table[static_cast<uint8_t>(*it)]) {
            ++it;
         }

         std::string_view str{start, static_cast<size_t>(it - start)};
         if (!yaml::is_yaml_null(str)) {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Enum with glz::meta - reads string representation
   template <class T>
      requires(is_named_enum<T>)
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto& value, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag - named enums are read as strings
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_string(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts>(value, ctx, it, end, node_props)) return;

         // Parse as string (quoted or plain)
         std::string str;
         const auto style = yaml::detect_scalar_style(*it);

         if (style == yaml::scalar_style::double_quoted) {
            yaml::parse_double_quoted_string(str, ctx, it, end);
         }
         else if (style == yaml::scalar_style::single_quoted) {
            yaml::parse_single_quoted_string(str, ctx, it, end);
         }
         else {
            yaml::parse_plain_scalar(str, ctx, it, end, yaml::check_flow_context(Opts));
         }

         if (bool(ctx.error)) [[unlikely]]
            return;

         constexpr auto N = reflect<T>::size;

         if constexpr (N == 1) {
            static constexpr auto key = glz::get<0>(reflect<T>::keys);
            if (str == key) {
               value = glz::get<0>(reflect<T>::values);
            }
            else {
               ctx.error = error_code::unexpected_enum;
            }
         }
         else {
            static constexpr auto HashInfo = hash_info<T>;
            const auto index = decode_hash_with_size<YAML, T, HashInfo, HashInfo.type>::op(
               str.data(), str.data() + str.size(), str.size());

            if (index >= N) [[unlikely]] {
               ctx.error = error_code::unexpected_enum;
               return;
            }

            visit<N>(
               [&]<size_t I>() {
                  static constexpr auto key = glz::get<I>(reflect<T>::keys);
                  if (str == key) [[likely]] {
                     value = glz::get<I>(reflect<T>::values);
                  }
                  else {
                     ctx.error = error_code::unexpected_enum;
                  }
               },
               index);
         }

         yaml::finalize_node_anchor(node_props, ctx, it);
      }
   };

   // Raw enum (without glz::meta) - reads as underlying numeric type
   template <class T>
      requires(std::is_enum_v<T> && !glaze_enum_t<T> && !meta_keys<T> && !custom_read<T>)
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto& value, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         std::underlying_type_t<std::decay_t<T>> x{};
         from<YAML, decltype(x)>::template op<Opts>(x, ctx, it, end);
         value = static_cast<std::decay_t<T>>(x);
      }
   };

   namespace yaml
   {
      // Parse flow sequence [item, item, ...]
      template <auto Opts, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_flow_sequence(T&& value, Ctx& ctx, It& it, End end)
      {
         using V = std::remove_cvref_t<T>;

         if (it == end || *it != '[') [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         ++it; // Skip '['
         // Skip whitespace and newlines after opening bracket (YAML allows multi-line flow sequences)
         skip_flow_ws_and_newlines(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;

         // Handle empty array
         if (it != end && *it == ']') {
            ++it;
            validate_flow_node_adjacent_tail(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            if constexpr (!yaml::check_flow_context(Opts)) {
               if (ctx.current_indent() < 0) {
                  validate_root_flow_tail_after_close(ctx, it, end);
               }
            }
            return;
         }

         using value_type = typename V::value_type;
         bool just_saw_comma = false;

         if constexpr (emplace_backable<V>) {
            // Resizable containers (vector, deque, list)
            while (it != end) {
               skip_flow_ws_and_newlines(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;


               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if (*it == ']') {
                  if (just_saw_comma) {
                     ++it;
                     validate_flow_node_adjacent_tail(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     if constexpr (!yaml::check_flow_context(Opts)) {
                        if (ctx.current_indent() < 0) {
                           validate_root_flow_tail_after_close(ctx, it, end);
                        }
                     }
                     return;
                  }
                  ctx.error = error_code::syntax_error;
                  return;
               }

               if (*it == ',' || *it == '#') {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               just_saw_comma = false;

               auto& element = value.emplace_back();
               from<YAML, value_type>::template op<flow_context_on<Opts>()>(element, ctx, it, end);

               if (bool(ctx.error)) [[unlikely]]
                  return;

               bool saw_line_break_before_separator = false;
               skip_flow_ws_and_newlines(ctx, it, end, &saw_line_break_before_separator);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (saw_line_break_before_separator && it != end && *it == ',') {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               if (it != end && *it == ':') {
                  if (saw_line_break_before_separator) {
                     ctx.error = error_code::syntax_error;

                     return;
                  }
                  if constexpr (std::same_as<std::remove_cvref_t<value_type>, glz::generic>) {
                     glz::generic key_node = std::move(element);
                     ++it;
                     skip_inline_ws(it, end);

                     glz::generic mapped{};
                     from<YAML, glz::generic>::template op<flow_context_on<Opts>()>(mapped, ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     std::string key;
                     if (key_node.is_null()) {
                        key.clear();
                     }
                     else if (auto* s = key_node.template get_if<std::string>()) {
                        key = *s;
                     }
                     else {
                        (void)glz::write_json(key_node, key);
                     }

                     glz::generic pair{};
                     pair[key] = std::move(mapped);
                     element = std::move(pair);

                     skip_flow_ws_and_newlines(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
                  else {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }

               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if (*it == ']') {
                  ++it;
                  validate_flow_node_adjacent_tail(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  if constexpr (!yaml::check_flow_context(Opts)) {
                     if (ctx.current_indent() < 0) {
                        validate_root_flow_tail_after_close(ctx, it, end);
                     }
                  }
                  return;
               }
               else if (*it == ',') {
                  ++it;
                  just_saw_comma = true;
                  skip_flow_ws_and_newlines(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
         else if constexpr (!resizable<V>) {
            // Fixed-size containers (std::array)
            const auto n = value.size();
            size_t i = 0;

            while (it != end && i < n) {
               skip_flow_ws_and_newlines(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if (*it == ']') {
                  if (just_saw_comma) {
                     ++it;
                     validate_flow_node_adjacent_tail(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     if constexpr (!yaml::check_flow_context(Opts)) {
                        if (ctx.current_indent() < 0) {
                           validate_root_flow_tail_after_close(ctx, it, end);
                        }
                     }
                     return;
                  }
                  ctx.error = error_code::syntax_error;
                  return;
               }

               if (*it == ',' || *it == '#') {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               just_saw_comma = false;

               from<YAML, value_type>::template op<flow_context_on<Opts>()>(value[i], ctx, it, end);

               if (bool(ctx.error)) [[unlikely]]
                  return;

               ++i;
               bool saw_line_break_before_separator = false;
               skip_flow_ws_and_newlines(ctx, it, end, &saw_line_break_before_separator);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (saw_line_break_before_separator && it != end && *it == ',') {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if (*it == ']') {
                  ++it;
                  validate_flow_node_adjacent_tail(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  if constexpr (!yaml::check_flow_context(Opts)) {
                     if (ctx.current_indent() < 0) {
                        validate_root_flow_tail_after_close(ctx, it, end);
                     }
                  }
                  return;
               }
               else if (*it == ',') {
                  ++it;
                  just_saw_comma = true;
                  skip_flow_ws_and_newlines(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            // If we exited because i >= n but haven't seen ']', skip to the closing bracket
            // This handles the case where YAML has more elements than the fixed-size array
            int bracket_depth = 1;
            while (it != end && bracket_depth > 0) {
               if (*it == '[') {
                  ++bracket_depth;
               }
               else if (*it == ']') {
                  --bracket_depth;
               }
               ++it;
            }
         }
         else {
            ctx.error = error_code::syntax_error;
            return;
         }
      }

      // Parse flow mapping {key: value, ...}
      template <auto Opts, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_flow_mapping(T&& value, Ctx& ctx, It& it, End end)
      {
         using U = std::remove_cvref_t<T>;
         static constexpr auto N = reflect<U>::size;
         static constexpr auto HashInfo = hash_info<U>;

         if (it == end || *it != '{') [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         ++it; // Skip '{'
         skip_flow_ws_and_newlines(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;

         // Handle empty mapping
         if (it != end && *it == '}') {
            ++it;
            return;
         }

         while (it != end) {
            skip_flow_ws_and_newlines(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            if (it != end && *it == '}') {
               ++it;
               validate_flow_node_adjacent_tail(ctx, it, end);
               return;
            }

            // Parse key using thread-local buffer to avoid allocation
            auto& key = string_buffer();
            key.clear();
            if (!parse_yaml_key(key, ctx, it, end, true)) {
               return;
            }

            // Separation between flow key and ':' may include comments/newlines.
            // A bare line break without a comment between key and ':' is invalid.
            bool saw_key_comment = false;
            while (it != end) {
               skip_inline_ws(it, end);
               if (it != end && *it == '#') {
                  skip_comment(it, end);
                  saw_key_comment = true;
               }
               if (it != end && (*it == '\n' || *it == '\r')) {
                  if (!saw_key_comment) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  skip_newline(it, end);
                  continue;
               }
               break;
            }

            // Expect colon
            if (it == end || *it != ':') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
            skip_flow_ws_and_newlines(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            // Look up key and parse value
            const auto index = decode_hash_with_size<YAML, U, HashInfo, HashInfo.type>::op(
               key.data(), key.data() + key.size(), key.size());

            const bool key_matches = index < N && std::string_view{key} == reflect<U>::keys[index];

            if (key_matches) [[likely]] {
               visit<N>(
                  [&]<size_t I>() {
                     if (I == index) {
                        decltype(auto) member = [&]() -> decltype(auto) {
                           if constexpr (reflectable<U>) {
                              return get<I>(to_tie(value));
                           }
                           else {
                              return get_member(value, get<I>(reflect<U>::values));
                           }
                        }();

                        using member_type = std::decay_t<decltype(member)>;
                        from<YAML, member_type>::template op<flow_context_on<Opts>()>(member, ctx, it, end);
                     }
                     return !bool(ctx.error);
                  },
                  index);
            }
            else {
               if constexpr (Opts.error_on_unknown_keys) {
                  ctx.error = error_code::unknown_key;
                  return;
               }
               else { // else used to fix MSVC unreachable code warning
                  skip_yaml_value<Opts>(ctx, it, end, 0, true);
               }
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            skip_inline_ws(it, end);

            if (it != end && *it == '}') {
               ++it;
               validate_flow_node_adjacent_tail(ctx, it, end);
               return;
            }
            else if (it != end && *it == ',') {
               ++it;
               skip_inline_ws(it, end);
            }
            else if (it != end && (*it == '\n' || *it == '\r')) {
               // Newlines without a separating comma are only valid before the
               // closing '}' of the current flow mapping.
               skip_flow_ws_and_newlines(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (it != end && *it == '}') {
                  ++it;
                  validate_flow_node_adjacent_tail(ctx, it, end);
                  return;
               }
               ctx.error = error_code::syntax_error;
               return;
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
      }

      // Detects a plain "key: value" mapping indicator in an inline block-map
      // value segment, while ignoring quoted strings and nested flow collections.
      template <class It, class End>
      GLZ_ALWAYS_INLINE bool inline_value_has_plain_mapping_indicator(It pos, End end) noexcept
      {
         int flow_depth = 0;
         while (pos != end) {
            const char c = *pos;
            if (c == '\n' || c == '\r' || c == '#') return false;
            if (c == '"' || c == '\'') {
               const char quote = c;
               ++pos;
               while (pos != end && *pos != quote) {
                  if (*pos == '\\' && quote == '"') {
                     ++pos;
                     if (pos != end) ++pos;
                  }
                  else {
                     ++pos;
                  }
               }
               if (pos != end) ++pos;
               continue;
            }
            if (c == '[' || c == '{') {
               ++flow_depth;
               ++pos;
               continue;
            }
            if ((c == ']' || c == '}') && flow_depth > 0) {
               --flow_depth;
               ++pos;
               continue;
            }
            if (c == ':' && flow_depth == 0) {
               const auto next = pos + 1;
               return (next == end) || *next == ' ' || *next == '\t' || *next == '\n' || *next == '\r';
            }
            ++pos;
         }
         return false;
      }

      // Parse block sequence
      // - item1
      // - item2
      template <auto Opts, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_block_sequence(T&& value, Ctx& ctx, It& it, End end, int32_t sequence_indent)
      {
         using V = std::remove_cvref_t<T>;
         using value_type = typename V::value_type;

         [[maybe_unused]] size_t index = 0;
         [[maybe_unused]] const size_t max_size = []() {
            if constexpr (!resizable<V>) {
               return std::tuple_size_v<V>;
            }
            else {
               return size_t(0); // unused for resizable containers
            }
         }();

         // Track if this is the first item (for handling whitespace-consumed case)
         bool first_item = true;
         bool has_pending_item_anchor = false;
         std::string pending_item_anchor_name{};
         int32_t pending_item_anchor_indent = sequence_indent;

         while (it != end) {
            // For fixed-size arrays, stop when we've filled all elements
            if constexpr (!resizable<V>) {
               if (index >= max_size) {
                  return;
               }
            }

            // Skip blank lines and comments, track indent
            int32_t line_indent = 0;
            auto line_start = it;

            while (it != end) {
               if (*it == '#') {
                  skip_comment(it, end);
                  skip_newline(it, end);
                  line_start = it;
                  line_indent = 0;
               }
               else if (*it == '\n' || *it == '\r') {
                  skip_newline(it, end);
                  line_start = it;
                  line_indent = 0;
               }
               else if (*it == ' ') {
                  line_start = it;
                  line_indent = 0;
                  while (it != end && *it == ' ') {
                     ++line_indent;
                     ++it;
                  }

                  bool saw_tab = false;
                  while (it != end && (*it == ' ' || *it == '\t')) {
                     saw_tab = saw_tab || (*it == '\t');
                     ++it;
                  }

                  if (it == end || *it == '\n' || *it == '\r' || *it == '#') {
                     // Blank or comment line - continue to next line
                     if (it != end && *it == '#') {
                        skip_comment(it, end);
                     }
                     line_indent = 0;
                     continue;
                  }

                  if (saw_tab) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  break; // Found content
               }
               else if (*it == '\t') {
                  line_start = it;
                  while (it != end && (*it == ' ' || *it == '\t')) ++it;
                  if (it == end || *it == '\n' || *it == '\r' || *it == '#') {
                     if (it != end && *it == '#') {
                        skip_comment(it, end);
                     }
                     line_indent = 0;
                     continue;
                  }
                  ctx.error = error_code::syntax_error;
                  return;
               }
               else {
                  // At content (no leading space on this line)
                  // If first item and at '-', whitespace was consumed by caller;
                  // treat as having correct indentation
                  if (first_item && *it == '-') {
                     line_indent = sequence_indent;
                     if constexpr (std::is_pointer_v<std::decay_t<It>>) {
                        if (ctx.stream_begin && it > ctx.stream_begin) {
                           auto probe = it;
                           int32_t leading_ws = 0;
                           bool saw_tab = false;
                           while (probe > ctx.stream_begin && (*(probe - 1) == ' ' || *(probe - 1) == '\t')) {
                              --probe;
                              ++leading_ws;
                              saw_tab = saw_tab || (*probe == '\t');
                           }
                           if (probe == ctx.stream_begin || *(probe - 1) == '\n' || *(probe - 1) == '\r') {
                              if (saw_tab) {
                                 ctx.error = error_code::syntax_error;
                                 return;
                              }
                              line_indent = (std::max)(line_indent, leading_ws);
                           }
                        }
                     }
                  }
                  break;
               }
            }

            if (it == end) break;

            // Check for document end marker
            if (at_document_end(it, end)) break;

            // Check for dedent
            if (line_indent < sequence_indent) {
               it = line_start;
               return;
            }

            // YAML allows node properties on a standalone line before a sequence entry:
            // &anchor
            // - value
            if (*it == '&') {
               ++it;
               auto pending_name = parse_anchor_name(it, end);
               if (pending_name.empty()) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               skip_inline_ws(it, end);
               if (it != end && *it == '#') {
                  skip_comment(it, end);
               }

               has_pending_item_anchor = true;
               pending_item_anchor_name = std::string(pending_name);
               pending_item_anchor_indent = line_indent;

               if (it != end && (*it == '\n' || *it == '\r')) {
                  skip_newline(it, end);
                  continue;
               }

               if (it == end || *it != '-') {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            // Expect dash for sequence item
            if (*it != '-') {
               it = line_start;
               return;
            }

            ++it; // Skip '-'

            // Check for valid sequence item indicator (- followed by space or newline)
            if (it != end && !yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*it)]) {
               // Not a sequence item (could be a number like -5)
               it = line_start;
               return;
            }

            bool saw_tab_after_dash = false;
            while (it != end && (*it == ' ' || *it == '\t')) {
               saw_tab_after_dash = saw_tab_after_dash || (*it == '\t');
               ++it;
            }
            // A tab-separated nested "- " marker is not valid block indentation.
            if (saw_tab_after_dash && it != end && *it == '-') {
               const auto after_dash = it + 1;
               if (after_dash == end || yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*after_dash)]) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            // Get reference to element to parse into
            auto parse_element = [&](auto& element) {
               const char* element_start = (it != end) ? &*it : nullptr;

               // Check what follows
               if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                  // Content on same line as dash - set indent to one less than content column
                  // This allows nested block mappings to continue parsing keys at the content indent
                  // For "- key: val", if dash is at column 0, content is at column 2
                  // We set parent indent to 1 so keys at column 2 pass the "indent > parent" check
                  if (!ctx.push_indent(line_indent + 1)) [[unlikely]] return;
                  const bool prev_sequence_item_value_context = ctx.sequence_item_value_context;
                  ctx.sequence_item_value_context = true;
                  from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
                  ctx.sequence_item_value_context = prev_sequence_item_value_context;
                  ctx.pop_indent();
               }
               else {
                  // Value on next line - nested block
                  skip_ws_and_comment(it, end);
                  skip_newline(it, end);

                  // Get indent of nested content
                  auto nested_start = it;
                  int32_t nested_indent = measure_indent(it, end, ctx);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  it = nested_start;

                  // Content is nested only if indented more than the current line.
                  // When line_indent is negative (root level), use 0 as the baseline.
                  // This prevents content at column 0 from being treated as nested.
                  const int32_t effective_line_indent = (line_indent < 0) ? 0 : line_indent;
                  if (nested_indent > effective_line_indent) {
                     // Save and set indent for nested parsing
                     // Set parent indent to one less than content indent so items at
                     // content indent pass the "indent > parent" check and continue parsing
                     if (!ctx.push_indent(nested_indent - 1)) [[unlikely]] return;
                     const bool prev_sequence_item_value_context = ctx.sequence_item_value_context;
                     ctx.sequence_item_value_context = true;
                     from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
                     ctx.sequence_item_value_context = prev_sequence_item_value_context;
                     ctx.pop_indent();
                  }
                  // else: empty element (default constructed)
               }

               if (has_pending_item_anchor && !bool(ctx.error)) {
                  const char* element_end = (it != end) ? &*it : element_start;
                  ctx.anchors[std::move(pending_item_anchor_name)] = {element_start, element_end, pending_item_anchor_indent};
                  has_pending_item_anchor = false;
               }
            };

            if constexpr (emplace_backable<V>) {
               auto& element = value.emplace_back();
               parse_element(element);
            }
            else if constexpr (!resizable<V>) {
               parse_element(value[index]);
               ++index;
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }

            first_item = false;

            if (bool(ctx.error)) [[unlikely]]
               return;

            // Note: after parsing nested block content, iterator may already be
            // at the start of the next line. The outer loop will handle it.
         }
      }

      // Detect whether there is nested block content after a `key:` with no same-line value.
      // Called with `it` positioned after the colon and any trailing inline whitespace.
      // Peeks ahead past blank lines and comment-only lines to find the first content line.
      // Returns the indent of that content if it's nested (> effective line_indent), else -1.
      // On return, `it` is positioned right after the key line's newline.
      // If return >= 0, caller should call skip_to_content() to advance to the content.
      //
      // High-level state machine:
      // 1) Validate that we are at end-of-key line and move to the next line.
      // 2) Scan forward to first real content line, skipping blank/comment lines.
      //    Also skip "property-only" lines (YAML node properties with no value yet).
      // 3) Measure indent and validate tab/structural edge cases.
      // 4) Return nested indent when content is truly nested, otherwise -1.
      template <class Ctx, class It, class End>
      int32_t detect_nested_value_indent(Ctx& ctx, It& it, End end, int32_t line_indent)
      {
         skip_ws_and_comment(it, end);
         if (it == end || (*it != '\n' && *it != '\r')) {
            return -1;
         }
         skip_newline(it, end);
         auto content_start = it;

         // Peek ahead to find the first content line (skip blank/comment lines)
         auto peek = it;
         while (peek != end) {
            if (*peek == '\n' || *peek == '\r') {
               skip_newline(peek, end);
            }
            else if (*peek == '#') {
               skip_comment(peek, end);
            }
            else if (*peek == ' ') {
               auto line_start = peek;
               while (peek != end && *peek == ' ') ++peek;
               if (peek == end || *peek == '\n' || *peek == '\r') {
                  continue; // blank line with trailing spaces
               }
               if (*peek == '#') {
                  skip_comment(peek, end);
                  continue; // comment-only line
               }
               // YAML allows node properties to appear on standalone lines before the
               // actual node value, for example:
               //   key:
               //     !tag
               //     &anchor
               //     actual: value
               // This loop recognizes chains of properties (`!tag`, `&anchor`, optional
               // inline spaces/comments) and treats those lines as non-content so they do
               // not incorrectly define nested indentation.
               {
                  auto prop = peek;
                  bool saw_property = false;
                  while (prop != end) {
                     if (*prop == '!') {
                        const auto tag = parse_yaml_tag(prop, end);
                        if (tag == yaml_tag::unknown) break;
                        saw_property = true;
                        skip_inline_ws(prop, end);
                        continue;
                     }
                     if (*prop == '&') {
                        ++prop;
                        auto aname = parse_anchor_name(prop, end);
                        if (aname.empty()) break;
                        saw_property = true;
                        skip_inline_ws(prop, end);
                        continue;
                     }
                     break;
                  }
                  if (saw_property) {
                     auto tail = prop;
                     skip_inline_ws(tail, end);
                     if (tail == end || *tail == '\n' || *tail == '\r' || *tail == '#') {
                        if (tail != end && *tail == '#') {
                           skip_comment(tail, end);
                        }
                        if (tail != end && (*tail == '\n' || *tail == '\r')) {
                           skip_newline(tail, end);
                        }
                        peek = tail;
                        continue;
                     }
                  }
               }
               peek = line_start; // content found, restore to measure properly
               break;
            }
            else {
               break; // content at column 0
            }
         }

         // Measure indent of the content line
         int32_t content_indent = measure_indent<false>(peek, end, ctx);
         if (bool(ctx.error)) [[unlikely]] return -1;

         // Tabs in indentation-like positions are only allowed when they are plain
         // scalar content. Structural lines (mapping keys/entries) remain errors.
         if (peek != end && *peek == '\t') {
            auto probe = peek;
            while (probe != end && (*probe == ' ' || *probe == '\t')) ++probe;
            if (probe != end && *probe != '\n' && *probe != '\r' && *probe != '#') {
               const bool looks_sequence_entry =
                  (*probe == '-') && ((probe + 1) == end || yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*(probe + 1))]);
               const bool looks_explicit_entry =
                  (*probe == '?' || *probe == ':') &&
                  ((probe + 1) == end || yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*(probe + 1))]);
               bool looks_mapping_key = false;
               auto scan = probe;
               while (scan != end && *scan != '\n' && *scan != '\r') {
                  if (*scan == ':') {
                     const auto after = scan + 1;
                     if (after == end || yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*after)]) {
                        looks_mapping_key = true;
                        break;
                     }
                  }
                  ++scan;
               }
               if (looks_sequence_entry || looks_explicit_entry || looks_mapping_key) {
                  ctx.error = error_code::syntax_error;
                  return -1;
               }
            }
         }

         const int32_t effective_line_indent = (line_indent < 0) ? 0 : line_indent;
         if (content_indent > effective_line_indent && peek != end && *peek != '\n' && *peek != '\r') {
            it = content_start;
            return content_indent;
         }

         // YAML allows "indentless sequences" as mapping values:
         // key:
         // - item
         if (content_indent == effective_line_indent && peek != end && *peek == '-') {
            const auto after_dash = peek + 1;
            if (after_dash == end || yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*after_dash)]) {
               it = content_start;
               return content_indent;
            }
         }

         it = content_start;
         return -1;
      }

      // Skip blank lines and comment-only lines.
      // Leaves `it` at the start of indentation whitespace of the first content line.
      template <class It, class End>
      void skip_to_content(It& it, End end)
      {
         while (it != end) {
            if (*it == '\n' || *it == '\r') {
               skip_newline(it, end);
            }
            else if (*it == '#') {
               skip_comment(it, end);
            }
            else if (*it == ' ') {
               auto line_start = it;
               while (it != end && *it == ' ') ++it;
               if (it == end || *it == '\n' || *it == '\r') {
                  continue; // blank line with trailing spaces
               }
               if (*it == '#') {
                  skip_comment(it, end);
                  continue; // comment-only line
               }
               it = line_start; // content found, restore to start of indent
               return;
            }
            else {
               return; // content at column 0
            }
         }
      }

      // Shared loop for block mapping parsing.
      // Handles blank/comment skipping, indent detection, dedent, and trailing whitespace.
      // process_entry(ctx, it, end, line_indent) should parse key+colon+value and return
      // true to continue or false to stop.
      //
      // mapping_indent >= 0: caller knows the key indent (struct case, first key may be mid-line)
      // mapping_indent < 0: discover from first key (map case)
      template <auto Opts, class Ctx, class It, class End, class ProcessEntry>
      void parse_block_mapping_loop(Ctx& ctx, It& it, End end,
                                    int32_t mapping_indent,
                                    ProcessEntry&& process_entry)
      {
         const int32_t parent_indent = ctx.current_indent();
         const bool discover_indent = (mapping_indent < 0);
         bool first_key = !discover_indent;
         bool discovered_first_key_mid_line = false;
         int32_t discovered_first_key_visual_indent = 0;

         while (it != end) {
            auto line_start = it;
            int32_t line_indent = first_key ? mapping_indent : 0;

            // Skip blank lines and comments, measure indent
            while (it != end) {
               if (*it == '#') {
                  skip_comment(it, end);
                  skip_newline(it, end);
                  first_key = false;
                  line_indent = 0;
               }
               else if (*it == '\n' || *it == '\r') {
                  skip_newline(it, end);
                  first_key = false;
                  line_indent = 0;
               }
               else if (*it == ' ') {
                  line_start = it;
                  line_indent = 0;
                  while (it != end && *it == ' ') {
                     ++line_indent;
                     ++it;
                  }

                  bool saw_tab = false;
                  while (it != end && (*it == ' ' || *it == '\t')) {
                     saw_tab = saw_tab || (*it == '\t');
                     ++it;
                  }

                  if (it == end || *it == '\n' || *it == '\r' || *it == '#') {
                     first_key = false;
                     line_indent = 0;
                     continue; // Blank or comment line
                  }

                  if (saw_tab) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }

                  // Early dedent check
                  if (mapping_indent >= 0 && line_indent < mapping_indent) {
                     it = line_start;
                     return;
                  }

                  first_key = false;
                  break; // Found content
               }
               else if (*it == '\t') {
                  line_start = it;
                  while (it != end && (*it == ' ' || *it == '\t')) ++it;
                  if (it == end || *it == '\n' || *it == '\r' || *it == '#') {
                     first_key = false;
                     line_indent = 0;
                     continue; // Blank or comment-only line with tab indentation
                  }
                  ctx.error = error_code::syntax_error;
                  return;
               }
               else {
                  line_start = it;
                  break; // Content at column 0
               }
            }

            if (it == end) break;

            // Document boundaries terminate the current mapping in both discovered
            // and fixed-indent modes.
            if (at_document_end(it, end) || at_document_start(it, end)) break;

            // Dedent checks (skip on first key for struct case)
            // Must check BEFORE establishing mapping_indent so the first discovered key
            // isn't rejected by the parent_indent check (mapping_indent is still -1).
            if (!first_key) {
               // Parent indent check for nested maps (only after mapping_indent established)
               if (discover_indent && mapping_indent >= 0 && parent_indent >= 0 && line_indent <= parent_indent) {
                  it = line_start;
                  return;
               }
               if (mapping_indent >= 0 && line_indent < mapping_indent) {
                  it = line_start;
                  return;
               }
            }

            // Establish mapping indent from first key (map case)
            bool established_mapping_indent_this_line = false;
            if (mapping_indent < 0) {
               mapping_indent = line_indent;
               established_mapping_indent_this_line = true;
               discovered_first_key_visual_indent = line_indent;
               if constexpr (std::is_pointer_v<std::decay_t<It>>) {
                  if (ctx.stream_begin && line_start > ctx.stream_begin) {
                     auto probe = line_start;
                     int32_t leading_ws = 0;
                     while (probe > ctx.stream_begin && (*(probe - 1) == ' ' || *(probe - 1) == '\t')) {
                        --probe;
                        ++leading_ws;
                     }
                     if (probe > ctx.stream_begin) {
                        const char prev = *(probe - 1);
                        discovered_first_key_mid_line = (prev != '\n' && prev != '\r');
                        if (!discovered_first_key_mid_line && mapping_indent == 0 && leading_ws > 0) {
                           discovered_first_key_visual_indent = leading_ws;
                        }
                     }
                     else {
                        discovered_first_key_mid_line = false;
                     }
                  }
               }
            }

            // In discovered-indent block mappings, sibling keys must stay at the same
            // indent level, except when the first key started mid-line (e.g. "--- k: v").
            if (discover_indent && mapping_indent >= 0 && !established_mapping_indent_this_line && !discovered_first_key_mid_line) {
               if (mapping_indent == 0 && discovered_first_key_visual_indent > 0) {
                  if (line_indent > discovered_first_key_visual_indent) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  if (line_indent < discovered_first_key_visual_indent) {
                     it = line_start;
                     return;
                  }
               }
               else if (line_indent > mapping_indent &&
                        (parent_indent < 0 || mapping_indent > 0 ||
                         (parent_indent >= 0 && mapping_indent == 0 && line_indent > (parent_indent + 1)))) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            // Sequence indicator check (struct case only - map parser lets key parsing
            // handle '- ' which produces appropriate errors for misindented items)
            if (!discover_indent && *it == '-' && ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n')) {
               it = line_start;
               return;
            }

            // Skip indented comment lines
            if (*it == '#') {
               skip_comment(it, end);
               first_key = false;
               continue;
            }

            // Skip blank/empty lines after indent
            if (*it == '\n' || *it == '\r') {
               first_key = false;
               continue;
            }

            // Process this mapping entry (key + colon + value)
            if (!process_entry(ctx, it, end, line_indent)) {
               return;
            }
            first_key = false;

            if (bool(ctx.error)) [[unlikely]]
               return;

            if constexpr (yaml::check_flow_context(Opts)) {
               // In flow context, an implicit "key: value" pair used as a sequence entry
               // ends at ',', ']', or '}'. Let the enclosing flow parser consume it.
               auto flow_end = it;
               skip_flow_ws_and_newlines(ctx, flow_end, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (flow_end != end && (*flow_end == ',' || *flow_end == ']' || *flow_end == '}')) {
                  it = flow_end;
                  return;
               }
            }

            // Trailing whitespace handling (peek-ahead pattern).
            // After parsing, iterator may be at leading indent of next line.
            // Don't consume it - let the outer loop handle indent measurement.
            if (it != end) {
               if (*it == ' ' || *it == '\t') {
                  auto peek = it;
                  skip_inline_ws(peek, end);
                  if (peek != end && *peek != '\n' && *peek != '\r' && *peek != '#') {
                     continue; // At leading indent of new line
                  }
               }
               skip_inline_ws(it, end);
               if (it != end && *it == '#') {
                  if constexpr (std::is_pointer_v<std::decay_t<It>>) {
                     if (it > line_start) {
                        const char prev = *(it - 1);
                        if (prev != ' ' && prev != '\t') {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                  }
               }
               skip_comment(it, end);
               if (it != end && (*it == '\n' || *it == '\r')) {
                  skip_newline(it, end);
               }
            }
         }
      }

      // Parse block mapping for struct/object types
      // key1: value1
      // key2: value2
      template <auto Opts, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_block_mapping(T&& value, Ctx& ctx, It& it, End end, int32_t mapping_indent)
      {
         using U = std::remove_cvref_t<T>;
         static constexpr auto N = reflect<U>::size;
         static constexpr auto HashInfo = hash_info<U>;

         // Clamp to >= 0 so the shared loop uses struct mode (discover_indent = false)
         if (mapping_indent < 0) mapping_indent = 0;

         parse_block_mapping_loop<Opts>(ctx, it, end, mapping_indent,
            [&](Ctx& ctx, It& it, End end, int32_t line_indent) -> bool {
               // Parse key using thread-local buffer to avoid allocation
               auto& key = string_buffer();
               key.clear();
               if (!parse_yaml_key(key, ctx, it, end, false)) {
                  return false;
               }

               skip_inline_ws(it, end);

               // Expect colon
               if (it == end || *it != ':') {
                  ctx.error = error_code::syntax_error;
                  return false;
               }
               ++it;
               skip_inline_ws(it, end);

               // Look up key
               const auto index = decode_hash_with_size<YAML, U, HashInfo, HashInfo.type>::op(
                  key.data(), key.data() + key.size(), key.size());

               const bool key_matches = index < N && std::string_view{key} == reflect<U>::keys[index];

               if (key_matches) [[likely]] {
                  visit<N>(
                     [&]<size_t I>() {
                        if (I == index) {
                           decltype(auto) member = [&]() -> decltype(auto) {
                              if constexpr (reflectable<U>) {
                                 return get<I>(to_tie(value));
                              }
                              else {
                                 return get_member(value, get<I>(reflect<U>::values));
                              }
                           }();

                           using member_type = std::decay_t<decltype(member)>;

                           // Check if value is on same line or next line
                           if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                              if (!ctx.push_indent(line_indent + 1)) [[unlikely]] return false;
                              from<YAML, member_type>::template op<Opts>(member, ctx, it, end);
                              ctx.pop_indent();
                           }
                           else {
                              int32_t nested_indent = detect_nested_value_indent(ctx, it, end, line_indent);
                              if (nested_indent >= 0) {
                                 skip_to_content(it, end);
                                 if constexpr (readable_map_t<member_type>) {
                                    if (!ctx.push_indent(nested_indent - 1)) [[unlikely]] return false;
                                 }
                                 else {
                                    if (!ctx.push_indent(nested_indent)) [[unlikely]] return false;
                                 }
                                 from<YAML, member_type>::template op<Opts>(member, ctx, it, end);
                                 ctx.pop_indent();
                              }
                           }
                        }
                        return !bool(ctx.error);
                     },
                     index);
               }
               else {
                  if constexpr (Opts.error_on_unknown_keys) {
                     ctx.error = error_code::unknown_key;
                     return false;
                  }
                  else { // else used to fix MSVC unreachable code warning
                     // Skip unknown value
                     if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                        skip_yaml_value<Opts>(ctx, it, end, line_indent, false);
                     }
                  }
               }

               return !bool(ctx.error);
            });
      }

      // Parse flow sequence into set types [item, item, ...]
      template <auto Opts, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_flow_sequence_set(T&& value, Ctx& ctx, It& it, End end)
      {
         using V = std::remove_cvref_t<T>;
         using value_type = typename V::value_type;

         if (it == end || *it != '[') [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         ++it; // Skip '['
         // Skip whitespace and newlines after opening bracket (YAML allows multi-line flow sequences)
         skip_ws_and_newlines(it, end);

         // Handle empty array
         if (it != end && *it == ']') {
            ++it;
            validate_flow_node_adjacent_tail(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            if constexpr (!yaml::check_flow_context(Opts)) {
               if (ctx.current_indent() < 0) {
                  validate_root_flow_tail_after_close(ctx, it, end);
               }
            }
            return;
         }

         bool just_saw_comma = false;
         while (it != end) {
            skip_ws_and_newlines(it, end);

            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if (*it == ']') {
               if (just_saw_comma) {
                  ++it;
                  validate_flow_node_adjacent_tail(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  if constexpr (!yaml::check_flow_context(Opts)) {
                     if (ctx.current_indent() < 0) {
                        validate_root_flow_tail_after_close(ctx, it, end);
                     }
                  }
                  return;
               }
               ctx.error = error_code::syntax_error;
               return;
            }

            if (*it == ',' || *it == '#') {
               ctx.error = error_code::syntax_error;
               return;
            }
            just_saw_comma = false;

            value_type element{};
            from<YAML, value_type>::template op<flow_context_on<Opts>()>(element, ctx, it, end);

            if (bool(ctx.error)) [[unlikely]]
               return;

            value.emplace(std::move(element));

            bool saw_line_break_before_separator = false;
            skip_flow_ws_and_newlines(ctx, it, end, &saw_line_break_before_separator);
            if (bool(ctx.error)) [[unlikely]]
               return;

            if (saw_line_break_before_separator && it != end && *it == ',') {
               ctx.error = error_code::syntax_error;
               return;
            }

            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if (*it == ']') {
               ++it;
               validate_flow_node_adjacent_tail(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if constexpr (!yaml::check_flow_context(Opts)) {
                  if (ctx.current_indent() < 0) {
                     validate_root_flow_tail_after_close(ctx, it, end);
                  }
               }
               return;
            }
            else if (*it == ',') {
               ++it;
               just_saw_comma = true;
               skip_ws_and_newlines(it, end);
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
      }

      // Parse block sequence into set types
      // - item1
      // - item2
      template <auto Opts, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_block_sequence_set(T&& value, Ctx& ctx, It& it, End end, int32_t sequence_indent)
      {
         using V = std::remove_cvref_t<T>;
         using value_type = typename V::value_type;

         while (it != end) {
            // Skip blank lines and comments, track indent
            int32_t line_indent = 0;
            auto line_start = it;

            while (it != end) {
               if (*it == '#') {
                  skip_comment(it, end);
                  skip_newline(it, end);
                  line_start = it;
                  line_indent = 0;
               }
               else if (*it == '\n' || *it == '\r') {
                  skip_newline(it, end);
                  line_start = it;
                  line_indent = 0;
               }
               else if (*it == ' ') {
                  line_start = it;
                  line_indent = measure_indent(it, end, ctx);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  if (it == end || *it == '\n' || *it == '\r') {
                     // Blank line - continue to next line
                     continue;
                  }
                  break; // Found content
               }
               else {
                  break; // At content (no leading space on this line)
               }
            }

            if (it == end) break;

            // Check for document end marker
            if (at_document_end(it, end)) break;

            // Check for dedent
            if (line_indent < sequence_indent) {
               it = line_start;
               return;
            }

            // Expect dash for sequence item
            if (*it != '-') {
               it = line_start;
               return;
            }

            ++it; // Skip '-'

            // Check for valid sequence item indicator (- followed by space or newline)
            if (it != end && !yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*it)]) {
               // Not a sequence item (could be a number like -5)
               it = line_start;
               return;
            }

            skip_inline_ws(it, end);

            value_type element{};

            // Check what follows
            if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
               // Content on same line as dash
               if (!ctx.push_indent(line_indent + 2)) [[unlikely]] return;
               from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
               ctx.pop_indent();
            }
            else {
               // Value on next line - nested block
               skip_ws_and_comment(it, end);
               skip_newline(it, end);

               // Get indent of nested content
               auto nested_start = it;
               int32_t nested_indent = measure_indent(it, end, ctx);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               it = nested_start;

               if (nested_indent > line_indent) {
                  if (!ctx.push_indent(nested_indent)) [[unlikely]] return;
                  from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
                  ctx.pop_indent();
               }
               // else: empty element (default constructed)
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            value.emplace(std::move(element));
         }
      }

   } // namespace yaml

   // Set types (std::set, std::unordered_set, etc.)
   template <class T>
      requires(readable_array_t<T> && !emplace_backable<T> && emplaceable<T>)
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         // Peek ahead to check for tag (don't consume indentation for block sequences)
         auto peek = it;
         yaml::skip_inline_ws(peek, end);

         if (peek == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(peek, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_seq(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         // After tag, check what follows
         yaml::skip_inline_ws(peek, end);

         if (peek == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts>(value, ctx, peek, end, node_props)) {
            it = peek;
            return;
         }

         if (*peek == '[') {
            // Flow sequence
            it = peek;
            yaml::parse_flow_sequence_set<Opts>(value, ctx, it, end);
         }
         else if (*peek == '-') {
            // Block sequence
            if (tag != yaml::yaml_tag::none || node_props.has_anchor) {
               it = peek;
            }
            int32_t seq_indent = ctx.current_indent();
            if (*it == '-' && ctx.current_indent() >= 0) {
               seq_indent = ctx.allow_indentless_sequence ? (ctx.current_indent() > 0 ? (ctx.current_indent() - 1) : 0)
                                                          : (ctx.current_indent() + 1);
            }
            yaml::parse_block_sequence_set<Opts>(value, ctx, it, end, seq_indent);
         }
         else {
            ctx.error = error_code::syntax_error;
         }

         yaml::finalize_node_anchor(node_props, ctx, it);
      }
   };

   // Arrays
   template <class T>
      requires(readable_array_t<T> && (emplace_backable<T> || !resizable<T>) && !emplaceable<T>)
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         // Peek ahead to check for tag (don't consume indentation for block sequences)
         auto peek = it;
         yaml::skip_inline_ws(peek, end);

         if (peek == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(peek, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_seq(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         // After tag, check what follows
         yaml::skip_inline_ws(peek, end);

         if (peek == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts>(value, ctx, peek, end, node_props)) {
            it = peek;
            return;
         }

         if (*peek == '[') {
            // Flow sequence - consume whitespace and parse
            it = peek;
            yaml::parse_flow_sequence<Opts>(value, ctx, it, end);
         }
         else if (*peek == '-') {
            // Block sequence - don't consume leading whitespace, let parse_block_sequence handle it
            // But if there was a tag, we need to advance past it
            if (tag != yaml::yaml_tag::none || node_props.has_anchor) {
               it = peek;
            }
            int32_t seq_indent = ctx.current_indent();
            if (*it == '-' && ctx.current_indent() >= 0) {
               seq_indent = ctx.allow_indentless_sequence ? (ctx.current_indent() > 0 ? (ctx.current_indent() - 1) : 0)
                                                          : (ctx.current_indent() + 1);
            }
            yaml::parse_block_sequence<Opts>(value, ctx, it, end, seq_indent);
         }
         else {
            ctx.error = error_code::syntax_error;
         }

         yaml::finalize_node_anchor(node_props, ctx, it);
      }
   };

   // Tuples (std::tuple, glaze_array_t, tuple_t)
   template <class T>
      requires(glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>)
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<T>) {
               return reflect<T>::size;
            }
            else {
               return glz::tuple_size_v<T>;
            }
         }();

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_seq(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (*it == '[') {
            // Flow sequence
            ++it; // Skip '['
            // Skip whitespace and newlines (YAML allows multi-line flow sequences)
            yaml::skip_ws_and_newlines(it, end);

            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (it != end && *it == ']') {
                  return; // Early termination
               }

               if constexpr (I != 0) {
                  if (it == end || *it != ',') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  ++it; // Skip ','
                  yaml::skip_ws_and_newlines(it, end);
               }

               if constexpr (is_std_tuple<T>) {
                  using element_t = std::tuple_element_t<I, std::remove_cvref_t<T>>;
                  from<YAML, element_t>::template op<yaml::flow_context_on<Opts>()>(std::get<I>(value), ctx, it, end);
               }
               else if constexpr (glaze_array_t<T>) {
                  using element_t = std::decay_t<decltype(get_member(value, glz::get<I>(meta_v<T>)))>;
                  from<YAML, element_t>::template op<yaml::flow_context_on<Opts>()>(
                     get_member(value, glz::get<I>(meta_v<T>)), ctx, it, end);
               }
               else {
                  using element_t = std::decay_t<decltype(glz::get<I>(value))>;
                  from<YAML, element_t>::template op<yaml::flow_context_on<Opts>()>(glz::get<I>(value), ctx, it, end);
               }

               yaml::skip_ws_and_newlines(it, end);
            });

            if (bool(ctx.error)) [[unlikely]]
               return;

            if (it == end || *it != ']') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it; // Skip ']'
         }
         else if (*it == '-') {
            // Block sequence
            size_t index = 0;

            while (it != end && index < N) {
               // Skip whitespace and measure indent
               auto line_start = it;
               yaml::skip_inline_ws(it, end);

               if (it == end) break;

               // Skip blank lines
               if (*it == '\n' || *it == '\r') {
                  yaml::skip_newline(it, end);
                  continue;
               }

               if (*it != '-') {
                  it = line_start;
                  break;
               }

               ++it; // Skip '-'

               // Check valid sequence indicator
               if (it != end && !yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*it)]) {
                  it = line_start;
                  break;
               }

               yaml::skip_inline_ws(it, end);

               // Parse element at current index
               [&]<size_t... Is>(std::index_sequence<Is...>) {
                  (
                     [&]<size_t I>() {
                        if (I == index) {
                           if constexpr (is_std_tuple<T>) {
                              using element_t = std::tuple_element_t<I, std::remove_cvref_t<T>>;
                              from<YAML, element_t>::template op<Opts>(std::get<I>(value), ctx, it, end);
                           }
                           else if constexpr (glaze_array_t<T>) {
                              using element_t = std::decay_t<decltype(get_member(value, glz::get<I>(meta_v<T>)))>;
                              from<YAML, element_t>::template op<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx,
                                                                       it, end);
                           }
                           else {
                              using element_t = std::decay_t<decltype(glz::get<I>(value))>;
                              from<YAML, element_t>::template op<Opts>(glz::get<I>(value), ctx, it, end);
                           }
                        }
                     }.template operator()<Is>(),
                     ...);
               }(std::make_index_sequence<N>{});

               if (bool(ctx.error)) [[unlikely]]
                  return;

               ++index;

               // Skip to next line
               yaml::skip_ws_and_comment(it, end);
               if (it != end && (*it == '\n' || *it == '\r')) {
                  yaml::skip_newline(it, end);
               }
            }
         }
         else {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Pairs (std::pair)
   template <pair_t T>
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag - pairs are treated as single-entry mappings
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_map(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         using first_type = typename std::remove_cvref_t<T>::first_type;
         using second_type = typename std::remove_cvref_t<T>::second_type;

         if (*it == '{') {
            // Flow mapping style: {key: value}
            ++it; // Skip '{'
            yaml::skip_inline_ws(it, end);

            if (it != end && *it == '}') {
               ++it;
               return; // Empty pair
            }

            // Parse key
            if constexpr (str_t<first_type>) {
               // Skip anchor on key
               if (*it == '&') {
                  ++it;
                  yaml::parse_anchor_name(it, end);
                  yaml::skip_inline_ws(it, end);
                  if (it == end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
               }
               std::string key_str;
               const auto style = yaml::detect_scalar_style(*it);
               if (style == yaml::scalar_style::double_quoted) {
                  yaml::parse_double_quoted_string(key_str, ctx, it, end);
               }
               else if (style == yaml::scalar_style::single_quoted) {
                  yaml::parse_single_quoted_string(key_str, ctx, it, end);
               }
               else {
                  yaml::parse_plain_scalar(key_str, ctx, it, end, true);
               }
               if (bool(ctx.error)) [[unlikely]]
                  return;
               value.first = std::move(key_str);
            }
            else {
               from<YAML, first_type>::template op<yaml::flow_context_on<Opts>()>(value.first, ctx, it, end);
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            yaml::skip_inline_ws(it, end);

            // Expect colon
            if (it == end || *it != ':') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
            yaml::skip_inline_ws(it, end);

            // Parse value
            from<YAML, second_type>::template op<yaml::flow_context_on<Opts>()>(value.second, ctx, it, end);

            if (bool(ctx.error)) [[unlikely]]
               return;

            yaml::skip_inline_ws(it, end);

            if (it == end || *it != '}') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it; // Skip '}'
         }
         else {
            // Block mapping style: key: value
            // Parse key
            if constexpr (str_t<first_type>) {
               std::string key_str;
               if (!yaml::parse_yaml_key(key_str, ctx, it, end, false)) {
                  return;
               }
               value.first = std::move(key_str);
            }
            else {
               from<YAML, first_type>::template op<Opts>(value.first, ctx, it, end);
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            yaml::skip_inline_ws(it, end);

            // Expect colon
            if (it == end || *it != ':') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
            yaml::skip_inline_ws(it, end);

            // Parse value
            from<YAML, second_type>::template op<Opts>(value.second, ctx, it, end);
         }
      }
   };

   // Objects (glaze_object_t and reflectable)
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_read<T>)
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) {
            return; // Empty input - keep default values
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_map(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) {
            return; // Empty input - keep default values
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts, yaml::node_property_policy{true, true, true, false}>(
                value, ctx, it, end, node_props))
            return;

         if (*it == '{') {
            // Flow mapping
            yaml::parse_flow_mapping<Opts>(value, ctx, it, end);
         }
         else {
            // Block mapping - use indent from context (set by parent parser)
            yaml::parse_block_mapping<Opts>(value, ctx, it, end, ctx.current_indent());
         }

         yaml::finalize_node_anchor(node_props, ctx, it);
      }
   };

   // Maps (std::map, std::unordered_map, etc.)
   template <readable_map_t T>
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Check for tag
         const auto tag = yaml::parse_yaml_tag(it, end);
         if (tag == yaml::yaml_tag::unknown) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (!yaml::tag_valid_for_map(tag)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         yaml::node_property_state node_props{};
         if (yaml::parse_node_properties<Opts, yaml::node_property_policy{true, true, false, false}>(
                value, ctx, it, end, node_props))
            return;

         using key_t = typename std::remove_cvref_t<T>::key_type;
         using val_t = typename std::remove_cvref_t<T>::mapped_type;

         if (*it == '{') {
            // Flow mapping
            ++it;
            // Skip whitespace and newlines after opening brace
            yaml::skip_ws_and_newlines(it, end);

            if (it != end && *it == '}') {
               ++it;
               yaml::validate_flow_node_adjacent_tail(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               yaml::finalize_node_anchor(node_props, ctx, it);
               return;
            }

            while (it != end) {
               // Skip whitespace and newlines at start of each iteration
               yaml::skip_ws_and_newlines(it, end);

               if (it != end && *it == '}') {
                  ++it;
                  yaml::validate_flow_node_adjacent_tail(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  break;
               }

               bool explicit_flow_key = false;
               auto explicit_probe = it;
               yaml::skip_inline_ws(explicit_probe, end);
               if (explicit_probe != end && *explicit_probe == '?') {
                  const auto after_q = explicit_probe + 1;
                  if (after_q == end || yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*after_q)] ||
                      *after_q == ',' || *after_q == '}') {
                     explicit_flow_key = true;
                     it = explicit_probe + 1;
                     yaml::skip_flow_ws_and_newlines(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
               }
               bool key_allows_linebreak_before_colon = explicit_flow_key;

               // Parse key
               key_t key{};
               if constexpr (std::same_as<std::remove_cvref_t<key_t>, std::string>) {
                  if (explicit_flow_key && it != end && (*it == ':' || *it == ',' || *it == '}')) {
                     key.clear();
                  }
                  else {
                     auto key_probe = it;
                     yaml::skip_inline_ws(key_probe, end);

                     // Use full node parsing only for structurally complex flow keys.
                     // Plain/quoted/alias keys stay on parse_yaml_key() for compatibility.
                     bool parse_complex_flow_key = false;
                     if (key_probe != end) {
                        if (*key_probe == '"' || *key_probe == '\'') {
                           key_allows_linebreak_before_colon = true;
                        }
                        if (*key_probe == '[' || *key_probe == '{') {
                           parse_complex_flow_key = true;
                           key_allows_linebreak_before_colon = true;
                        }
                        else if (*key_probe == '&') {
                           ++key_probe;
                           yaml::parse_anchor_name(key_probe, end);
                           yaml::skip_inline_ws(key_probe, end);
                           if (key_probe != end && (*key_probe == '[' || *key_probe == '{')) {
                              parse_complex_flow_key = true;
                              key_allows_linebreak_before_colon = true;
                           }
                        }
                     }

                     if (parse_complex_flow_key) {
                        glz::generic key_node{};
                        from<YAML, glz::generic>::template op<yaml::flow_context_on<Opts>()>(key_node, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        if (key_node.is_null()) {
                           key.clear();
                        }
                        else if (auto* s = key_node.template get_if<std::string>()) {
                           key = *s;
                        }
                        else {
                           std::string key_json;
                           (void)glz::write_json(key_node, key_json);
                           key = std::move(key_json);
                        }
                     }
                     else {
                        if (!yaml::parse_yaml_key(key, ctx, it, end, true)) {
                           return;
                        }
                     }
                  }
               }
               else {
                  if (explicit_flow_key && it != end && (*it == ':' || *it == ',' || *it == '}')) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  from<YAML, key_t>::template op<yaml::flow_context_on<Opts>()>(key, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               // Separation between flow key and ':' may include comments/newlines.
               // A bare line break without a comment between key and ':' is invalid.
               bool saw_key_comment = false;
               bool saw_key_linebreak = false;
               while (it != end) {
                  yaml::skip_inline_ws(it, end);
                  if (it != end && *it == '#') {
                     yaml::skip_comment(it, end);
                     saw_key_comment = true;
                  }
                  if (it != end && (*it == '\n' || *it == '\r')) {
                     if (!saw_key_comment) {
                        if (!key_allows_linebreak_before_colon) {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                        // Without an intervening comment, only allow line-break
                        // separation if the next line is an indented ':' marker.
                        auto look = it;
                        yaml::skip_newline(look, end);
                        int indent = 0;
                        while (look != end && (*look == ' ' || *look == '\t')) {
                           ++indent;
                           ++look;
                        }
                        if (look == end || *look != ':' || indent == 0) {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                     saw_key_linebreak = true;
                     yaml::skip_newline(it, end);
                     continue;
                  }
                  break;
               }

               auto parse_implicit_null = [&](val_t& val) -> bool {
                  if constexpr (nullable_like<std::remove_cvref_t<val_t>>) {
                     val = {};
                     return true;
                  }
                  else if constexpr (std::same_as<std::remove_cvref_t<val_t>, glz::generic>) {
                     val = glz::generic{};
                     return true;
                  }
                  else {
                     static constexpr std::string_view null_value{"null"};
                     auto null_it = null_value.data();
                     from<YAML, val_t>::template op<yaml::flow_context_on<Opts>()>(
                        val, ctx, null_it, null_value.data() + null_value.size());
                     return !bool(ctx.error);
                  }
               };

               if (it != end && (*it == ',' || *it == '}')) {
                  val_t val{};
                  if (!parse_implicit_null(val)) [[unlikely]]
                     return;

                  value.emplace(std::move(key), std::move(val));

                  if (*it == '}') {
                     ++it;
                     yaml::validate_flow_node_adjacent_tail(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     break;
                  }

                  ++it; // skip comma
                  yaml::skip_ws_and_newlines(it, end);
                  continue;
               }

               if (it == end || *it != ':') {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               // If the key/value indicator was reached only after a line break,
               // require an explicit separator after ':' (spaces, line break, comment,
               // omitted value delimiter, or end). Adjacent values like ":bar" are
               // malformed in this layout.
               if (saw_key_linebreak && it != end &&
                   !yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*it)] && *it != '#' && *it != ',' &&
                   *it != '}') {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               yaml::skip_flow_ws_and_newlines(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               // Omitted value after an explicit ':' maps to an implicit null.
               if (it != end && (*it == ',' || *it == '}')) {
                  val_t val{};
                  if (!parse_implicit_null(val)) [[unlikely]]
                     return;

                  value.emplace(std::move(key), std::move(val));

                  if (*it == '}') {
                     ++it;
                     yaml::validate_flow_node_adjacent_tail(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     break;
                  }

                  ++it; // skip comma
                  yaml::skip_ws_and_newlines(it, end);
                  continue;
               }

               // Parse value
               val_t val{};
               from<YAML, val_t>::template op<yaml::flow_context_on<Opts>()>(val, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               value.emplace(std::move(key), std::move(val));

               yaml::skip_inline_ws(it, end);

               if (it != end && *it == '}') {
                  ++it;
                  yaml::validate_flow_node_adjacent_tail(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  break;
               }
               else if (it != end && *it == ',') {
                  ++it;
                  // Skip whitespace and newlines after comma
                  yaml::skip_ws_and_newlines(it, end);
               }
               else if (it != end && (*it == '\n' || *it == '\r')) {
                  // Newlines may precede either the closing brace or the
                  // separator comma for the next entry.
                  yaml::skip_ws_and_newlines(it, end);
                  if (it != end && *it == '}') {
                     ++it;
                     yaml::validate_flow_node_adjacent_tail(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     break;
                  }
                  if (it != end && *it == ',') {
                     ++it;
                     yaml::skip_ws_and_newlines(it, end);
                     continue;
                  }
                  ctx.error = error_code::syntax_error;
                  return;
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
         else {
            // Block mapping - use shared loop with map-specific callback
            yaml::parse_block_mapping_loop<Opts>(ctx, it, end, int32_t(-1),
               [&](auto& ctx, auto& it, auto end, int32_t line_indent) -> bool {
                  auto parse_map_value = [&](val_t& val) -> bool {
                     if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                        // Inline mapping values in block context cannot start with a
                        // plain "key: value" pair on the same line (e.g. "a: b: c").
                        // Such constructs are malformed unless wrapped in flow
                        // delimiters ({...} / [...]) or quoted.
                        if constexpr (std::is_pointer_v<std::decay_t<decltype(it)>>) {
                           if (ctx.stream_begin) {
                              const auto begin = static_cast<std::remove_cvref_t<decltype(it)>>(ctx.stream_begin);
                              auto is_line_content_start = [&](auto pos) {
                                 auto line = pos;
                                 while (line != begin) {
                                    auto prev = line - 1;
                                    if (*prev == '\n' || *prev == '\r') break;
                                    line = prev;
                                 }
                                 for (auto p = line; p != pos; ++p) {
                                    if (*p != ' ' && *p != '\t') return false;
                                 }
                                 return true;
                              };

                              auto line_has_explicit_value_indicator = [&](auto pos) {
                                 auto line = pos;
                                 while (line != begin) {
                                    auto prev = line - 1;
                                    if (*prev == '\n' || *prev == '\r') break;
                                    line = prev;
                                 }
                                 while (line != end && (*line == ' ' || *line == '\t')) ++line;
                                 return line != end && *line == ':';
                              };

                              if (!is_line_content_start(it) && yaml::inline_value_has_plain_mapping_indicator(it, end) &&
                                  !line_has_explicit_value_indicator(it)) {
                                 ctx.error = error_code::syntax_error;
                                 return false;
                              }
                           }
                        }

                        if (!ctx.push_indent(line_indent + 1)) [[unlikely]] return false;
                        from<YAML, val_t>::template op<Opts>(val, ctx, it, end);
                        ctx.pop_indent();
                     }
                     else {
                        int32_t nested_indent = yaml::detect_nested_value_indent(ctx, it, end, line_indent);
                        if (nested_indent >= 0) {
                           yaml::skip_to_content(it, end);
                           if (it != end && *it == ':' && (it + 1) != end && *(it + 1) == '\t') {
                              ctx.error = error_code::syntax_error;
                              return false;
                           }
                           if (!ctx.push_indent(nested_indent - 1)) [[unlikely]] return false;
                           from<YAML, val_t>::template op<Opts>(val, ctx, it, end);
                           ctx.pop_indent();
                        }
                     }
                     return !bool(ctx.error);
                  };

                  // Explicit key entry form:
                  // ? key
                  // : value
                  if (*it == '?' && ((it + 1) != end) && *(it + 1) == '\t') {
                     ctx.error = error_code::syntax_error;
                     return false;
                  }
                  if (*it == '?' && ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\n' || *(it + 1) == '\r')) {
                     ++it; // skip '?'
                     yaml::skip_inline_ws(it, end);

                     key_t key{};
                     if constexpr (std::same_as<std::remove_cvref_t<key_t>, std::string>) {
                        auto to_string_key = [&](glz::generic& key_node) {
                           if (key_node.is_null()) {
                              key.clear();
                           }
                           else if (auto* s = key_node.template get_if<std::string>()) {
                              key = *s;
                           }
                           else {
                              std::string key_json;
                              (void)glz::write_json(key_node, key_json);
                              key = std::move(key_json);
                           }
                        };

                        auto key_it = it;
                        yaml::skip_inline_ws(key_it, end);

                        auto content = key_it;
                        yaml::skip_to_content(content, end);
                        bool handled_anchor_only_empty_key = false;

                        // Anchor on an empty explicit key node:
                        //   ? &a
                        //   : value
                        if (content != end && *content == '&') {
                           auto anchor_probe = content + 1;
                           auto anchor_name = yaml::parse_anchor_name(anchor_probe, end);
                           if (!anchor_name.empty()) {
                              auto after_anchor = anchor_probe;
                              yaml::skip_inline_ws(after_anchor, end);

                              auto value_indicator = after_anchor;
                              if (value_indicator != end && *value_indicator == ':') {
                                 key.clear();
                                 ctx.anchors[std::string(anchor_name)] = {content, content, ctx.current_indent()};
                                 it = value_indicator;
                                 handled_anchor_only_empty_key = true;
                              }
                              else {
                                 if (value_indicator != end && *value_indicator == '#') {
                                    yaml::skip_comment(value_indicator, end);
                                 }
                                 if (value_indicator != end && (*value_indicator == '\n' || *value_indicator == '\r')) {
                                    yaml::skip_newline(value_indicator, end);
                                    auto value_line = value_indicator;
                                    int32_t value_indent = yaml::measure_indent(value_line, end, ctx);
                                    if (bool(ctx.error)) [[unlikely]]
                                       return false;
                                    if (value_line != end && *value_line == ':' && value_indent >= line_indent) {
                                       key.clear();
                                       ctx.anchors[std::string(anchor_name)] = {content, content, ctx.current_indent()};
                                       it = value_line;
                                       handled_anchor_only_empty_key = true;
                                    }
                                 }
                              }
                           }
                        }

                        if (handled_anchor_only_empty_key) {
                           // 'it' already points to the explicit value indicator ':'
                        }
                        else if (content == end || *content == ':') {
                           key.clear();
                           it = content;
                        }
                        else {
                           const bool complex_explicit_key =
                              (*content == '[' || *content == '{' || *content == '-' || *content == '?' ||
                               *content == '|' || *content == '>' || *content == '&' || *content == '!' ||
                               *content == '*' || *content == '"' || *content == '\'');

                           if (complex_explicit_key || content != key_it) {
                              auto key_node_it = content;
                              glz::generic key_node{};
                              if (*content == '[' || *content == '{') {
                                 from<YAML, glz::generic>::template op<yaml::flow_context_on<Opts>()>(key_node, ctx,
                                                                                                       key_node_it, end);
                              }
                              else {
                                 from<YAML, glz::generic>::template op<Opts>(key_node, ctx, key_node_it, end);
                              }
                              if (bool(ctx.error)) [[unlikely]]
                                 return false;
                              it = key_node_it;
                              to_string_key(key_node);
                           }
                           else {
                              const bool prev_explicit_mapping_key_context = ctx.explicit_mapping_key_context;
                              ctx.explicit_mapping_key_context = true;
                              const bool ok = yaml::parse_yaml_key(key, ctx, it, end, true);
                              ctx.explicit_mapping_key_context = prev_explicit_mapping_key_context;
                              if (!ok) {
                                 return false;
                              }
                           }
                        }
                     }
                     else {
                        if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                           from<YAML, key_t>::template op<Opts>(key, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return false;
                        }
                     }

                     yaml::skip_inline_ws(it, end);
                     yaml::skip_comment(it, end);

                     // Value indicator may appear on the next line at the same indent.
                     if (it != end && (*it == '\n' || *it == '\r')) {
                        auto probe = it;
                        yaml::skip_newline(probe, end);
                        auto value_line = probe;
                        int32_t value_indent = yaml::measure_indent(value_line, end, ctx);
                        if (bool(ctx.error)) [[unlikely]]
                           return false;
                        if (value_line != end && *value_line == ':' && value_indent == line_indent) {
                           yaml::skip_newline(it, end);
                           yaml::skip_inline_ws(it, end);
                        }
                     }

                     val_t val{};
                     if (it != end && *it == ':') {
                        ++it;
                        if (it != end && *it == '\t') {
                           ctx.error = error_code::syntax_error;
                           return false;
                        }
                        if (it != end && (*it == '\n' || *it == '\r')) {
                           auto probe = it;
                           yaml::skip_newline(probe, end);
                           while (probe != end && *probe == ' ') ++probe;
                           if (probe != end && *probe == ':' && ((probe + 1) != end) && *(probe + 1) == '\t') {
                              ctx.error = error_code::syntax_error;
                              return false;
                           }
                        }
                        yaml::skip_inline_ws(it, end);
                        if (!parse_map_value(val)) [[unlikely]]
                           return false;
                     }

                     value.emplace(std::move(key), std::move(val));
                     return true;
                  }

                  key_t key{};
                  if constexpr (std::same_as<std::remove_cvref_t<key_t>, std::string>) {
                     auto key_probe = it;
                     yaml::skip_inline_ws(key_probe, end);
                     const bool complex_flow_key = (key_probe != end && (*key_probe == '[' || *key_probe == '{'));
                     if (complex_flow_key) {
                        glz::generic key_node{};
                        from<YAML, glz::generic>::template op<yaml::flow_context_on<Opts>()>(key_node, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return false;
                        if (key_node.is_null()) {
                           key.clear();
                        }
                        else if (auto* s = key_node.template get_if<std::string>()) {
                           key = *s;
                        }
                        else {
                           std::string key_json;
                           (void)glz::write_json(key_node, key_json);
                           key = std::move(key_json);
                        }
                     }
                     else {
                        from<YAML, key_t>::template op<Opts>(key, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return false;
                     }
                  }
                  else {
                     from<YAML, key_t>::template op<Opts>(key, ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return false;
                  }

                  yaml::skip_inline_ws(it, end);

                  if (it == end || *it != ':') {
                     ctx.error = error_code::syntax_error;
                     return false;
                  }
                  ++it;
                  yaml::skip_inline_ws(it, end);

                  val_t val{};
                  if (!parse_map_value(val)) [[unlikely]]
                     return false;

                  value.emplace(std::move(key), std::move(val));
                  return true;
               });
         }

         yaml::finalize_node_anchor(node_props, ctx, it);
      }
   };

   // ============================================================================
   // YAML Variant type category traits
   // ============================================================================

   template <class T>
   concept yaml_variant_bool_type = bool_t<remove_meta_wrapper_t<T>>;

   template <class T>
   concept yaml_variant_num_type = num_t<remove_meta_wrapper_t<T>>;

   template <class T>
   concept yaml_variant_str_type = str_t<remove_meta_wrapper_t<T>>;

   template <class T>
   concept yaml_variant_object_type =
      glaze_object_t<T> || reflectable<T> || writable_map_t<T> || readable_map_t<T> || is_memory_object<T>;

   template <class T>
   concept yaml_variant_array_type =
      array_t<remove_meta_wrapper_t<T>> || glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>;

   template <class T>
   concept yaml_variant_null_type = null_t<T>;

   // Type traits wrapping concepts for template template parameter use
   template <class T>
   struct is_yaml_variant_bool : std::bool_constant<yaml_variant_bool_type<T>>
   {};
   template <class T>
   struct is_yaml_variant_num : std::bool_constant<yaml_variant_num_type<T>>
   {};
   template <class T>
   struct is_yaml_variant_str : std::bool_constant<yaml_variant_str_type<T>>
   {};
   template <class T>
   struct is_yaml_variant_object : std::bool_constant<yaml_variant_object_type<T>>
   {};
   template <class T>
   struct is_yaml_variant_array : std::bool_constant<yaml_variant_array_type<T>>
   {};
   template <class T>
   struct is_yaml_variant_null : std::bool_constant<yaml_variant_null_type<T>>
   {};

   // Count types in variant matching a trait
   template <class Variant, template <class> class Trait>
   struct yaml_variant_count_impl;

   template <template <class> class Trait, class... Ts>
   struct yaml_variant_count_impl<std::variant<Ts...>, Trait>
   {
      static constexpr size_t value = (size_t(Trait<Ts>::value) + ... + 0);
   };

   template <class Variant, template <class> class Trait>
   constexpr size_t yaml_variant_count_v = yaml_variant_count_impl<Variant, Trait>::value;

   // Get first index matching trait (or variant_npos if none)
   template <class Variant, template <class> class Trait>
   struct yaml_variant_first_index_impl;

   template <template <class> class Trait, class... Ts>
   struct yaml_variant_first_index_impl<std::variant<Ts...>, Trait>
   {
      static constexpr size_t find()
      {
         size_t result = std::variant_npos;
         size_t idx = 0;
         ((Trait<Ts>::value && result == std::variant_npos ? (result = idx, ++idx) : ++idx), ...);
         return result;
      }
      static constexpr size_t value = find();
   };

   template <class Variant, template <class> class Trait>
   constexpr size_t yaml_variant_first_index_v = yaml_variant_first_index_impl<Variant, Trait>::value;

   // Precomputed type counts for a variant (similar to JSON's variant_type_count)
   template <class T>
   struct yaml_variant_type_count
   {
      static constexpr auto n_bool = yaml_variant_count_v<T, is_yaml_variant_bool>;
      static constexpr auto n_num = yaml_variant_count_v<T, is_yaml_variant_num>;
      static constexpr auto n_str = yaml_variant_count_v<T, is_yaml_variant_str>;
      static constexpr auto n_object = yaml_variant_count_v<T, is_yaml_variant_object>;
      static constexpr auto n_array = yaml_variant_count_v<T, is_yaml_variant_array>;
      static constexpr auto n_null = yaml_variant_count_v<T, is_yaml_variant_null>;
   };

   // Check if variant is auto-deducible based on type composition
   template <is_variant T>
   consteval auto yaml_variant_is_auto_deducible()
   {
      using counts = yaml_variant_type_count<T>;
      return counts::n_bool < 2 && counts::n_num < 2 && counts::n_str < 2 && counts::n_object < 2 &&
             counts::n_array < 2 && counts::n_null < 2;
   }

   // Process YAML variant alternatives by iterating directly over variant indices
   template <class Variant, template <class> class Trait>
   struct process_yaml_variant_alternatives
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         constexpr auto category_count = yaml_variant_count_v<Variant, Trait>;

         if constexpr (category_count == 0) {
            ctx.error = error_code::no_matching_variant_type;
         }
         else if constexpr (category_count == 1) {
            // Only one type in this category, use it directly
            constexpr auto first_idx = yaml_variant_first_index_v<Variant, Trait>;
            using V = std::variant_alternative_t<first_idx, Variant>;
            if (!std::holds_alternative<V>(value)) value = V{};
            from<YAML, V>::template op<Opts>(std::get<V>(value), ctx, it, end);
         }
         else {
            // Multiple types in this category, try each one
            constexpr auto N = std::variant_size_v<Variant>;
            bool found_match{};
            size_t match_idx = 0;

            for_each<N>([&]<size_t I>() {
               if (found_match) {
                  return;
               }
               using V = std::variant_alternative_t<I, Variant>;
               if constexpr (Trait<V>::value) {
                  auto copy_it = it;
                  if (!std::holds_alternative<V>(value)) {
                     value = V{};
                  }

                  yaml::yaml_context temp_ctx{};
                  temp_ctx.anchors = ctx.anchors;
                  temp_ctx.stream_begin = ctx.stream_begin;
                  from<YAML, V>::template op<Opts>(std::get<V>(value), temp_ctx, it, end);

                  if (!bool(temp_ctx.error)) {
                     found_match = true;
                     ctx.anchors = std::move(temp_ctx.anchors);
                  }
                  else {
                     it = copy_it;
                     if (match_idx + 1 < category_count) {
                        // Not the last type, continue trying
                     }
                     else {
                        // Last type failed, propagate error
                        ctx.error = temp_ctx.error;
                     }
                  }
                  ++match_idx;
               }
            });

            if (!found_match && !bool(ctx.error)) {
               ctx.error = error_code::no_matching_variant_type;
            }
         }
      }
   };

   // Quick check if current line contains a colon that could indicate a block mapping key.
   // Only scans to end of line (bounded by newline), so O(line length) not O(input).
   // Returns false for obvious non-mappings to avoid expensive full parse attempts.
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool line_could_be_block_mapping(It it, End end)
   {
      bool prev_was_whitespace = true; // Start of value acts like after whitespace
      int flow_depth = 0;
      while (it != end) {
         const char c = *it;
         if (c == '\n' || c == '\r') {
            return false;
         }
         if (c == ':' && flow_depth == 0) {
            ++it;
            // Colon followed by space, newline, or end indicates a mapping key
            if (it == end || *it == ' ' || *it == '\t' || *it == '\n' || *it == '\r') {
               return true;
            }
            // Otherwise this ':' is part of plain content (e.g., "::", "http://").
            // Continue scanning for a later mapping separator on the same line.
            prev_was_whitespace = false;
            continue;
         }
         // Per YAML spec: # only starts a comment when preceded by whitespace
         // Stop scanning if we hit a comment - any colon after is not a key indicator
         if (c == '#' && flow_depth == 0 && prev_was_whitespace) {
            return false;
         }
         // Skip over quoted strings only when they start a quoted token.
         // Quote characters are otherwise valid in plain scalars/keys.
         if ((c == '"' || c == '\'') && prev_was_whitespace) {
            const char quote = c;
            ++it;
            while (it != end && *it != quote) {
               if (*it == '\\' && quote == '"') {
                  ++it; // Skip escape character
                  if (it != end) ++it; // Skip escaped character
               }
               else if (*it == '\n' || *it == '\r') {
                  // Unterminated quote on this line
                  return false;
               }
               else {
                  ++it;
               }
            }
            if (it != end) ++it; // Skip closing quote
            prev_was_whitespace = false;
            continue;
         }
         if (c == '[' || c == '{') {
            ++flow_depth;
            prev_was_whitespace = false;
            ++it;
            continue;
         }
         if ((c == ']' || c == '}') && flow_depth > 0) {
            --flow_depth;
            prev_was_whitespace = false;
            ++it;
            continue;
         }
         prev_was_whitespace = (c == ' ' || c == '\t');
         ++it;
      }
      return false;
   }

   // Quick check for implicit single-pair flow mappings used as sequence entries
   // (e.g. `"k":v`, `{k: v}:v`, `k: v`) up to the next top-level flow delimiter.
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool line_could_be_flow_mapping(It it, End end)
   {
      int flow_depth = 0;
      bool prev_was_whitespace = true;
      bool key_supports_adjacent_value = false;

      while (it != end) {
         const char c = *it;
         if (c == '\n' || c == '\r') {
            return false;
         }
         if (flow_depth == 0 && (c == ',' || c == ']' || c == '}')) {
            return false;
         }
         if (c == '#' && flow_depth == 0 && prev_was_whitespace) {
            return false;
         }
         if ((c == '"' || c == '\'') && prev_was_whitespace) {
            const char quote = c;
            ++it;
            while (it != end && *it != quote) {
               if (*it == '\\' && quote == '"') {
                  ++it;
                  if (it != end) ++it;
               }
               else if (*it == '\n' || *it == '\r') {
                  return false;
               }
               else {
                  ++it;
               }
            }
            if (it != end) ++it;
            key_supports_adjacent_value = true;
            prev_was_whitespace = false;
            continue;
         }
         if (c == '[' || c == '{') {
            ++flow_depth;
            key_supports_adjacent_value = false;
            prev_was_whitespace = false;
            ++it;
            continue;
         }
         if (c == ']' || c == '}') {
            if (flow_depth > 0) {
               --flow_depth;
               if (flow_depth == 0) {
                  key_supports_adjacent_value = true;
               }
               prev_was_whitespace = false;
               ++it;
               continue;
            }
            return false;
         }
         if (c == ':' && flow_depth == 0) {
            const auto next = it + 1;
            if (next == end || *next == ' ' || *next == '\t' || *next == '\n' || *next == '\r') {
               return true;
            }
            if (key_supports_adjacent_value) {
               return true;
            }
            return false;
         }

         key_supports_adjacent_value = false;
         prev_was_whitespace = (c == ' ' || c == '\t');
         ++it;
      }

      return false;
   }

   // Speculatively try parsing as a block mapping into a variant.
   // Returns true if successful, false otherwise (restoring iterator on failure).
   // Uses quick line-based lookahead to short-circuit obvious non-mappings,
   // then falls back to full parse attempt for potential mappings.
   // If the line looks like a block mapping (has "key: " pattern) but parsing
   // fails with a syntax error, that error is propagated (not silently caught)
   // to avoid hiding malformed content.
   template <class Variant, auto Opts>
   GLZ_ALWAYS_INLINE bool try_parse_block_mapping_into_variant(auto&& value, auto&& ctx, auto&& it, auto end)
   {
      using counts = yaml_variant_type_count<Variant>;
      if constexpr (counts::n_object == 0) {
         return false;
      }
      else {
         // Quick check: if no viable mapping separator in the current span,
         // avoid expensive speculative parse.
         const bool could_be_mapping = [&] {
            if constexpr (yaml::check_flow_context(Opts)) {
               return line_could_be_flow_mapping(it, end);
            }
            else {
               return line_could_be_block_mapping(it, end);
            }
         }();
         if (!could_be_mapping) {
            return false;
         }
         // Full parse attempt - use a temporary context that inherits indent from parent
         yaml::yaml_context temp_ctx{};
         temp_ctx.indent_stack = ctx.indent_stack; // Propagate indent context for nested parsing
         temp_ctx.anchors = ctx.anchors;
         temp_ctx.stream_begin = ctx.stream_begin;
         process_yaml_variant_alternatives<Variant, is_yaml_variant_object>::template op<Opts>(value, temp_ctx, it,
                                                                                               end);
         if (!bool(temp_ctx.error)) {
            ctx.anchors = std::move(temp_ctx.anchors);
            return true;
         }
         // The line matched "key: value" pattern but parsing failed.
         // This indicates malformed content (e.g., unclosed flow collection),
         // so propagate the error rather than silently falling back to string.
         ctx.error = temp_ctx.error;
         return false;
      }
   }

   // Try block mapping first, then fall back to string.
   // Common pattern in YAML variant parsing for ambiguous scalars.
   template <class Variant, auto Opts>
   GLZ_ALWAYS_INLINE void parse_block_mapping_or_string(auto&& value, auto&& ctx, auto&& it, auto end)
   {
         if constexpr (yaml::check_flow_context(Opts)) {
         // In flow context, only treat plain content as an implicit "key: value"
         // when a mapping separator appears before the next top-level flow delimiter.
         auto could_be_implicit_flow_pair = [&](auto pos) {
            int depth = 0;
            while (pos != end) {
               const char c = *pos;
               if (c == '"' || c == '\'') {
                  const char quote = c;
                  ++pos;
                  while (pos != end && *pos != quote) {
                     if (*pos == '\\' && quote == '"') {
                        ++pos;
                        if (pos != end) ++pos;
                     }
                     else {
                        ++pos;
                     }
                  }
                  if (pos != end) ++pos;
                  continue;
               }
               if (c == '[' || c == '{') {
                  ++depth;
                  ++pos;
                  continue;
               }
               if (c == ']' || c == '}') {
                  if (depth == 0) return false;
                  --depth;
                  ++pos;
                  continue;
               }
               if (c == ',' && depth == 0) return false;
               if (c == ':' && depth == 0) {
                  auto next = pos + 1;
                  return next == end || *next == ' ' || *next == '\t' || *next == '\n' || *next == '\r';
               }
               ++pos;
            }
            return false;
         };

         if (could_be_implicit_flow_pair(it) || line_could_be_flow_mapping(it, end)) {
            if (try_parse_block_mapping_into_variant<Variant, Opts>(value, ctx, it, end)) {
               return;
            }
            if (bool(ctx.error)) {
               return;
            }
         }

         process_yaml_variant_alternatives<Variant, is_yaml_variant_str>::template op<Opts>(value, ctx, it, end);
         return;
      }

      if (try_parse_block_mapping_into_variant<Variant, Opts>(value, ctx, it, end)) {
         return;
      }
      // If an error was set (e.g., malformed flow collection in value), don't try to parse as string
      if (bool(ctx.error)) {
         return;
      }
      process_yaml_variant_alternatives<Variant, is_yaml_variant_str>::template op<Opts>(value, ctx, it, end);
   }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable code from if constexpr
#endif
   // Variant support
   template <is_variant T>
   struct from<YAML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]]
            return;

         // At root level (indent == -1), skip leading whitespace, newlines, and comments
         // For nested values, only skip inline whitespace to preserve block structure
         if (ctx.current_indent() < 0) {
            yaml::skip_ws_newlines_comments(it, end);
         }
         else {
            yaml::skip_inline_ws(it, end);
         }

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if constexpr (yaml_variant_is_auto_deducible<std::remove_cvref_t<T>>()) {
            using V = std::remove_cvref_t<T>;
            using counts = yaml_variant_type_count<V>;
            const char c = *it;
            if constexpr (!yaml::check_flow_context(Opts)) {
               // At document root, a line beginning with '---' / '...' must be
               // a document marker token. Adjacent content (e.g. '---word') is
               // malformed and must not be treated as a plain scalar.
               if (ctx.current_indent() < 0 && (end - it >= 3)) {
                  const bool malformed_doc_start =
                     (it[0] == '-' && it[1] == '-' && it[2] == '-' && (end - it > 3) &&
                      !yaml::whitespace_or_line_end_table[static_cast<uint8_t>(it[3])]);
                  const bool malformed_doc_end =
                     (it[0] == '.' && it[1] == '.' && it[2] == '.' && (end - it > 3) &&
                      !yaml::whitespace_or_line_end_table[static_cast<uint8_t>(it[3])]);
                  if (malformed_doc_start || malformed_doc_end) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
            }
            auto is_plain_scalar_boundary = [](const char ch) {
               switch (ch) {
               case ' ':
               case '\t':
               case '\n':
               case '\r':
               case ',':
               case ']':
               case '}':
                  return true;
               default:
                  return false;
               }
            };

            auto has_indented_block_continuation = [&](auto word_end) {
               if constexpr (yaml::check_flow_context(Opts)) {
                  return false;
               }
               if (ctx.current_indent() < 0) {
                  return false;
               }
               if (word_end == end || (*word_end != '\n' && *word_end != '\r')) {
                  return false;
               }

               auto look = word_end;
               yaml::skip_newline(look, end);
               while (look != end) {
                  int32_t line_indent = 0;
                  while (look != end && *look == ' ') {
                     ++line_indent;
                     ++look;
                  }
                  if (look != end && *look == '\t') {
                     return false;
                  }
                  if (look == end || *look == '\n' || *look == '\r') {
                     if (look != end) {
                        yaml::skip_newline(look, end);
                        continue;
                     }
                     return false;
                  }
                  if (*look == '#') {
                     yaml::skip_comment(look, end);
                     if (look != end && (*look == '\n' || *look == '\r')) {
                        yaml::skip_newline(look, end);
                        continue;
                     }
                     return false;
                  }

                  // If the next non-empty line is an implicit mapping entry,
                  // don't treat it as scalar continuation for auto-deduced
                  // booleans/nulls.
                  {
                     auto scan = look;
                     while (scan != end && *scan != '\n' && *scan != '\r') {
                        if (*scan == ':') {
                           auto after_colon = scan + 1;
                           if (after_colon == end || *after_colon == ' ' || *after_colon == '\t' ||
                               *after_colon == '\n' || *after_colon == '\r') {
                              return false;
                           }
                        }
                        ++scan;
                     }
                  }

                  return line_indent > ctx.current_indent();
               }
               return false;
            };
            auto is_word_boundary = [&](const auto ptr) {
               if (ptr == end) {
                  return true;
               }
               if (*ptr == ':') {
                  auto next = ptr + 1;
                  return (next == end) || *next == ' ' || *next == '\t' || *next == '\n' || *next == '\r';
               }
               return is_plain_scalar_boundary(*ptr);
            };

            switch (c) {
            case '&': {
               // Anchor before value: parse name, then re-dispatch
               ++it;
               auto aname = yaml::parse_anchor_name(it, end);
               if (aname.empty()) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               yaml::skip_inline_ws(it, end);
               bool anchor_on_same_line = true;
               bool value_is_indentless_sequence = false;
               if (it == end || *it == '\n' || *it == '\r' || *it == '#') {
                  // Anchor followed by end-of-line. Check if the value continues on the
                  // next line (indented past current context) or if this is an empty node.
                  bool value_on_next_line = false;
                  if (it != end) {
                     auto peek = it;
                     // Skip comment if present
                     if (*peek == '#') {
                        while (peek != end && *peek != '\n' && *peek != '\r') ++peek;
                     }
                     // Skip newline
                     if (peek != end && (*peek == '\n' || *peek == '\r')) {
                        yaml::skip_newline(peek, end);
                        // Skip blank lines
                        while (peek != end && (*peek == '\n' || *peek == '\r')) {
                           yaml::skip_newline(peek, end);
                        }
                        // Measure indent of next content line
                        int32_t next_indent = 0;
                        while (peek != end && *peek == ' ') {
                           ++next_indent;
                           ++peek;
                        }
                        if (peek != end && *peek != '\n' && *peek != '\r') {
                           const bool indentless_sequence =
                              (!ctx.sequence_item_value_context) && (next_indent == ctx.current_indent()) &&
                              (*peek == '-') &&
                              ((peek + 1) == end || yaml::whitespace_or_line_end_table[static_cast<uint8_t>(*(peek + 1))]);
                           value_is_indentless_sequence = indentless_sequence;
                           const bool same_indent_property_node =
                              (next_indent == ctx.current_indent()) &&
                              (*peek == '!' || *peek == '&' || *peek == '*' || *peek == '[' || *peek == '{' ||
                               *peek == '"' || *peek == '\'' || *peek == '|' || *peek == '>');
                           value_on_next_line =
                              (next_indent > ctx.current_indent()) || indentless_sequence || same_indent_property_node;
                        }
                     }
                  }
                  if (!value_on_next_line) {
                     // Anchor on empty/null node — store empty span, leave value as default
                     ctx.anchors[std::string(aname)] = {&*it, &*it, ctx.current_indent()};
                     return;
                  }
                  // Value is on next line — advance past newline/blank lines to the content
                  if (*it == '#') {
                     while (it != end && *it != '\n' && *it != '\r') ++it;
                  }
                  if (it != end) yaml::skip_newline(it, end);
                  while (it != end && (*it == '\n' || *it == '\r')) {
                     yaml::skip_newline(it, end);
                  }
                  yaml::skip_inline_ws(it, end);
                  anchor_on_same_line = false;
               }
               // Anchor on alias (&anchor *alias) is invalid per YAML spec.
               // But alias-as-key (&anchor *alias : value) is valid.
               if (*it == '*' && !yaml::alias_token_is_mapping_key(it, end)) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               const char* anchor_start = &*it;
               const int32_t anchor_indent = ctx.current_indent();

               if constexpr (!yaml::check_flow_context(Opts)) {
                  // Check if the anchor is on a block-mapping key (&name key: value).
                  // Only when the anchor and key are on the SAME line — if the value was on the
                  // next line, the anchor applies to the entire next-line content.
                  if (anchor_on_same_line && line_could_be_block_mapping(it, end)) {
                     // Find the key span by scanning to the key-value separator
                     auto key_scan = it;
                     if (*key_scan == '"' || *key_scan == '\'') {
                        // Quoted key: skip to closing quote
                        const char quote = *key_scan;
                        ++key_scan;
                        while (key_scan != end && *key_scan != quote) {
                           if (*key_scan == '\\' && quote == '"') {
                              ++key_scan;
                              if (key_scan != end) ++key_scan;
                           }
                           else {
                              ++key_scan;
                           }
                        }
                        if (key_scan != end) ++key_scan; // past closing quote
                     }
                     else {
                        // Plain key: scan to ':'
                        while (key_scan != end) {
                           if (*key_scan == ':') {
                              auto next = key_scan + 1;
                              if (next == end || *next == ' ' || *next == '\t' || *next == '\n' || *next == '\r') {
                                 break;
                              }
                           }
                           ++key_scan;
                        }
                     }
                     // Store anchor with just the key text span
                     ctx.anchors[std::string(aname)] = {anchor_start, &*key_scan, anchor_indent};
                     // Parse as an object alternative so complex keys (e.g. flow collections) stay in mapping context.
                     if constexpr (counts::n_object > 0) {
                        process_yaml_variant_alternatives<V, is_yaml_variant_object>::template op<Opts>(value, ctx, it,
                                                                                                        end);
                     }
                     else {
                        from<YAML, V>::template op<Opts>(value, ctx, it, end);
                     }
                     return;
                  }
               }

               const bool prev_allow_indentless_sequence = ctx.allow_indentless_sequence;
               ctx.allow_indentless_sequence = value_is_indentless_sequence;
               from<YAML, V>::template op<Opts>(value, ctx, it, end);
               ctx.allow_indentless_sequence = prev_allow_indentless_sequence;
               if (!bool(ctx.error)) {
                  ctx.anchors[std::string(aname)] = {anchor_start, &*it, anchor_indent};
               }
               return;
            }
            case '*': {
               // In block context, "*alias : value" starts a mapping with an alias key.
               if constexpr (!yaml::check_flow_context(Opts)) {
                  if (yaml::alias_token_is_mapping_key(it, end)) {
                     if constexpr (counts::n_object > 0) {
                        process_yaml_variant_alternatives<V, is_yaml_variant_object>::template op<Opts>(value, ctx, it,
                                                                                                        end);
                        return;
                     }
                     else {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }
               }

               // Alias value: look up anchor and replay
               yaml::handle_alias<Opts>(value, ctx, it, end);
               return;
            }
            case '!': {
               // Tag - parse it and dispatch based on tag type
               const auto tag = yaml::parse_yaml_tag(it, end);
               if (tag == yaml::yaml_tag::unknown) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               yaml::skip_inline_ws(it, end);
               if (it != end && *it == '#') {
                  yaml::skip_comment(it, end);
               }
               if (it != end && (*it == '\n' || *it == '\r')) {
                  yaml::skip_to_content(it, end);
               }
               if constexpr (!yaml::check_flow_context(Opts)) {
                  // In block context, a comma immediately after a tag token is malformed
                  // (e.g., "!!str, value"). Flow context has legitimate ", ] }" usage.
                  if (it != end && *it == ',') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               if (it == end) {
                  if (tag == yaml::yaml_tag::str) {
                     if constexpr (counts::n_str > 0) {
                        constexpr auto first_idx = yaml_variant_first_index_v<V, is_yaml_variant_str>;
                        using StrType = std::variant_alternative_t<first_idx, V>;
                        value = StrType{};
                        return;
                     }
                  }
                  if (tag == yaml::yaml_tag::none) {
                     if constexpr (counts::n_null > 0) {
                        constexpr auto first_idx = yaml_variant_first_index_v<V, is_yaml_variant_null>;
                        using NullType = std::variant_alternative_t<first_idx, V>;
                        if (!std::holds_alternative<NullType>(value)) value = NullType{};
                        return;
                     }
                  }
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               switch (tag) {
               case yaml::yaml_tag::str:
                  process_yaml_variant_alternatives<V, is_yaml_variant_str>::template op<Opts>(value, ctx, it, end);
                  return;
               case yaml::yaml_tag::int_tag:
               case yaml::yaml_tag::float_tag:
                  if constexpr (counts::n_num > 0) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_num>::template op<Opts>(value, ctx, it, end);
                     return;
                  }
                  else { // else used to fix MSVC unreachable code warning
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               case yaml::yaml_tag::bool_tag:
                  if constexpr (counts::n_bool > 0) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_bool>::template op<Opts>(value, ctx, it, end);
                     return;
                  }
                  else { // else used to fix MSVC unreachable code warning
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               case yaml::yaml_tag::null_tag:
                  if constexpr (counts::n_null > 0) {
                     constexpr auto first_idx = yaml_variant_first_index_v<V, is_yaml_variant_null>;
                     using NullType = std::variant_alternative_t<first_idx, V>;
                     if (!std::holds_alternative<NullType>(value)) value = NullType{};
                     from<YAML, NullType>::template op<Opts>(std::get<NullType>(value), ctx, it, end);
                     return;
                  }
                  else { // else used to fix MSVC unreachable code warning
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               case yaml::yaml_tag::map:
                  if constexpr (counts::n_object > 0) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_object>::template op<Opts>(value, ctx, it,
                                                                                                     end);
                     return;
                  }
                  else { // else used to fix MSVC unreachable code warning
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               case yaml::yaml_tag::seq:
                  if constexpr (counts::n_array > 0) {
                     const bool prev_allow_indentless_sequence = ctx.allow_indentless_sequence;
                     if constexpr (!yaml::check_flow_context(Opts)) {
                        if (it != end && *it == '-') {
                           ctx.allow_indentless_sequence = true;
                        }
                     }
                     process_yaml_variant_alternatives<V, is_yaml_variant_array>::template op<Opts>(value, ctx, it,
                                                                                                    end);
                     ctx.allow_indentless_sequence = prev_allow_indentless_sequence;
                     return;
                  }
                  else { // else used to fix MSVC unreachable code warning
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               default:
                  // none tag - continue with normal parsing based on what follows
                  break;
               }
               // If we get here with a none tag, re-dispatch based on what character is now present
               // (fall through to the main switch by recursively calling op)
               from<YAML, V>::template op<Opts>(value, ctx, it, end);
               return;
            }
            case '{':
               if constexpr (!yaml::check_flow_context(Opts)) {
                  // In block context, "{...}: value" can be a complex mapping key.
                  if (ctx.current_indent() < 0 && line_could_be_block_mapping(it, end)) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_object>::template op<Opts>(value, ctx, it,
                                                                                                     end);
                     return;
                  }
               }
               // Flow mapping - object type
               process_yaml_variant_alternatives<V, is_yaml_variant_object>::template op<Opts>(value, ctx, it, end);
               return;
            case '[':
               if constexpr (!yaml::check_flow_context(Opts)) {
                  // In block context, "[...]: value" can be a complex mapping key.
                  if (ctx.current_indent() < 0 && line_could_be_block_mapping(it, end)) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_object>::template op<Opts>(value, ctx, it,
                                                                                                     end);
                     return;
                  }
               }
               // Flow sequence - array type
               process_yaml_variant_alternatives<V, is_yaml_variant_array>::template op<Opts>(value, ctx, it, end);
               return;
            case '?':
               // Explicit mapping key indicator ("? key")
               if ((end - it >= 2) && it[1] == '\t') {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if ((end - it >= 2) && (it[1] == ' ' || it[1] == '\n' || it[1] == '\r')) {
                  if constexpr (counts::n_object > 0) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_object>::template op<Opts>(value, ctx, it,
                                                                                                     end);
                     return;
                  }
               }
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            case '%':
               // Reserved directive indicator is not a plain scalar start.
               ctx.error = error_code::syntax_error;
               return;
            case '"':
            case '\'':
               // Could be a quoted string OR a quoted key in a block mapping
               // Try block mapping first (e.g., "key": value), then fall back to string
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            case 't':
            case 'T':
               // Could be "true", "True", "TRUE", or a key/string starting with t/T
               if constexpr (counts::n_bool > 0) {
                  if ((end - it >= 4) && ((it[1] == 'r' && it[2] == 'u' && it[3] == 'e') ||
                                          (it[1] == 'R' && it[2] == 'U' && it[3] == 'E'))) {
                     // Check what follows the word "true"
                     const auto after_true = it + 4;
                     const bool at_word_boundary = is_word_boundary(after_true);

                     if (at_word_boundary) {
                        if (has_indented_block_continuation(after_true)) {
                           parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
                           return;
                        }
                        // Check if this is a key (followed by ": ") rather than a boolean value
                        // "true: value" should parse as {true: value}, not as the boolean true
                        if ((end - it > 4) && it[4] == ':' &&
                            (end - it == 5 || it[5] == ' ' || it[5] == '\t' || it[5] == '\n' || it[5] == '\r')) {
                           // This is "true:" followed by space/tab/newline/end - treat as key
                           parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
                           return;
                        }
                        process_yaml_variant_alternatives<V, is_yaml_variant_bool>::template op<Opts>(value, ctx, it,
                                                                                                      end);
                        return;
                     }
                     // "true" is followed by more characters (e.g., "True\b") - treat as string
                  }
               }
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            case 'f':
            case 'F':
               // Could be "false", "False", "FALSE", or a key/string starting with f/F
               if constexpr (counts::n_bool > 0) {
                  if ((end - it >= 5) && ((it[1] == 'a' && it[2] == 'l' && it[3] == 's' && it[4] == 'e') ||
                                          (it[1] == 'A' && it[2] == 'L' && it[3] == 'S' && it[4] == 'E'))) {
                     // Check what follows the word "false"
                     const auto after_false = it + 5;
                     const bool at_word_boundary = is_word_boundary(after_false);

                     if (at_word_boundary) {
                        if (has_indented_block_continuation(after_false)) {
                           parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
                           return;
                        }
                        // Check if this is a key (followed by ": ") rather than a boolean value
                        // "false: value" should parse as {false: value}, not as the boolean false
                        if ((end - it > 5) && it[5] == ':' &&
                            (end - it == 6 || it[6] == ' ' || it[6] == '\t' || it[6] == '\n' || it[6] == '\r')) {
                           // This is "false:" followed by space/tab/newline/end - treat as key
                           parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
                           return;
                        }
                        process_yaml_variant_alternatives<V, is_yaml_variant_bool>::template op<Opts>(value, ctx, it,
                                                                                                      end);
                        return;
                     }
                     // "false" is followed by more characters (e.g., "False\b") - treat as string
                  }
               }
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            case 'n':
            case 'N':
               // Could be "null", "Null", "NULL", or a key/string starting with n/N
               if constexpr (counts::n_null > 0) {
                  if ((end - it >= 4) && ((it[1] == 'u' && it[2] == 'l' && it[3] == 'l') ||
                                          (it[1] == 'U' && it[2] == 'L' && it[3] == 'L'))) {
                     // Check what follows the word "null"
                     const auto after_null = it + 4;
                     const bool at_word_boundary = is_word_boundary(after_null);

                     if (at_word_boundary) {
                        if (has_indented_block_continuation(after_null)) {
                           parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
                           return;
                        }
                        // Check if this is a key (followed by ": ") rather than a null value
                        // "null: value" should parse as {null: value}, not as the null type
                        if ((end - it > 4) && it[4] == ':' &&
                            (end - it == 5 || it[5] == ' ' || it[5] == '\t' || it[5] == '\n' || it[5] == '\r')) {
                           // This is "null:" followed by space/tab/newline/end - treat as key
                           parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
                           return;
                        }
                        constexpr auto first_idx = yaml_variant_first_index_v<V, is_yaml_variant_null>;
                        using NullType = std::variant_alternative_t<first_idx, V>;
                        if (!std::holds_alternative<NullType>(value)) value = NullType{};
                        from<YAML, NullType>::template op<Opts>(std::get<NullType>(value), ctx, it, end);
                        return;
                     }
                     // "null" is followed by more characters (e.g., "Null\b") - treat as string
                  }
               }
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            case '~':
               // Null indicator
               if constexpr (counts::n_null > 0) {
                  constexpr auto first_idx = yaml_variant_first_index_v<V, is_yaml_variant_null>;
                  using NullType = std::variant_alternative_t<first_idx, V>;
                  if (!std::holds_alternative<NullType>(value)) value = NullType{};
                  from<YAML, NullType>::template op<Opts>(std::get<NullType>(value), ctx, it, end);
                  return;
               }
               break; // Fall through to try other types
            case '-':
               // Could be negative number or block sequence indicator
               if ((end - it >= 2) && (it[1] == ' ' || it[1] == '\t' || it[1] == '\n' || it[1] == '\r')) {
                  // Block sequence indicator "- "
                  if constexpr (counts::n_array > 0) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_array>::template op<Opts>(value, ctx, it,
                                                                                                    end);
                     return;
                  }
               }
               if constexpr (counts::n_num > 0) {
                  if ((end - it >= 2) && (std::isdigit(static_cast<unsigned char>(it[1])) || it[1] == '.')) {
                     process_yaml_variant_alternatives<V, is_yaml_variant_num>::template op<Opts>(value, ctx, it, end);
                     return;
                  }
               }
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            case '+':
            case '.':
               // Could be a number (.5, +5, .inf, etc.) or a string (.c, .cpp, +name, etc.)
               // Try parsing as number speculatively, fall back to string on failure
               if constexpr (counts::n_num > 0) {
                  auto start_it = it;
                  yaml::yaml_context temp_ctx{};
                  temp_ctx.indent_stack = ctx.indent_stack;
                  temp_ctx.anchors = ctx.anchors;
                  temp_ctx.stream_begin = ctx.stream_begin;
                  process_yaml_variant_alternatives<V, is_yaml_variant_num>::template op<Opts>(value, temp_ctx, it,
                                                                                               end);
                  if (!bool(temp_ctx.error)) {
                     ctx.anchors = std::move(temp_ctx.anchors);
                     return; // Successfully parsed as number
                  }
                  it = start_it; // Restore and fall through to string
               }
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            default:
               // Check for digit (0-9) - numeric or block mapping key starting with digit
               if (c >= '0' && c <= '9') {
                  // Try block mapping first (e.g., "123key: value")
                  if (try_parse_block_mapping_into_variant<V, Opts>(value, ctx, it, end)) {
                     return;
                  }
                  // Try numeric value speculatively - values like "12:30" look numeric but aren't
                  if constexpr (counts::n_num > 0) {
                     auto start_it = it;
                     yaml::yaml_context temp_ctx{};
                     temp_ctx.indent_stack = ctx.indent_stack;
                     temp_ctx.anchors = ctx.anchors;
                     temp_ctx.stream_begin = ctx.stream_begin;
                     process_yaml_variant_alternatives<V, is_yaml_variant_num>::template op<Opts>(value, temp_ctx, it,
                                                                                                  end);
                     if (!bool(temp_ctx.error)) {
                        ctx.anchors = std::move(temp_ctx.anchors);
                        return; // Successfully parsed as number
                     }
                     it = start_it; // Restore and fall through to string
                  }
                  // Fall through to string
                  process_yaml_variant_alternatives<V, is_yaml_variant_str>::template op<Opts>(value, ctx, it, end);
                  return;
               }
               // Plain scalar - try block mapping, then string
               parse_block_mapping_or_string<V, Opts>(value, ctx, it, end);
               return;
            }
         }

         // For non-auto-deducible variants or fallback, try each type until one succeeds
         constexpr auto N = std::variant_size_v<std::remove_cvref_t<T>>;

         auto try_parse = [&]<size_t I>() -> bool {
            using V = std::variant_alternative_t<I, std::remove_cvref_t<T>>;
            auto start = it;

            V v{};
            yaml::yaml_context temp_ctx{};
            temp_ctx.anchors = ctx.anchors;
            temp_ctx.stream_begin = ctx.stream_begin;
            from<YAML, V>::template op<Opts>(v, temp_ctx, it, end);

            if (!bool(temp_ctx.error)) {
               value = std::move(v);
               ctx.anchors = std::move(temp_ctx.anchors);
               return true;
            }

            it = start;
            return false;
         };

         bool parsed = false;
         [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((parsed = parsed || try_parse.template operator()<Is>()), ...);
         }(std::make_index_sequence<N>{});

         if (!parsed) {
            ctx.error = error_code::no_matching_variant_type;
         }
      }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
   };

   // Convenience functions

   template <auto Opts = yaml::yaml_opts{}, class T, contiguous Buffer>
   [[nodiscard]] error_ctx read_yaml(T&& value, Buffer&& buffer) noexcept
   {
      if (buffer.empty()) {
         using V = std::remove_cvref_t<T>;
         if constexpr (requires { value = nullptr; }) {
            value = nullptr;
            return {};
         }
         else if constexpr (nullable_like<V>) {
            value = {};
            return {};
         }
      }
      yaml::yaml_context ctx{};
      return read<set_yaml<Opts>()>(std::forward<T>(value), std::forward<Buffer>(buffer), ctx);
   }

   template <auto Opts = yaml::yaml_opts{}, class T>
   [[nodiscard]] error_ctx read_file_yaml(T&& value, const sv file_path) noexcept
   {
      std::string buffer;
      const auto ec = file_to_buffer(buffer, file_path);
      if (bool(ec)) {
         return {0, ec};
      }

      yaml::yaml_context ctx{};
      return read<set_yaml<Opts>()>(std::forward<T>(value), buffer, ctx);
   }

} // namespace glz
