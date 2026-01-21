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
   template <glaze_value_t T>
   struct from<YAML, T>
   {
      template <auto Opts, is_context Ctx, class It0, class It1>
      static void op(auto&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
         from<YAML, V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx),
                                          std::forward<It0>(it), std::forward<It1>(end));
      }
   };

   namespace yaml
   {
      // Parse a double-quoted string
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_double_quoted_string(std::string& value, Ctx& ctx, It& it, End end)
      {
         if (it == end || *it != '"') [[unlikely]] {
            ctx.error = error_code::expected_quote;
            return;
         }

         ++it; // skip opening quote
         value.clear();

         while (it != end) {
            if (*it == '\\') {
               ++it;
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               switch (*it) {
               case '"':
                  value.push_back('"');
                  break;
               case '\\':
                  value.push_back('\\');
                  break;
               case 'n':
                  value.push_back('\n');
                  break;
               case 't':
                  value.push_back('\t');
                  break;
               case 'r':
                  value.push_back('\r');
                  break;
               case '0':
                  value.push_back('\0');
                  break;
               case '/':
                  value.push_back('/');
                  break;
               case 'b':
                  value.push_back('\b');
                  break;
               case 'f':
                  value.push_back('\f');
                  break;
               default:
                  value.push_back('\\');
                  value.push_back(*it);
                  break;
               }
               ++it;
            }
            else if (*it == '"') {
               ++it; // skip closing quote
               return;
            }
            else {
               value.push_back(*it);
               ++it;
            }
         }
         ctx.error = error_code::unexpected_end;
      }

      // Parse a single-quoted string
      template <class Ctx, class It, class End>
      GLZ_ALWAYS_INLINE void parse_single_quoted_string(std::string& value, Ctx& ctx, It& it, End end)
      {
         if (it == end || *it != '\'') [[unlikely]] {
            ctx.error = error_code::expected_quote;
            return;
         }

         ++it; // skip opening quote
         value.clear();

         while (it != end) {
            if (*it == '\'') {
               // Check for escaped single quote ('')
               if ((it + 1) != end && *(it + 1) == '\'') {
                  value.push_back('\'');
                  it += 2;
               }
               else {
                  ++it; // skip closing quote
                  return;
               }
            }
            else {
               value.push_back(*it);
               ++it;
            }
         }
         ctx.error = error_code::unexpected_end;
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
            int32_t line_indent = measure_indent(it, end);

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
                  // Remove underscores for parsing
                  std::string clean;
                  for (size_t i = offset; i < num_str.size(); ++i) {
                     if (num_str[i] != '_') {
                        clean.push_back(num_str[i]);
                     }
                  }

                  auto [ptr, ec] =
                     std::from_chars(clean.data(), clean.data() + clean.size(), value, base);
                  if (ec != std::errc{}) {
                     ctx.error = error_code::parse_number_failure;
                  }
                  return;
               }
            }
         }

         // Standard number parsing (remove underscores)
         std::string clean;
         for (char c : num_str) {
            if (c != '_') {
               clean.push_back(c);
            }
         }

         if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
            auto result = glz::fast_float::from_chars(clean.data(), clean.data() + clean.size(), value);
            if (result.ec != std::errc{}) {
               ctx.error = error_code::parse_number_failure;
            }
         }
         else {
            auto [ptr, ec] = std::from_chars(clean.data(), clean.data() + clean.size(), value);
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

         if (yaml::check_unsupported_feature(*it, ctx)) [[unlikely]] {
            return;
         }

         // Check for null
         auto start = it;
         std::string str;
         yaml::parse_plain_scalar(str, ctx, it, end, yaml::check_flow_context(Opts));

         if (yaml::is_yaml_null(str)) {
            value = {};
            return;
         }

         // Not null - reset and parse the actual value
         it = start;

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
            const auto index =
               decode_hash_with_size<YAML, T, HashInfo, HashInfo.type>::op(str.data(), str.data() + str.size(), str.size());

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

            using value_type = typename std::remove_cvref_t<T>::value_type;

            if constexpr (emplace_backable<std::remove_cvref_t<T>>) {
               auto& element = value.emplace_back();
               from<YAML, value_type>::template op<flow_context_on<Opts>()>(element, ctx, it, end);
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }

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

            // Parse key
            std::string key;
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
            const auto index =
               decode_hash_with_size<YAML, U, HashInfo, HashInfo.type>::op(key.data(), key.data() + key.size(), key.size());

            const bool key_matches = index < N && key == reflect<U>::keys[index];

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
         while (it != end) {
            // Skip blank lines and comments
            while (it != end && (*it == '\n' || *it == '\r' || *it == '#' || *it == ' ')) {
               if (*it == '#') {
                  skip_comment(it, end);
               }
               else if (*it == '\n' || *it == '\r') {
                  skip_newline(it, end);
               }
               else {
                  // Check indentation
                  auto line_start = it;
                  int32_t line_indent = measure_indent(it, end);

                  if (it == end || *it == '\n' || *it == '\r') {
                     // Blank line
                     continue;
                  }

                  if (line_indent < sequence_indent) {
                     // Dedented - end of sequence
                     it = line_start;
                     return;
                  }

                  if (*it != '-') {
                     // Not a sequence item - end of sequence
                     it = line_start;
                     return;
                  }

                  break;
               }
            }

            if (it == end) break;

            // Check for document end marker
            if (at_document_end(it, end)) break;

            // Measure current indent
            auto line_start = it;
            int32_t line_indent = measure_indent(it, end);

            if (it == end) break;

            // Check for dedent
            if (line_indent < sequence_indent) {
               it = line_start;
               return;
            }

            // Expect dash
            if (*it != '-') {
               it = line_start;
               return;
            }

            ++it; // Skip '-'

            // Check for valid sequence item indicator (- followed by space or newline)
            if (it != end && *it != ' ' && *it != '\t' && *it != '\n' && *it != '\r') {
               // Not a sequence item (could be a number like -5)
               it = line_start;
               return;
            }

            skip_inline_ws(it, end);

            using value_type = typename std::remove_cvref_t<T>::value_type;

            if constexpr (emplace_backable<std::remove_cvref_t<T>>) {
               auto& element = value.emplace_back();

               // Check what follows
               if (it != end && *it != '\n' && *it != '\r' && *it != '#') {
                  from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
               }
               else {
                  // Value on next line - nested block
                  skip_ws_and_comment(it, end);
                  skip_newline(it, end);

                  // Get indent of nested content
                  auto nested_start = it;
                  int32_t nested_indent = measure_indent(it, end);
                  it = nested_start;

                  if (nested_indent > line_indent) {
                     from<YAML, value_type>::template op<Opts>(element, ctx, it, end);
                  }
                  // else: empty element (default constructed)
               }
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            // Skip to next line
            skip_ws_and_comment(it, end);
            if (it != end && (*it == '\n' || *it == '\r')) {
               skip_newline(it, end);
            }
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

         while (it != end) {
            // Skip blank lines and comments
            while (it != end) {
               if (*it == '#') {
                  skip_comment(it, end);
                  skip_newline(it, end);
               }
               else if (*it == '\n' || *it == '\r') {
                  skip_newline(it, end);
               }
               else if (*it == ' ') {
                  auto line_start = it;
                  int32_t line_indent = measure_indent(it, end);

                  if (it == end || *it == '\n' || *it == '\r' || *it == '#') {
                     // Blank or comment line
                     continue;
                  }

                  if (line_indent < mapping_indent) {
                     it = line_start;
                     return; // Dedented
                  }

                  break;
               }
               else {
                  break;
               }
            }

            if (it == end) break;

            // Check for document end marker
            if (at_document_end(it, end)) break;

            // Check indentation
            auto line_start = it;
            int32_t line_indent = 0;
            if (*it == ' ') {
               line_indent = measure_indent(it, end);
            }

            if (it == end) break;

            // Check for dedent
            if (line_indent < mapping_indent) {
               it = line_start;
               return;
            }

            // Check for sequence indicator (not a mapping key)
            if (*it == '-' && ((it + 1) == end || *(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\n')) {
               it = line_start;
               return;
            }

            // Parse key
            std::string key;
            if (!parse_yaml_key(key, ctx, it, end, false)) {
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

            // Look up key
            const auto index =
               decode_hash_with_size<YAML, U, HashInfo, HashInfo.type>::op(key.data(), key.data() + key.size(), key.size());

            const bool key_matches = index < N && key == reflect<U>::keys[index];

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
                        if (it != end && *it != '\n' && *it != '\r' && *it != '#') {
                           from<YAML, member_type>::template op<Opts>(member, ctx, it, end);
                        }
                        else {
                           // Value on next line - nested block
                           skip_ws_and_comment(it, end);
                           skip_newline(it, end);

                           auto nested_start = it;
                           int32_t nested_indent = measure_indent(it, end);
                           it = nested_start;

                           if (nested_indent > line_indent) {
                              from<YAML, member_type>::template op<Opts>(member, ctx, it, end);
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
               if (it != end && *it != '\n' && *it != '\r' && *it != '#') {
                  skip_yaml_value<Opts>(ctx, it, end, line_indent, false);
               }
            }

            if (bool(ctx.error)) [[unlikely]]
               return;

            // Skip to next line
            skip_ws_and_comment(it, end);
            if (it != end && (*it == '\n' || *it == '\r')) {
               skip_newline(it, end);
            }
         }
      }

   } // namespace yaml

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

         yaml::skip_inline_ws(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (*it == '[') {
            // Flow sequence
            yaml::parse_flow_sequence<Opts>(value, ctx, it, end);
         }
         else if (*it == '-') {
            // Block sequence
            // Find the indent level
            // We're already past the indentation, so sequence_indent is 0 or whatever was measured
            int32_t sequence_indent = 0;

            yaml::parse_block_sequence<Opts>(value, ctx, it, end, sequence_indent);
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
               if (it != end && *it != ' ' && *it != '\t' && *it != '\n' && *it != '\r') {
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

         if (*it == '{') {
            // Flow mapping
            yaml::parse_flow_mapping<Opts>(value, ctx, it, end);
         }
         else {
            // Block mapping
            int32_t mapping_indent = 0;
            yaml::parse_block_mapping<Opts>(value, ctx, it, end, mapping_indent);
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
               yaml::measure_indent(it, end);

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
               if (it != end && *it != '\n' && *it != '\r' && *it != '#') {
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

         // Try to detect type from content
         [[maybe_unused]] const char c = *it;

         // Try each variant type in order
         auto try_parse = [&]<size_t I>() -> bool {
            using V = std::variant_alternative_t<I, std::remove_cvref_t<T>>;
            auto start = it;

            V v{};
            context temp_ctx{};
            from<YAML, V>::template op<Opts>(v, temp_ctx, it, end);

            if (!bool(temp_ctx.error)) {
               value = std::move(v);
               return true;
            }

            it = start;
            return false;
         };

         // Try parsing based on syntax hints
         constexpr auto N = std::variant_size_v<std::remove_cvref_t<T>>;

         bool parsed = false;
         [&]<size_t... Is>(std::index_sequence<Is...>) { ((parsed = parsed || try_parse.template operator()<Is>()), ...); }
         (std::make_index_sequence<N>{});

         if (!parsed) {
            ctx.error = error_code::no_matching_variant_type;
         }
      }
   };

   // Convenience functions

   template <auto Opts = yaml::yaml_opts{}, class T, contiguous Buffer>
   [[nodiscard]] error_ctx read_yaml(T&& value, Buffer&& buffer) noexcept
   {
      return read<set_yaml<Opts>()>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <auto Opts = yaml::yaml_opts{}, class T>
   [[nodiscard]] error_ctx read_file_yaml(T&& value, const sv file_path) noexcept
   {
      std::string buffer;
      const auto ec = file_to_buffer(buffer, file_path);
      if (bool(ec)) {
         return {0, ec};
      }

      return read<set_yaml<Opts>()>(std::forward<T>(value), buffer);
   }

} // namespace glz
