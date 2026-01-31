// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cctype>
#include <charconv>

#include "glaze/core/common.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
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
         // Skip document start marker (---) if present
         yaml::skip_document_start(it, end);

         using V = std::remove_cvref_t<T>;
         from<YAML, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), end);
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
      // SWAR-optimized double-quoted string parsing
      // Uses 8-byte chunks for fast scanning and copying
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_double_quoted_string(std::string& value, Ctx& ctx, It& it, End end)
      {
         static constexpr size_t string_padding_bytes = 8;

         if (it == end || *it != '"') [[unlikely]] {
            ctx.error = error_code::expected_quote;
            return;
         }

         ++it; // skip opening quote
         auto start = it;

         // Pass 1: Find closing quote using SWAR
         // Process 8 bytes at a time looking for quote or backslash
         const auto remaining = static_cast<size_t>(end - it);
         if (remaining >= 8) {
            const auto* end8 = &*(end - 7); // Safe to read 8 bytes up to here
            while (it < end8) {
               uint64_t chunk;
               std::memcpy(&chunk, &*it, 8);
               if constexpr (std::endian::native == std::endian::big) {
                  chunk = std::byteswap(chunk);
               }

               // Check for quote or backslash
               const uint64_t has_quote_mask = glz::has_quote(chunk);
               const uint64_t has_backslash_mask = glz::has_escape(chunk);
               const uint64_t special = has_quote_mask | has_backslash_mask;

               if (special) {
                  // Found a special character - process byte by byte from here
                  const auto offset = countr_zero(special) >> 3;
                  it += offset;
                  break;
               }
               it += 8;
            }
         }

         // Finish finding the closing quote byte-by-byte
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

         // Pass 2: Copy and process escapes using SWAR
         auto src = start;
         const auto* const src_end = &*it;

         while (src < src_end) {
            const auto src_remaining = static_cast<size_t>(src_end - src);

            // Try to copy 8 bytes at a time when no escapes
            if (src_remaining >= 8) {
               uint64_t chunk;
               std::memcpy(&chunk, src, 8);
               if constexpr (std::endian::native == std::endian::big) {
                  chunk = std::byteswap(chunk);
               }

               // Check for backslash using SWAR
               const uint64_t has_backslash_mask = glz::has_escape(chunk);
               if (!has_backslash_mask) {
                  // No backslash in this chunk - copy all 8 bytes
                  std::memcpy(dst, src, 8);
                  src += 8;
                  dst += 8;
                  continue;
               }

               // Found a backslash - copy bytes up to it
               const auto offset = countr_zero(has_backslash_mask) >> 3;
               if (offset > 0) {
                  std::memcpy(dst, src, offset);
                  dst += offset;
                  src += offset;
               }
            }

            // Process one character (possibly an escape)
            if (src >= src_end) break;

            if (*src == '\\') {
               ++src;
               if (src >= src_end) [[unlikely]] {
                  // Shouldn't happen - we validated in pass 1
                  ctx.error = error_code::syntax_error;
                  return;
               }

               const unsigned char esc = static_cast<unsigned char>(*src);

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
                  // Unknown escape - pass through literally
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

      // SWAR-optimized single-quoted string parsing
      // Only escape is '' -> ' (doubled single quote)
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_single_quoted_string(std::string& value, Ctx& ctx, It& it, End end)
      {
         static constexpr size_t string_padding_bytes = 8;

         if (it == end || *it != '\'') [[unlikely]] {
            ctx.error = error_code::expected_quote;
            return;
         }

         ++it; // skip opening quote
         auto start = it;

         // Pass 1: Find closing quote using SWAR
         const auto remaining = static_cast<size_t>(end - it);
         if (remaining >= 8) {
            const auto* end8 = &*(end - 7);
            while (it < end8) {
               uint64_t chunk;
               std::memcpy(&chunk, &*it, 8);
               if constexpr (std::endian::native == std::endian::big) {
                  chunk = std::byteswap(chunk);
               }

               const uint64_t has_sq = glz::has_char<'\''>(chunk);
               if (has_sq) {
                  const auto offset = countr_zero(has_sq) >> 3;
                  it += offset;
                  break;
               }
               it += 8;
            }
         }

         // Finish finding quote byte-by-byte
         while (it != end && *it != '\'') {
            ++it;
         }

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Now scan for '' escapes and find actual end
         auto scan = start;
         auto actual_end = it;
         bool has_escapes = false;

         while (scan < actual_end) {
            if (*scan == '\'') {
               if (scan + 1 < end && *(scan + 1) == '\'') {
                  has_escapes = true;
                  scan += 2;
               }
               else {
                  actual_end = scan;
                  break;
               }
            }
            else {
               ++scan;
            }
         }

         // Continue scanning past '' escapes to find true end
         it = scan;
         while (it != end) {
            if (*it == '\'') {
               if ((it + 1) != end && *(it + 1) == '\'') {
                  has_escapes = true;
                  it += 2;
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

         actual_end = it;
         const auto input_len = static_cast<size_t>(actual_end - start);

         if (!has_escapes) {
            // Fast path: no escapes, direct assign
            value.assign(&*start, input_len);
            ++it; // skip closing quote
            return;
         }

         // Has escapes: need to process
         value.resize(input_len + string_padding_bytes);
         auto* dst = value.data();
         auto* const dst_start = dst;
         auto src = start;
         const auto* const src_end = &*actual_end;

         while (src < src_end) {
            const auto src_remaining = static_cast<size_t>(src_end - src);

            // Try to copy 8 bytes at a time
            if (src_remaining >= 8) {
               uint64_t chunk;
               std::memcpy(&chunk, src, 8);
               if constexpr (std::endian::native == std::endian::big) {
                  chunk = std::byteswap(chunk);
               }

               const uint64_t has_sq = glz::has_char<'\''>(chunk);
               if (!has_sq) {
                  std::memcpy(dst, src, 8);
                  src += 8;
                  dst += 8;
                  continue;
               }

               const auto offset = countr_zero(has_sq) >> 3;
               if (offset > 0) {
                  std::memcpy(dst, src, offset);
                  dst += offset;
                  src += offset;
               }
            }

            if (src >= src_end) break;

            if (*src == '\'') {
               // Must be '' (escaped quote)
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

      // Parse a plain scalar (unquoted)
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_plain_scalar(std::string& value, Ctx&, It& it, End end, bool in_flow)
      {
         value.clear();

         while (it != end) {
            const char c = *it;

            // End conditions
            if (c == '\n' || c == '\r') break;
            if (c == '#') break; // Comment

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

         // Chomping indicator: - (strip), + (keep), or none (clip)
         char chomping = ' '; // default: clip
         int explicit_indent = 0;

         while (it != end && *it != '\n' && *it != '\r') {
            if (*it == '-' || *it == '+') {
               chomping = *it;
            }
            else if (*it >= '1' && *it <= '9') {
               explicit_indent = *it - '0';
            }
            else if (*it == ' ' || *it == '\t') {
               // Skip whitespace
            }
            else if (*it == '#') {
               // Skip comment
               while (it != end && *it != '\n' && *it != '\r') {
                  ++it;
               }
               break;
            }
            else {
               break;
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
         bool first_line = true;
         std::string trailing_newlines;

         while (it != end) {
            auto line_start = it;
            int32_t line_indent = measure_indent(it, end, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;

            // Check for blank line
            if (it == end || *it == '\n' || *it == '\r') {
               trailing_newlines.push_back('\n');
               skip_newline(it, end);
               continue;
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
            }

            // Check if we've dedented
            if (line_indent < content_indent) {
               it = line_start;
               break;
            }

            // Add previous newlines (unless this is the first line)
            if (!first_line) {
               if (indicator == '|') {
                  // Literal: preserve newlines
                  value += trailing_newlines;
               }
               else {
                  // Folded: single newline becomes space, multiple preserved
                  if (trailing_newlines.size() == 1) {
                     value.push_back(' ');
                  }
                  else {
                     value += trailing_newlines.substr(1); // Keep all but first
                  }
               }
            }
            trailing_newlines.clear();
            first_line = false;

            // Add indentation beyond content_indent for literal style
            if (indicator == '|' && line_indent > content_indent) {
               for (int32_t i = content_indent; i < line_indent; ++i) {
                  value.push_back(' ');
               }
            }

            // Skip to content_indent level
            it = line_start;
            for (int32_t i = 0; i < content_indent && it != end && *it == ' '; ++i) {
               ++it;
            }

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

         if (check_unsupported_feature(*it, ctx)) [[unlikely]] {
            return false;
         }

         if (*it == '"') {
            parse_double_quoted_string(key, ctx, it, end);
            return !bool(ctx.error);
         }
         else if (*it == '\'') {
            parse_single_quoted_string(key, ctx, it, end);
            return !bool(ctx.error);
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
               if (c == '\n' || c == '\r') break;
               if (in_flow && (c == ',' || c == ']' || c == '}')) break;
               if (c == '#') break;

               key.push_back(c);
               ++it;
            }

            // Trim trailing whitespace from key
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
               key.pop_back();
            }

            if (key.empty()) {
               ctx.error = error_code::syntax_error;
               return false;
            }
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
            ctx.error = error_code::feature_not_supported;
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

         if (yaml::check_unsupported_feature(*it, ctx)) [[unlikely]] {
            return;
         }

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
            yaml::parse_block_scalar(str, ctx, it, end, 0);
            break;
         case yaml::scalar_style::plain:
            yaml::parse_plain_scalar(str, ctx, it, end, yaml::check_flow_context(Opts));
            break;
         }

         if (!bool(ctx.error)) {
            value = std::move(str);
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
            ctx.error = error_code::feature_not_supported;
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

         if (yaml::check_unsupported_feature(*it, ctx)) [[unlikely]] {
            return;
         }

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
            ctx.error = error_code::feature_not_supported;
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

         if (yaml::check_unsupported_feature(*it, ctx)) [[unlikely]] {
            return;
         }

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
                  return;
               }
               if (it + 3 <= end && (std::string_view(it, 3) == "nan" || std::string_view(it, 3) == "NaN" ||
                                     std::string_view(it, 3) == "NAN")) {
                  value = std::numeric_limits<std::remove_cvref_t<T>>::quiet_NaN();
                  it += 3;
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
                     return;
                  }
               }
               it = start;
            }
         }

         // Parse as plain scalar and convert
         auto start = it;

         // Find end of number
         while (it != end) {
            const char c = *it;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ']' || c == '}' || c == '#' ||
                c == ':') {
               break;
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

         if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
            auto result = glz::fast_float::from_chars(clean_view.data(), clean_view.data() + clean_view.size(), value);
            if (result.ec != std::errc{}) {
               ctx.error = error_code::parse_number_failure;
            }
         }
         else {
            auto [ptr, ec] = std::from_chars(clean_view.data(), clean_view.data() + clean_view.size(), value);
            if (ec != std::errc{}) {
               ctx.error = error_code::parse_number_failure;
            }
         }
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
            ctx.error = error_code::feature_not_supported;
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

         if (yaml::check_unsupported_feature(*it, ctx)) [[unlikely]] {
            return;
         }

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
            ctx.error = error_code::feature_not_supported;
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

         if (yaml::check_unsupported_feature(*it, ctx)) [[unlikely]] {
            return;
         }

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
         skip_inline_ws(it, end);

         // Handle empty array
         if (it != end && *it == ']') {
            ++it;
            return;
         }

         using value_type = typename V::value_type;

         if constexpr (emplace_backable<V>) {
            // Resizable containers (vector, deque, list)
            while (it != end) {
               skip_inline_ws(it, end);

               auto& element = value.emplace_back();
               from<YAML, value_type>::template op<flow_context_on<Opts>()>(element, ctx, it, end);

               if (bool(ctx.error)) [[unlikely]]
                  return;

               skip_inline_ws(it, end);

               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if (*it == ']') {
                  ++it;
                  return;
               }
               else if (*it == ',') {
                  ++it;
                  skip_inline_ws(it, end);
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
               skip_inline_ws(it, end);

               from<YAML, value_type>::template op<flow_context_on<Opts>()>(value[i], ctx, it, end);

               if (bool(ctx.error)) [[unlikely]]
                  return;

               ++i;
               skip_inline_ws(it, end);

               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if (*it == ']') {
                  ++it;
                  return;
               }
               else if (*it == ',') {
                  ++it;
                  skip_inline_ws(it, end);
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
         skip_inline_ws(it, end);

         // Handle empty mapping
         if (it != end && *it == '}') {
            ++it;
            return;
         }

         while (it != end) {
            skip_inline_ws(it, end);

            if (it != end && *it == '}') {
               ++it;
               return;
            }

            // Parse key using thread-local buffer to avoid allocation
            auto& key = string_buffer();
            key.clear();
            if (!parse_yaml_key(key, ctx, it, end, true)) {
               return;
            }

            skip_inline_ws(it, end);

            // Expect colon
            if (it == end || *it != ':') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
            skip_inline_ws(it, end);

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
               skip_yaml_value<Opts>(ctx, it, end, 0, true);
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            skip_inline_ws(it, end);

            if (it != end && *it == '}') {
               ++it;
               return;
            }
            else if (it != end && *it == ',') {
               ++it;
               skip_inline_ws(it, end);
            }
            else if (it != end && (*it == '\n' || *it == '\r')) {
               // Allow newlines in flow mappings
               skip_newline(it, end);
               skip_inline_ws(it, end);
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
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

            // Get reference to element to parse into
            auto parse_element = [&](auto& element) {
               // Check what follows
               if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                  // Content on same line as dash - set indent to column after "- "
                  // This allows nested block mappings to know their indent level
                  auto saved_indent = ctx.indent;
                  ctx.indent = line_indent + 2; // dash + space
                  from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
                  ctx.indent = saved_indent;
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
                     // Save and set indent for nested parsing
                     auto saved_indent = ctx.indent;
                     ctx.indent = nested_indent;
                     from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
                     ctx.indent = saved_indent;
                  }
                  // else: empty element (default constructed)
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

            if (bool(ctx.error)) [[unlikely]]
               return;

            // Note: after parsing nested block content, iterator may already be
            // at the start of the next line. The outer loop will handle it.
         }
      }

      // Parse block mapping
      // key1: value1
      // key2: value2
      template <auto Opts, class T, class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_block_mapping(T&& value, Ctx& ctx, It& it, End end, int32_t mapping_indent)
      {
         using U = std::remove_cvref_t<T>;
         static constexpr auto N = reflect<U>::size;
         static constexpr auto HashInfo = hash_info<U>;

         // Track if we're on the first key (might be mid-line from sequence "- key: value")
         bool first_key = true;

         while (it != end) {
            // Find next content line and measure its indent
            auto line_start = it;
            int32_t line_indent =
               first_key ? mapping_indent : 0; // Default: mapping_indent for mid-line, 0 for new lines

            // Skip blank lines and comments (only if not first key mid-line)
            while (it != end) {
               if (*it == '#') {
                  skip_comment(it, end);
                  skip_newline(it, end);
                  first_key = false;
                  line_indent = 0; // Next line starts fresh
               }
               else if (*it == '\n' || *it == '\r') {
                  skip_newline(it, end);
                  first_key = false;
                  line_indent = 0; // Next line starts fresh
               }
               else if (*it == ' ') {
                  line_start = it;
                  line_indent = measure_indent(it, end, ctx);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  if (it == end || *it == '\n' || *it == '\r' || *it == '#') {
                     // Blank or comment line - continue to next
                     first_key = false;
                     continue;
                  }

                  // Found content - line_indent is set, check dedent
                  if (line_indent < mapping_indent) {
                     it = line_start;
                     return; // Dedented
                  }

                  first_key = false;
                  break;
               }
               else {
                  // Content at current position (no leading whitespace)
                  // line_indent was set at top of outer loop iteration
                  line_start = it;
                  break;
               }
            }

            if (it == end) break;

            // Check for document end marker
            if (at_document_end(it, end)) break;

            // Check for dedent (applies when line_indent was measured or defaulted to 0)
            if (!first_key && line_indent < mapping_indent) {
               it = line_start;
               return;
            }

            // Check for sequence indicator (not a mapping key)
            if (*it == '-' && ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n')) {
               it = line_start;
               return;
            }

            // Parse key using thread-local buffer to avoid allocation
            auto& key = string_buffer();
            key.clear();
            if (!parse_yaml_key(key, ctx, it, end, false)) {
               return;
            }
            first_key = false; // After first key, always check indent

            skip_inline_ws(it, end);

            // Expect colon
            if (it == end || *it != ':') {
               ctx.error = error_code::syntax_error;
               return;
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
                           from<YAML, member_type>::template op<Opts>(member, ctx, it, end);
                        }
                        else {
                           // Value on next line - nested block
                           skip_ws_and_comment(it, end);
                           skip_newline(it, end);

                           auto nested_start = it;
                           int32_t nested_indent = measure_indent(it, end, ctx);
                           if (bool(ctx.error)) [[unlikely]]
                              return false;
                           it = nested_start;

                           if (nested_indent > line_indent) {
                              // Save and set indent for nested parsing
                              auto saved_indent = ctx.indent;
                              ctx.indent = nested_indent;
                              from<YAML, member_type>::template op<Opts>(member, ctx, it, end);
                              ctx.indent = saved_indent;
                           }
                           // else: keep default value
                        }
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

               // Skip unknown value
               if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                  skip_yaml_value<Opts>(ctx, it, end, line_indent, false);
               }
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            // Skip to end of current line (trailing whitespace, comment) and newline
            // But don't skip if we're already at the start of a new line
            // (which happens after nested block parsing returns)
            if (it != end) {
               // Check if we're at leading whitespace of a new line (not trailing whitespace)
               // Leading whitespace would have content (not newline/comment) after it
               if (*it == ' ' || *it == '\t') {
                  auto peek = it;
                  skip_inline_ws(peek, end);
                  if (peek != end && *peek != '\n' && *peek != '\r' && *peek != '#') {
                     // There's content after whitespace - we're at leading indent of new line
                     // Don't skip anything, let the outer loop handle it
                     continue;
                  }
               }
               // Skip trailing whitespace and comment on current line
               skip_inline_ws(it, end);
               skip_comment(it, end);
               if (it != end && (*it == '\n' || *it == '\r')) {
                  skip_newline(it, end);
               }
            }
         }
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
         skip_inline_ws(it, end);

         // Handle empty array
         if (it != end && *it == ']') {
            ++it;
            return;
         }

         while (it != end) {
            skip_inline_ws(it, end);

            value_type element{};
            from<YAML, value_type>::template op<flow_context_on<Opts>()>(element, ctx, it, end);

            if (bool(ctx.error)) [[unlikely]]
               return;

            value.emplace(std::move(element));

            skip_inline_ws(it, end);

            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if (*it == ']') {
               ++it;
               return;
            }
            else if (*it == ',') {
               ++it;
               skip_inline_ws(it, end);
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
               auto saved_indent = ctx.indent;
               ctx.indent = line_indent + 2; // dash + space
               from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
               ctx.indent = saved_indent;
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
                  auto saved_indent = ctx.indent;
                  ctx.indent = nested_indent;
                  from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
                  ctx.indent = saved_indent;
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
            ctx.error = error_code::feature_not_supported;
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

         if (*peek == '[') {
            // Flow sequence
            it = peek;
            yaml::parse_flow_sequence_set<Opts>(value, ctx, it, end);
         }
         else if (*peek == '-') {
            // Block sequence
            if (tag != yaml::yaml_tag::none) {
               it = peek;
            }
            yaml::parse_block_sequence_set<Opts>(value, ctx, it, end, ctx.indent);
         }
         else {
            ctx.error = error_code::syntax_error;
         }
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
            ctx.error = error_code::feature_not_supported;
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

         if (*peek == '[') {
            // Flow sequence - consume whitespace and parse
            it = peek;
            yaml::parse_flow_sequence<Opts>(value, ctx, it, end);
         }
         else if (*peek == '-') {
            // Block sequence - don't consume leading whitespace, let parse_block_sequence handle it
            // But if there was a tag, we need to advance past it
            if (tag != yaml::yaml_tag::none) {
               it = peek;
               // Rewind to find the start of the line for proper indent handling
               // Actually, tags are rare for block sequences, just use the peek position
            }
            yaml::parse_block_sequence<Opts>(value, ctx, it, end, ctx.indent);
         }
         else {
            ctx.error = error_code::syntax_error;
         }
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
            ctx.error = error_code::feature_not_supported;
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
            yaml::skip_inline_ws(it, end);

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
                  yaml::skip_inline_ws(it, end);
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

               yaml::skip_inline_ws(it, end);
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
            ctx.error = error_code::feature_not_supported;
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
               if (yaml::check_unsupported_feature(*it, ctx)) [[unlikely]] {
                  return;
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
            ctx.error = error_code::feature_not_supported;
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

         if (*it == '{') {
            // Flow mapping
            yaml::parse_flow_mapping<Opts>(value, ctx, it, end);
         }
         else {
            // Block mapping - use indent from context (set by parent parser)
            yaml::parse_block_mapping<Opts>(value, ctx, it, end, ctx.indent);
         }
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
            ctx.error = error_code::feature_not_supported;
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

         using key_t = typename std::remove_cvref_t<T>::key_type;
         using val_t = typename std::remove_cvref_t<T>::mapped_type;

         if (*it == '{') {
            // Flow mapping
            ++it;
            yaml::skip_inline_ws(it, end);

            if (it != end && *it == '}') {
               ++it;
               return;
            }

            while (it != end) {
               yaml::skip_inline_ws(it, end);

               if (it != end && *it == '}') {
                  ++it;
                  return;
               }

               // Parse key
               key_t key{};
               from<YAML, key_t>::template op<yaml::flow_context_on<Opts>()>(key, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               yaml::skip_inline_ws(it, end);

               if (it == end || *it != ':') {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;
               yaml::skip_inline_ws(it, end);

               // Parse value
               val_t val{};
               from<YAML, val_t>::template op<yaml::flow_context_on<Opts>()>(val, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               value.emplace(std::move(key), std::move(val));

               yaml::skip_inline_ws(it, end);

               if (it != end && *it == '}') {
                  ++it;
                  return;
               }
               else if (it != end && *it == ',') {
                  ++it;
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
         else {
            // Block mapping
            while (it != end) {
               // Skip blank lines
               while (it != end && (*it == '\n' || *it == '\r' || *it == ' ' || *it == '#')) {
                  if (*it == '#') {
                     yaml::skip_comment(it, end);
                  }
                  else if (*it == '\n' || *it == '\r') {
                     yaml::skip_newline(it, end);
                  }
                  else {
                     break;
                  }
               }

               if (it == end) break;

               // Measure and skip indent
               yaml::measure_indent(it, end, ctx);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (it == end || *it == '\n' || *it == '\r') continue;

               // Parse key
               key_t key{};
               from<YAML, key_t>::template op<Opts>(key, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               yaml::skip_inline_ws(it, end);

               if (it == end || *it != ':') {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;
               yaml::skip_inline_ws(it, end);

               // Parse value
               val_t val{};
               if (it != end && !yaml::line_end_or_comment_table[static_cast<uint8_t>(*it)]) {
                  from<YAML, val_t>::template op<Opts>(val, ctx, it, end);
               }
               if (bool(ctx.error)) [[unlikely]]
                  return;

               value.emplace(std::move(key), std::move(val));

               yaml::skip_ws_and_comment(it, end);
               yaml::skip_newline(it, end);
            }
         }
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
   concept yaml_variant_array_type = array_t<remove_meta_wrapper_t<T>> || glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>;

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

   // Check if variant is auto-deducible based on type composition
   template <is_variant T>
   consteval auto yaml_variant_is_auto_deducible()
   {
      int bools{}, numbers{}, strings{}, objects{}, arrays{}, nulls{};
      constexpr auto N = std::variant_size_v<T>;
      for_each<N>([&]<auto I>() {
         using V = std::decay_t<std::variant_alternative_t<I, T>>;
         bools += bool_t<V>;
         numbers += num_t<V>;
         strings += str_t<V>;
         objects += (writable_map_t<V> || readable_map_t<V> || glaze_object_t<V> || reflectable<V> || is_memory_object<V>);
         arrays += (glaze_array_t<V> || array_t<V> || tuple_t<V> || is_std_tuple<V>);
         nulls += null_t<V>;
      });
      return bools < 2 && numbers < 2 && strings < 2 && objects < 2 && arrays < 2 && nulls < 2;
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
                  from<YAML, V>::template op<Opts>(std::get<V>(value), temp_ctx, it, end);

                  if (!bool(temp_ctx.error)) {
                     found_match = true;
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

   // Variant support
   template <is_variant T>
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

         if constexpr (yaml_variant_is_auto_deducible<std::remove_cvref_t<T>>()) {
            const char c = *it;

            switch (c) {
            case '{':
               // Flow mapping - object type
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_object>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case '[':
               // Flow sequence - array type
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_array>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case '"':
            case '\'':
               // Quoted string
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case 't':
            case 'T':
               // Could be "true", "True", "TRUE"
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_bool> > 0) {
                  // Peek ahead to check for boolean
                  if ((end - it >= 4) &&
                      ((it[1] == 'r' && it[2] == 'u' && it[3] == 'e') ||
                       (it[1] == 'R' && it[2] == 'U' && it[3] == 'E'))) {
                     process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_bool>::template op<Opts>(
                        value, ctx, it, end);
                     return;
                  }
               }
               // Otherwise treat as string
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case 'f':
            case 'F':
               // Could be "false", "False", "FALSE"
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_bool> > 0) {
                  // Peek ahead to check for boolean
                  if ((end - it >= 5) &&
                      ((it[1] == 'a' && it[2] == 'l' && it[3] == 's' && it[4] == 'e') ||
                       (it[1] == 'A' && it[2] == 'L' && it[3] == 'S' && it[4] == 'E'))) {
                     process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_bool>::template op<Opts>(
                        value, ctx, it, end);
                     return;
                  }
               }
               // Otherwise treat as string
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case 'n':
            case 'N':
               // Could be "null", "Null", "NULL"
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_null> > 0) {
                  if ((end - it >= 4) &&
                      ((it[1] == 'u' && it[2] == 'l' && it[3] == 'l') ||
                       (it[1] == 'U' && it[2] == 'L' && it[3] == 'L'))) {
                     constexpr auto first_idx = yaml_variant_first_index_v<std::remove_cvref_t<T>, is_yaml_variant_null>;
                     using V = std::variant_alternative_t<first_idx, std::remove_cvref_t<T>>;
                     if (!std::holds_alternative<V>(value)) value = V{};
                     from<YAML, V>::template op<Opts>(std::get<V>(value), ctx, it, end);
                     return;
                  }
               }
               // Otherwise treat as string
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case '~':
               // Null indicator
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_null> > 0) {
                  constexpr auto first_idx = yaml_variant_first_index_v<std::remove_cvref_t<T>, is_yaml_variant_null>;
                  using V = std::variant_alternative_t<first_idx, std::remove_cvref_t<T>>;
                  if (!std::holds_alternative<V>(value)) value = V{};
                  from<YAML, V>::template op<Opts>(std::get<V>(value), ctx, it, end);
                  return;
               }
               // Fall through to try other types
               break;
            case '-':
               // Could be negative number or block sequence indicator
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_num> > 0) {
                  // Check if followed by digit or decimal point (number)
                  if ((end - it >= 2) && (std::isdigit(static_cast<unsigned char>(it[1])) || it[1] == '.')) {
                     process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_num>::template op<Opts>(
                        value, ctx, it, end);
                     return;
                  }
                  // Check if followed by space (block sequence indicator)
                  if ((end - it >= 2) && (it[1] == ' ' || it[1] == '\n' || it[1] == '\r')) {
                     process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_array>::template op<Opts>(
                        value, ctx, it, end);
                     return;
                  }
               }
               // Try as string
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case '+':
            case '.':
               // Likely a number
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_num> > 0) {
                  process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_num>::template op<Opts>(
                     value, ctx, it, end);
                  return;
               }
               // Fall through to try as string
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                  value, ctx, it, end);
               return;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
               // Numeric value
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_num> > 0) {
                  process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_num>::template op<Opts>(
                     value, ctx, it, end);
                  return;
               }
               // Fall through to try as string
               process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                  value, ctx, it, end);
               return;
            default:
               // Plain scalar - try as string first, then fall back to trying all types
               if constexpr (yaml_variant_count_v<std::remove_cvref_t<T>, is_yaml_variant_str> > 0) {
                  process_yaml_variant_alternatives<std::remove_cvref_t<T>, is_yaml_variant_str>::template op<Opts>(
                     value, ctx, it, end);
                  return;
               }
               break;
            }
         }

         // For non-auto-deducible variants or fallback, try each type until one succeeds
         constexpr auto N = std::variant_size_v<std::remove_cvref_t<T>>;

         auto try_parse = [&]<size_t I>() -> bool {
            using V = std::variant_alternative_t<I, std::remove_cvref_t<T>>;
            auto start = it;

            V v{};
            yaml::yaml_context temp_ctx{};
            from<YAML, V>::template op<Opts>(v, temp_ctx, it, end);

            if (!bool(temp_ctx.error)) {
               value = std::move(v);
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
   };

   // Convenience functions

   template <auto Opts = yaml::yaml_opts{}, class T, contiguous Buffer>
   [[nodiscard]] error_ctx read_yaml(T&& value, Buffer&& buffer) noexcept
   {
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
