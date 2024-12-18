// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <charconv>
#include <optional>

#include "glaze/core/seek.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/skip.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   // TODO: GCC 12 lacks constexpr std::from_chars
   // Remove this code when dropping GCC 12 support
   namespace detail
   {
      struct from_chars_result
      {
         const char* ptr;
         std::errc ec;
      };

      inline constexpr int char_to_digit(char c) noexcept
      {
         if (c >= '0' && c <= '9') return c - '0';
         if (c >= 'a' && c <= 'z') return c - 'a' + 10;
         if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
         return -1;
      }

      template <class I>
      constexpr from_chars_result from_chars(const char* first, const char* last, I& value, int base = 10)
      {
         from_chars_result result{first, std::errc{}};

         // Basic validation of base
         if (base < 2 || base > 36) {
            // Not standard behavior to check base validity here, but let's return invalid_argument
            result.ec = std::errc::invalid_argument;
            return result;
         }

         using U = std::make_unsigned_t<I>;
         constexpr bool is_signed = std::is_signed<I>::value;
         constexpr U umax = (std::numeric_limits<U>::max)();

         if (first == last) {
            // Empty range
            result.ec = std::errc::invalid_argument;
            return result;
         }

         bool negative = false;
         // Check for sign only if signed type
         if constexpr (is_signed) {
            if (*first == '-') {
               negative = true;
               ++first;
            }
            else if (*first == '+') {
               ++first;
            }
         }

         if (first == last) {
            // After sign there's nothing
            result.ec = std::errc::invalid_argument;
            return result;
         }

         U acc = 0;
         bool any = false;

         // We'll do overflow checking as we parse
         // For accumulation: acc * base + digit
         // Overflow if acc > (umax - digit)/base

         while (first != last) {
            int d = char_to_digit(*first);
            if (d < 0 || d >= base) break;

            // Check overflow before multiplying/adding
            if (acc > (umax - static_cast<U>(d)) / static_cast<U>(base)) {
               // Overflow
               result.ec = std::errc::result_out_of_range;
               // We still move ptr to the last valid digit parsed
               result.ptr = first;
               // No need to parse further; we know it's out of range.
               return result;
            }

            acc = acc * base + static_cast<U>(d);
            any = true;
            ++first;
         }

         if (!any) {
            // No digits parsed
            result.ec = std::errc::invalid_argument;
            return result;
         }

         // If signed and negative, check if result fits
         if constexpr (is_signed) {
            using S = std::make_signed_t<U>;
            // The largest magnitude we can represent in a negative value is (max + 1)
            // since -(min()) = max() + 1.
            U limit = static_cast<U>(std::numeric_limits<I>::max()) + 1U;
            if (negative) {
               if (acc > limit) {
                  result.ec = std::errc::result_out_of_range;
                  result.ptr = first;
                  return result;
               }
               value = static_cast<I>(0 - static_cast<S>(acc));
            }
            else {
               if (acc > static_cast<U>(std::numeric_limits<I>::max())) {
                  result.ec = std::errc::result_out_of_range;
                  result.ptr = first;
                  return result;
               }
               value = static_cast<I>(acc);
            }
         }
         else {
            // Unsigned type
            value = acc;
         }

         result.ptr = first;
         return result;
      }
   }

   namespace jmespath
   {
      enum struct tokenization_error {
         none, // No error
         unbalanced_brackets, // Mismatched '[' and ']'
         unbalanced_parentheses, // Mismatched '(' and ')'
         unclosed_string, // String literal not properly closed
         invalid_escape_sequence, // Invalid escape sequence in string
         unexpected_delimiter, // Unexpected character encountered (e.g., consecutive delimiters)
      };

      struct tokenization_result
      {
         std::string_view first;
         std::string_view second;
         tokenization_error error;
      };

      /**
       * @brief Trims leading whitespace characters from a string_view.
       *
       * @param s The input string_view to trim.
       * @return A string_view with leading whitespace removed.
       */
      inline constexpr std::string_view trim_left(std::string_view s)
      {
         size_t start = 0;
         while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) {
            start++;
         }
         return s.substr(start);
      }

      /**
       * @brief Splits a JMESPath expression into the first token and the remaining path with error handling.
       *
       * @param s The JMESPath expression to tokenize.
       * @return tokenization_result containing:
       *         - first: The first token of the expression.
       *         - second: The remaining expression after the first token.
       *         - error: tokenization_error indicating if an error occurred.
       */
      inline constexpr tokenization_result tokenize_jmes_path(std::string_view s)
      {
         if (s.empty()) {
            return {"", "", tokenization_error::none};
         }

         size_t pos = 0;
         size_t len = s.size();
         int bracket_level = 0;
         int parenthesis_level = 0;
         bool in_string = false;
         char string_delim = '\0';

         while (pos < len) {
            char current = s[pos];

            if (in_string) {
               if (current == string_delim) {
                  // Check for escaped delimiter
                  size_t backslashes = 0;
                  size_t temp = pos;
                  while (temp > 0 && s[--temp] == '\\') {
                     backslashes++;
                  }
                  if (backslashes % 2 == 0) {
                     in_string = false;
                  }
               }
               else if (current == '\\') {
                  // Validate escape sequence
                  if (pos + 1 >= len) {
                     return {"", "", tokenization_error::invalid_escape_sequence};
                  }
                  char next_char = s[pos + 1];
                  // Simple validation: allow known escape characters
                  if (next_char != '"' && next_char != '\'' && next_char != '\\' && next_char != '/' &&
                      next_char != 'b' && next_char != 'f' && next_char != 'n' && next_char != 'r' &&
                      next_char != 't' && next_char != 'u') {
                     return {"", "", tokenization_error::invalid_escape_sequence};
                  }
               }
               pos++;
               continue;
            }

            switch (current) {
            case '"':
            case '\'':
               in_string = true;
               string_delim = current;
               pos++;
               break;
            case '[':
               bracket_level++;
               pos++;
               break;
            case ']':
               if (bracket_level > 0) {
                  bracket_level--;
               }
               else {
                  return {"", "", tokenization_error::unbalanced_brackets};
               }
               pos++;
               break;
            case '(':
               parenthesis_level++;
               pos++;
               break;
            case ')':
               if (parenthesis_level > 0) {
                  parenthesis_level--;
               }
               else {
                  return {"", "", tokenization_error::unbalanced_parentheses};
               }
               pos++;
               break;
            case '.':
            case '|':
               if (bracket_level == 0 && parenthesis_level == 0) {
                  // Split here
                  return {s.substr(0, pos), s.substr(pos, len - pos), tokenization_error::none};
               }
               pos++;
               break;
            default:
               pos++;
               break;
            }
         }

         if (in_string) {
            return {"", "", tokenization_error::unclosed_string};
         }

         if (bracket_level != 0) {
            return {"", "", tokenization_error::unbalanced_brackets};
         }

         if (parenthesis_level != 0) {
            return {"", "", tokenization_error::unbalanced_parentheses};
         }

         // If no delimiter found, return the whole string as first token
         return {s, "", tokenization_error::none};
      }

      /**
       * @brief Recursively tokenizes a full JMESPath expression into all its tokens with error handling.
       *
       * @param expression The complete JMESPath expression to tokenize.
       * @param tokens A vector to store all tokens in the order they appear.
       * @param error An output parameter to capture any tokenization errors.
       * @return true if tokenization succeeded without errors, false otherwise.
       */
      inline constexpr tokenization_error tokenize_full_jmespath(std::string_view expression,
                                                                 std::vector<std::string_view>& tokens)
      {
         tokens.clear();
         auto remaining = expression;

         while (!remaining.empty()) {
            tokenization_result result = tokenize_jmes_path(remaining);
            if (result.error != tokenization_error::none) {
               return result.error;
            }

            // Detect empty tokens which indicate consecutive delimiters
            if (result.first.empty()) {
               return tokenization_error::unexpected_delimiter;
            }

            tokens.emplace_back(result.first);

            // Remove the delimiter (either '.' or '|') from the rest
            if (!result.second.empty()) {
               char delimiter = result.second.front();
               if (delimiter == '.' || delimiter == '|') {
                  remaining = result.second.substr(1);
                  // Trim leading whitespace after the delimiter
                  remaining = trim_left(remaining);
                  // Check if the next character is another delimiter, indicating an empty token
                  if (!remaining.empty() && (remaining.front() == '.' || remaining.front() == '|')) {
                     return tokenization_error::unexpected_delimiter;
                  }
               }
               else {
                  // Unexpected character, set error and stop tokenization
                  return tokenization_error::unexpected_delimiter;
               }
            }
            else {
               break;
            }
         }

         return tokenization_error::none;
      }

      template <const std::string_view& expression>
      consteval auto tokenize_as_array()
      {
         constexpr auto N = [] {
            std::vector<std::string_view> tokens;
            auto err = tokenize_full_jmespath(expression, tokens);
            if (err != tokenization_error::none) {
               std::abort();
            }
            return tokens.size();
         }();

         std::vector<std::string_view> tokens;
         auto err = tokenize_full_jmespath(expression, tokens);
         if (err != tokenization_error::none) {
            std::abort();
         }

         std::array<std::string_view, N> arr{};
         for (std::size_t i = 0; i < N; ++i) {
            arr[i] = tokens[i];
         }
         return arr; // Vector destroyed here, leaving only the array.
      }

      struct ArrayParseResult
      {
         bool is_array_access = false; // True if "key[...]"
         bool error = false; // True if parsing encountered an error
         std::string_view key; // The part before the first '['
         std::optional<int32_t>
            start; // For a single index, this holds the index. For slices, this is the first number.
         std::optional<int32_t> end;
         std::optional<int32_t> step;
      };

      inline constexpr std::optional<int> parse_int(std::string_view s)
      {
         if (s.empty()) {
            return std::nullopt;
         }
         int value;
         auto result = detail::from_chars(s.data(), s.data() + s.size(), value);
         if (result.ec == std::errc()) {
            return value;
         }
         return std::nullopt;
      }

      // Parse a token that may have array indexing or slicing.
      inline constexpr ArrayParseResult parse_jmespath_token(std::string_view token)
      {
         ArrayParseResult result;

         // Find the first '[' and the corresponding ']'
         auto open_pos = token.find('[');
         if (open_pos == std::string_view::npos) {
            // No array access, just a key.
            result.key = token;
            result.is_array_access = false;
            result.error = false;
            return result;
         }

         auto close_pos = token.rfind(']');
         if (close_pos == std::string_view::npos || close_pos < open_pos) {
            // Mismatched brackets -> error
            result.key = token.substr(0, open_pos);
            result.is_array_access = true; // We found a '[', so it looks like array access was intended
            result.error = true;
            return result;
         }

         result.key = token.substr(0, open_pos);
         auto inside = token.substr(open_pos + 1, close_pos - (open_pos + 1));
         if (inside.empty()) {
            // "children[]" not valid
            result.is_array_access = true;
            result.error = true;
            return result;
         }

         // Count colons to determine if it's a slice
         size_t colon_count = 0;
         for (char c : inside) {
            if (c == ':') {
               colon_count++;
            }
         }

         // Helper lambda to parse slices:
         auto parse_slice = [&](std::string_view inside) {
            // We'll have up to 3 parts: start:end:step depending on colon_count (1 or 2)
            std::string_view parts[3];
            {
               size_t start_idx = 0;
               int idx = 0;
               for (size_t i = 0; i <= inside.size(); ++i) {
                  if (i == inside.size() || inside[i] == ':') {
                     if (idx < 3) {
                        parts[idx] = inside.substr(start_idx, i - start_idx);
                        idx++;
                     }
                     start_idx = i + 1;
                  }
               }
            }

            // parts are now filled. If colon_count == 1, we have parts[0] and parts[1], parts[2] empty
            // If colon_count == 2, we have parts[0], parts[1], and parts[2].
            // Empty parts are allowed.

            // Parse each non-empty part
            if (!parts[0].empty()) {
               auto val = parse_int(parts[0]);
               if (!val.has_value()) {
                  result.error = true;
               }
               else {
                  result.start = val;
               }
            }

            if (!parts[1].empty()) {
               auto val = parse_int(parts[1]);
               if (!val.has_value()) {
                  result.error = true;
               }
               else {
                  result.end = val;
               }
            }

            if (colon_count == 2 && !parts[2].empty()) {
               auto val = parse_int(parts[2]);
               if (!val.has_value()) {
                  result.error = true;
               }
               else {
                  result.step = val;
               }
            }
         };

         result.is_array_access = true; // If we found brackets, it's array access attempt

         if (colon_count == 0) {
            // single integer index
            auto val = parse_int(inside);
            if (!val.has_value()) {
               // Not a valid integer
               result.error = true;
            }
            else {
               result.start = val; // store single index in start
            }
         }
         else if (colon_count == 1 || colon_count == 2) {
            // slice syntax
            parse_slice(inside);
         }
         else {
            // More than 2 colons is invalid
            result.error = true;
         }

         return result;
      }
   }

   // Read into a C++ type given a path denoted by a JMESPath query
   template <string_literal Path, opts Opts = opts{}, class T, contiguous Buffer>
      requires(Opts.format == JSON)
   [[nodiscard]] inline error_ctx read_jmespath(T&& value, Buffer&& buffer)
   {
      static constexpr auto S = chars<Path>;
      static constexpr auto tokens = jmespath::tokenize_as_array<S>();
      static constexpr auto N = tokens.size();

      constexpr bool use_padded = resizable<Buffer> && non_const_buffer<Buffer> && !has_disable_padding(Opts);

      if constexpr (use_padded) {
         // Pad the buffer for SWAR
         buffer.resize(buffer.size() + padding_bytes);
      }
      auto p = read_iterators<Opts>(buffer);

      auto it = p.first;
      auto end = p.second;

      auto start = it;

      context ctx{};

      static constexpr auto FinalOpts = use_padded ? is_padded_on<Opts>() : is_padded_off<Opts>();

      if constexpr (N == 0) {
         detail::read<Opts.format>::template op<FinalOpts>(value, ctx, it, end);
      }
      else {
         using namespace glz::detail;

         skip_ws<Opts>(ctx, it, end);

         for_each<N>([&](auto I) {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            static constexpr auto decomposed_key = jmespath::parse_jmespath_token(tokens[I]);

            static constexpr auto key = decomposed_key.key;
            if constexpr (decomposed_key.is_array_access) {
               GLZ_MATCH_OPEN_BRACE;

               while (true) {
                  GLZ_SKIP_WS();
                  GLZ_MATCH_QUOTE;

                  auto* start = it;
                  skip_string_view<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  const sv k = {start, size_t(it - start)};
                  ++it;

                  if (key.size() == k.size() && comparitor<key>(k.data())) {
                     GLZ_SKIP_WS();
                     GLZ_MATCH_COLON();
                     GLZ_SKIP_WS();
                     GLZ_MATCH_OPEN_BRACKET;

                     static constexpr auto n = decomposed_key.start;
                     if constexpr (n) {
                        for_each<n.value()>([&](auto) {
                           skip_value<JSON>::op<Opts>(ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] {
                              return;
                           }
                           if (*it != ',') {
                              ctx.error = error_code::array_element_not_found;
                              return;
                           }
                           ++it;
                        });

                        GLZ_SKIP_WS();

                        if constexpr (I == (N - 1)) {
                           detail::read<Opts.format>::template op<FinalOpts>(value, ctx, it, end);
                        }
                        return;
                     }
                     else {
                        ctx.error = error_code::array_element_not_found;
                        return;
                     }
                     return;
                  }
                  else {
                     skip_value<JSON>::op<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (*it != ',') {
                        ctx.error = error_code::key_not_found;
                        return;
                     }
                     ++it;
                  }
               }
            }
            else {
               GLZ_MATCH_OPEN_BRACE;

               while (it < end) {
                  GLZ_SKIP_WS();
                  GLZ_MATCH_QUOTE;

                  auto* start = it;
                  skip_string_view<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  const sv k = {start, size_t(it - start)};
                  ++it;

                  if (key.size() == k.size() && comparitor<key>(k.data())) {
                     GLZ_SKIP_WS();
                     GLZ_MATCH_COLON();
                     GLZ_SKIP_WS();

                     if constexpr (I == (N - 1)) {
                        detail::read<Opts.format>::template op<FinalOpts>(value, ctx, it, end);
                     }
                     return;
                  }
                  else {
                     skip_value<JSON>::op<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (*it != ',') {
                        ctx.error = error_code::key_not_found;
                        return;
                     }
                     ++it;
                  }
               }
            }
         });
      }

      if constexpr (use_padded) {
         // Restore the original buffer state
         buffer.resize(buffer.size() - padding_bytes);
      }

      return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
   }

   // Read into a C++ type given a path denoted by a JMESPath query
   // This version supports a runtime path
   template <opts Opts = opts{}, class T, contiguous Buffer>
      requires(Opts.format == JSON)
   [[nodiscard]] inline error_ctx read_jmespath(const std::string_view path, T&& value, Buffer&& buffer)
   {
      std::vector<std::string_view> tokens{};
      if (auto ec = jmespath::tokenize_full_jmespath(path, tokens); bool(ec)) {
         return {error_code::syntax_error};
      }
      const auto N = tokens.size();

      constexpr bool use_padded = resizable<Buffer> && non_const_buffer<Buffer> && !has_disable_padding(Opts);

      if constexpr (use_padded) {
         // Pad the buffer for SWAR
         buffer.resize(buffer.size() + padding_bytes);
      }
      auto p = read_iterators<Opts>(buffer);

      auto it = p.first;
      auto end = p.second;

      auto start = it;

      context ctx{};

      static constexpr auto FinalOpts = use_padded ? is_padded_on<Opts>() : is_padded_off<Opts>();

      if (N == 0) {
         detail::read<Opts.format>::template op<FinalOpts>(value, ctx, it, end);
      }
      else {
         using namespace glz::detail;

         skip_ws<Opts>(ctx, it, end);

         for (size_t I = 0; I < N; ++I) {
            if (bool(ctx.error)) [[unlikely]] {
               break;
            }

            [&] {
               const auto decomposed_key = jmespath::parse_jmespath_token(tokens[I]);

               const auto key = decomposed_key.key;
               if (decomposed_key.is_array_access) {
                  GLZ_MATCH_OPEN_BRACE;

                  while (true) {
                     GLZ_SKIP_WS();
                     GLZ_MATCH_QUOTE;

                     auto* start = it;
                     skip_string_view<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     const sv k = {start, size_t(it - start)};
                     ++it;

                     if (key == k) {
                        GLZ_SKIP_WS();
                        GLZ_MATCH_COLON();
                        GLZ_SKIP_WS();
                        GLZ_MATCH_OPEN_BRACKET;

                        const auto n = decomposed_key.start;
                        if (n) {
                           for (int32_t i = 0; i < n.value(); ++i) {
                              [&] {
                                 skip_value<JSON>::op<Opts>(ctx, it, end);
                                 if (bool(ctx.error)) [[unlikely]] {
                                    return;
                                 }
                                 if (*it != ',') {
                                    ctx.error = error_code::array_element_not_found;
                                    return;
                                 }
                                 ++it;
                              }();
                           }

                           GLZ_SKIP_WS();

                           if (I == (N - 1)) {
                              detail::read<Opts.format>::template op<FinalOpts>(value, ctx, it, end);
                           }
                           return;
                        }
                        else {
                           ctx.error = error_code::array_element_not_found;
                           return;
                        }
                        return;
                     }
                     else {
                        skip_value<JSON>::op<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        if (*it != ',') {
                           ctx.error = error_code::key_not_found;
                           return;
                        }
                        ++it;
                     }
                  }
               }
               else {
                  GLZ_MATCH_OPEN_BRACE;

                  while (it < end) {
                     GLZ_SKIP_WS();
                     GLZ_MATCH_QUOTE;

                     auto* start = it;
                     skip_string_view<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     const sv k = {start, size_t(it - start)};
                     ++it;

                     if (key == k) {
                        GLZ_SKIP_WS();
                        GLZ_MATCH_COLON();
                        GLZ_SKIP_WS();

                        if (I == (N - 1)) {
                           detail::read<Opts.format>::template op<FinalOpts>(value, ctx, it, end);
                        }
                        return;
                     }
                     else {
                        skip_value<JSON>::op<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        if (*it != ',') {
                           ctx.error = error_code::key_not_found;
                           return;
                        }
                        ++it;
                     }
                  }
               }
            }();
         }
      }

      if constexpr (use_padded) {
         // Restore the original buffer state
         buffer.resize(buffer.size() - padding_bytes);
      }

      return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
   }
}
