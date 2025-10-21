// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cctype>
#include <charconv>
#include <iostream>

#include "glaze/core/common.hpp"
// #include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/toml/common.hpp"
#include "glaze/toml/opts.hpp"
#include "glaze/toml/skip.hpp"
#include "glaze/util/glaze_fast_float.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   template <>
   struct parse<TOML>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end)
      {
         using V = std::remove_cvref_t<T>;
         from<TOML, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                          std::forward<It1>(end));
      }
   };

   // Parse TOML key (bare key or quoted key)
   template <class Ctx, class It, class End>
   GLZ_ALWAYS_INLINE bool parse_toml_key(std::string& key, Ctx& ctx, It&& it, End&& end) noexcept
   {
      key.clear();
      skip_ws_and_comments(it, end);

      if (it == end) {
         ctx.error = error_code::unexpected_end;
         return false;
      }

      if (*it == '"') {
         // Quoted key
         ++it;
         while (it != end && *it != '"') {
            if (*it == '\\') {
               ++it;
               if (it == end) {
                  ctx.error = error_code::unexpected_end;
                  return false;
               }
               switch (*it) {
               case '"':
                  key.push_back('"');
                  break;
               case '\\':
                  key.push_back('\\');
                  break;
               case 'n':
                  key.push_back('\n');
                  break;
               case 't':
                  key.push_back('\t');
                  break;
               case 'r':
                  key.push_back('\r');
                  break;
               default:
                  key.push_back('\\');
                  key.push_back(*it);
                  break;
               }
            }
            else {
               key.push_back(*it);
            }
            ++it;
         }

         if (it == end || *it != '"') {
            ctx.error = error_code::syntax_error;
            return false;
         }
         ++it; // Skip closing quote
      }
      else {
         // Bare key
         while (it != end && (std::isalnum(*it) || *it == '_' || *it == '-')) {
            key.push_back(*it);
            ++it;
         }

         if (key.empty()) {
            ctx.error = error_code::syntax_error;
            return false;
         }
      }

      return true;
   }

   template <class Ctx, class It, class End>
   GLZ_ALWAYS_INLINE bool parse_toml_key(std::vector<std::string>& keys, Ctx& ctx, It& it, End end) noexcept
   {
      keys.clear();
      skip_ws_and_comments(it, end);

      if (it == end) {
         ctx.error = error_code::unexpected_end;
         return false;
      }

      while (true) {
         std::string key;

         if (it == end) {
            ctx.error = error_code::unexpected_end;
            return false;
         }

         if (*it == '"') {
            ++it;
            while (it != end && *it != '"') {
               if (*it == '\\') {
                  ++it;
                  if (it == end) {
                     ctx.error = error_code::unexpected_end;
                     return false;
                  }
                  switch (*it) {
                  case '"':
                     key.push_back('"');
                     break;
                  case '\\':
                     key.push_back('\\');
                     break;
                  case 'n':
                     key.push_back('\n');
                     break;
                  case 't':
                     key.push_back('\t');
                     break;
                  case 'r':
                     key.push_back('\r');
                     break;
                  default:
                     key.push_back('\\');
                     key.push_back(*it);
                     break;
                  }
               }
               else {
                  key.push_back(*it);
               }
               ++it;
            }

            if (it == end || *it != '"') {
               ctx.error = error_code::syntax_error;
               return false;
            }
            ++it; // skip closing quote
         }
         else {
            while (it != end && (std::isalnum(*it) || *it == '_' || *it == '-')) {
               key.push_back(*it);
               ++it;
            }

            if (key.empty()) {
               ctx.error = error_code::syntax_error;
               return false;
            }
         }

         keys.push_back(std::move(key));

         skip_ws_and_comments(it, end);

         if (it == end || *it != '.') {
            break;
         }

         ++it; // skip '.', We know it's '.' because we've checked before
         skip_ws_and_comments(it, end);

         if (it == end) {
            ctx.error = error_code::unexpected_end;
            return false;
         }
      }

      return true;
   }

   template <glaze_value_t T>
   struct from<TOML, T>
   {
      template <auto Opts, is_context Ctx, class It0, class It1>
      static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end)
      {
         using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
         from<TOML, V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx),
                                          std::forward<It0>(it), std::forward<It1>(end));
      }
   };

   namespace detail
   {
      constexpr bool is_toml_base_specifier(char c) noexcept { return (c == 'x' || c == 'b' || c == 'o'); }

      constexpr int toml_specified_base(char c) noexcept
      {
         switch (c) {
         case 'x':
            return 16;
         case 'b':
            return 2;
         case 'o':
            return 8;
         }

         // Should never reach here, but seems a good default.
         return 10;
      }

      constexpr bool is_any_toml_digit(char c) noexcept
      {
         if (c >= '0' && c <= '9') {
            return true;
         }

         if (c >= 'a' && c <= 'f') {
            return true;
         }

         if (c >= 'A' && c <= 'F') {
            return true;
         }

         return false;
      }

      constexpr bool is_valid_toml_digit(char c, int base) noexcept
      {
         if (base <= 10) {
            return c >= '0' && c < ('0' + base);
         }

         return is_any_toml_digit(c);
      }

      struct unsigned_accumulator
      {
        private:
         int base = 10;

        public:
         constexpr void inform_base(int base) noexcept { this->base = base; }

         constexpr bool is_valid_digit(char c) const noexcept { return is_valid_toml_digit(c, this->base); }

         template <std::unsigned_integral T>
         constexpr bool try_accumulate(T& v, int digit) const noexcept
         {
            if (digit < 0 || digit >= this->base) [[unlikely]] {
               return false;
            }

            // Check overflow.
            const auto max_without_overflow = ((std::numeric_limits<T>::max)() - digit) / this->base;
            if (v > max_without_overflow) [[unlikely]] {
               return false;
            }

            v = v * this->base + digit;

            return true;
         }
      };

      struct signed_accumulator
      {
        private:
         int base = 10;
         bool negated = false;

        public:
         constexpr void inform_negated() noexcept { this->negated = true; }

         constexpr void inform_base(int base) noexcept { this->base = base; }

         constexpr bool is_valid_digit(char c) const noexcept { return is_valid_toml_digit(c, this->base); }

         template <std::signed_integral T>
         constexpr bool try_accumulate(T& v, int digit) const noexcept
         {
            if (digit < 0 || digit >= this->base) [[unlikely]] {
               return false;
            }

            if (this->negated) {
               // Check underflow.
               const auto min_without_underflow = ((std::numeric_limits<T>::min)() + digit) / this->base;
               if (v < min_without_underflow) [[unlikely]] {
                  return false;
               }

               v = v * this->base - digit;

               return true;
            }

            // Check overflow.
            const auto max_without_overflow = ((std::numeric_limits<T>::max)() - digit) / this->base;
            if (v > max_without_overflow) [[unlikely]] {
               return false;
            }

            v = v * this->base + digit;

            return true;
         }
      };

      template <std::integral T>
      constexpr bool parse_toml_integer(T& v, auto&& it, auto&& end) noexcept
      {
         // Ensure the caller gave us at least one character.
         if (it == end) [[unlikely]] {
            return false;
         }

         auto accumulator = [] {
            if constexpr (std::unsigned_integral<T>) {
               return unsigned_accumulator{};
            }
            else {
               return signed_accumulator{};
            }
         }();

         bool saw_negative_sign = false;
         bool saw_sign = false; // Tracks whether +/- was supplied so prefixed bases can reject it.

         if (*it == '-') {
            saw_negative_sign = true;
            saw_sign = true;
            ++it;

            if (it == end) [[unlikely]] {
               return false;
            }

            if constexpr (std::signed_integral<T>) {
               accumulator.inform_negated();
            }
         }

         if (*it == '+') {
            // Only a single leading sign is permitted.
            if (saw_sign) [[unlikely]] {
               return false;
            }

            ++it;
            saw_sign = true;

            if (it == end) [[unlikely]] {
               return false;
            }
         }

         if (it == end) [[unlikely]] {
            return false;
         }

         if (*it == '_') [[unlikely]] {
            return false;
         }

         int base = 10;
         bool base_overridden = false;

         if (*it == '0') {
            auto next = it;
            ++next;

            if (next == end) {
               it = next;
               v = 0;
               return true;
            }

            if (*next == '_') [[unlikely]] {
               return false;
            }

            if (is_toml_base_specifier(*next)) {
               // Switching to hexadecimal/octal/binary after 0.
               base = toml_specified_base(*next);
               base_overridden = true;

               it = next;
               ++it;

               if (it == end) [[unlikely]] {
                  return false;
               }

               if (not is_valid_toml_digit(*it, base)) [[unlikely]] {
                  return false;
               }
            }
            else if (is_any_toml_digit(*next)) [[unlikely]] {
               // Decimal literals like 0123 are invalid.
               return false;
            }
            else {
               it = next;
               v = 0;
               return true;
            }
         }

         if (base_overridden) {
            accumulator.inform_base(base);

            // Only non-negative values are allowed for prefixed bases.
            if (saw_sign) [[unlikely]] {
               return false;
            }
         }

         v = 0;
         bool saw_digit = false;

         // Consume digits (with optional underscores) while respecting overflow limits.
         while (true) {
            if (it == end) {
               break;
            }

            if (*it == '_') {
               ++it;

               if (it == end) [[unlikely]] {
                  return false;
               }

               if (not is_any_toml_digit(*it)) [[unlikely]] {
                  return false;
               }
            }
            else if (not is_any_toml_digit(*it)) {
               break;
            }

            const auto digit = char_to_digit(*it);

            if (not accumulator.try_accumulate(v, digit)) {
               return false;
            }

            saw_digit = true;
            ++it;
         }

         if (not saw_digit) [[unlikely]] {
            return false;
         }

         if constexpr (std::unsigned_integral<T>) {
            if (saw_negative_sign && v != 0) [[unlikely]] {
               return false;
            }
         }

         return true;
      }
   }

   template <num_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         skip_ws_and_comments(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         using V = decay_keep_volatile_t<decltype(value)>;
         if constexpr (int_t<V>) {
            if (not detail::parse_toml_integer(value, it, end)) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
         }
         else {
            auto [ptr, ec] = glz::from_chars<false>(it, end, value); // Always treat as non-null-terminated
            if (ec != std::errc()) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            it = ptr;
         }
      }
   };

   template <string_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         value.clear();
         skip_ws_and_comments(it, end);

         if (it == end) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (it + 2 < end && *it == '"' && *(it + 1) == '"' && *(it + 2) == '"') {
            // Basic Multiline String
            it += 3; // Skip """
            if (it != end && *it == '\n') { // Skip initial newline
               ++it;
            }
            else if (it + 1 < end && *it == '\r' && *(it + 1) == '\n') { // Skip initial CRLF
               it += 2;
            }

            while (it + 2 < end && !(*it == '"' && *(it + 1) == '"' && *(it + 2) == '"')) {
               if (*it == '\\') {
                  ++it;
                  if (it == end) {
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
                  case 'b':
                     value.push_back('\b');
                     break;
                  case 'f':
                     value.push_back('\f');
                     break;
                  // TOML: Any other character is an error for escape sequences in basic strings
                  // However, we also need to handle escaped newlines for line trimming
                  case '\n': /* ignore escaped newline */
                     // Trim all whitespace after escaped newline until non-whitespace or actual newline
                     while (it + 1 < end &&
                            (*(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\r' || *(it + 1) == '\n')) {
                        ++it;
                        if (*it == '\n') break; // Stop if we hit an actual newline
                     }
                     break;
                  case '\r': // part of CRLF, handle similar to \n
                     if (it + 1 < end && *(it + 1) == '\n') ++it;
                     while (it + 1 < end &&
                            (*(it + 1) == ' ' || *(it + 1) == '\t' || *(it + 1) == '\r' || *(it + 1) == '\n')) {
                        ++it;
                        if (*it == '\n') break;
                     }
                     break;
                  default:
                     // In TOML, an unknown escape sequence is an error.
                     // For simplicity here, we might just append them or flag an error.
                     // Glaze JSON parser often appends, let's be stricter for TOML.
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               else {
                  value.push_back(*it);
               }
               ++it;
            }

            if (it + 2 >= end || !(*it == '"' && *(it + 1) == '"' && *(it + 2) == '"')) {
               ctx.error = error_code::syntax_error; // Unterminated multiline string
               return;
            }
            it += 3; // Skip closing """
         }
         else if (*it == '"') {
            // Basic string
            ++it; // Skip opening quote

            while (it != end && *it != '"') {
               if (*it == '\\') {
                  ++it;
                  if (it == end) {
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
                  case 'b':
                     value.push_back('\b');
                     break;
                  case 'f':
                     value.push_back('\f');
                     break;
                  default:
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               else if (*it == '\n' || *it == '\r') { // Newlines not allowed in single-line basic strings
                  ctx.error = error_code::syntax_error;
                  return;
               }
               else {
                  value.push_back(*it);
               }
               ++it;
            }

            if (it == end || *it != '"') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it; // Skip closing quote
         }
         else if (it + 2 < end && *it == '\'' && *(it + 1) == '\'' && *(it + 2) == '\'') {
            // Literal Multiline String
            it += 3; // Skip '''
            if (it != end && *it == '\n') { // Skip initial newline
               ++it;
            }
            else if (it + 1 < end && *it == '\r' && *(it + 1) == '\n') { // Skip initial CRLF
               it += 2;
            }

            while (it + 2 < end && !(*it == '\'' && *(it + 1) == '\'' && *(it + 2) == '\'')) {
               value.push_back(*it);
               ++it;
            }

            if (it + 2 >= end || !(*it == '\'' && *(it + 1) == '\'' && *(it + 2) == '\'')) {
               ctx.error = error_code::syntax_error; // Unterminated multiline literal string
               return;
            }
            it += 3; // Skip closing '''
         }
         else if (*it == '\'') {
            // Literal string
            ++it; // Skip opening quote

            while (it != end && *it != '\'') {
               if (*it == '\n' || *it == '\r') { // Newlines not allowed in single-line literal strings
                  ctx.error = error_code::syntax_error;
                  return;
               }
               value.push_back(*it);
               ++it;
            }

            if (it == end || *it != '\'') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it; // Skip closing quote
         }
         else {
            // Bare string (not typical in TOML values, but handle it if necessary for some edge cases)
            // TOML spec does not allow bare strings for values. This path should ideally not be hit for valid TOML.
            // If it is, it's likely an error or a deviation.
            // For now, let's assume this is an error according to strict TOML.
            ctx.error = error_code::syntax_error;
            return;
            /*
            while (it != end && *it != '\n' && *it != '\r' && *it != '#') {
               value.push_back(*it);
               ++it;
            }

            // Trim trailing whitespace
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
               value.pop_back();
            }
            */
         }
      }
   };

   template <bool_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         skip_ws_and_comments(it, end);

         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (it + 4 <= end &&
             std::string_view(it, 4) == "true") { // TODO: Maybe we can use the bool_true from header.hpp
            value = true;
            it += 4;
         }
         else if (it + 5 <= end &&
                  std::string_view(it, 5) == "false") { // TODO: Maybe we can use the bool_false from header.hpp
            value = false;
            it += 5;
         }
         else {
            ctx.error = error_code::expected_true_or_false;
            return;
         }
      }
   };

   template <readable_array_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         skip_ws_and_comments(it, end);

         if (it == end || *it != '[') {
            ctx.error = error_code::syntax_error;
            return;
         }

         ++it; // Skip '['
         skip_ws_and_comments(it, end);

         // Handle empty array
         if (it != end && *it == ']') {
            ++it;
            return;
         }

         size_t index = 0;
         while (it != end) {
            using value_type = typename std::remove_cvref_t<T>::value_type;

            if constexpr (emplace_backable<T>) {
               auto& element = value.emplace_back();
               from<TOML, value_type>::template op<Opts>(element, ctx, it, end);
            }
            else {
               // For fixed-size arrays like std::array
               if (index >= value.size()) {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }
               from<TOML, value_type>::template op<Opts>(value[index], ctx, it, end);
               ++index;
            }

            if (bool(ctx.error)) {
               return;
            }

            skip_ws_and_comments(it, end);

            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if (*it == ']') {
               ++it;
               break;
            }
            else if (*it == ',') {
               ++it;
               skip_ws_and_comments(it, end);
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
      }
   };

   namespace detail
   {
      template <auto Opts, class T, class It, class End, class Ctx>
      GLZ_ALWAYS_INLINE void parse_toml_object_members(T&& value, It&& it, End&& end, Ctx&& ctx, bool is_inline_table)
      {
         using U = std::remove_cvref_t<T>;
         static constexpr auto N = reflect<U>::size;
         static constexpr auto HashInfo = hash_info<U>;
         // TODO: Think if we should make inline table a constexpr to reduce runtime checks
         // TODO: Think if it's feasible to write a function to find out the deepest nested structure
         // and use it instead of vector

         while (it != end) {
            skip_ws_and_comments(it, end);

            if (it == end) {
               if (is_inline_table) ctx.error = error_code::unexpected_end; // Inline table must end with '}'
               break;
            }

            if (is_inline_table && *it == '}') {
               ++it; // Consume '}'
               return; // End of inline table
            }

            // Skip empty lines (only if not in an inline table, inline tables don't have newlines)
            if (!is_inline_table && (*it == '\n' || *it == '\r')) {
               skip_to_next_line(ctx, it, end);
               continue;
            }

            std::string key_str;
            // std::vector<std::string> key_str; // TODO: std::string is used temporarily, we may swap it to view later
            // on or as said before completely switch to array

            // Handle section headers [section] (only if not in an inline table)
            if (!is_inline_table && *it == '[') {
               // For now, skip section headers - we'll implement nested object support later
               // Or, this could be where we handle table arrays or nested tables.
               // For the current task, we are focusing on inline tables.
               // This part might need to be more sophisticated for full TOML table support.
               skip_to_next_line(ctx, it, end);
               continue;
            }

            if (!parse_toml_key(key_str, ctx, it, end)) {
               return;
            }

            skip_ws_and_comments(it, end);

            if (it == end || *it != '=') {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it; // Skip '='
            skip_ws_and_comments(it, end);

            if (it == end) {
               ctx.error = error_code::unexpected_end; // Value expected
               return;
            }

            const auto index = decode_hash_with_size<TOML, U, HashInfo, HashInfo.type>::op(
               key_str.data(), key_str.data() + key_str.size(), key_str.size());

            const bool key_matches = index < N && key_str == reflect<U>::keys[index];

            if (key_matches) [[likely]] {
               visit<N>(
                  [&]<size_t I>() {
                     if (I == index) {
                        decltype(auto) member_obj = [&]() -> decltype(auto) {
                           if constexpr (reflectable<U>) {
                              // For reflectable types, to_tie provides access to members
                              return get<I>(to_tie(value));
                           }
                           else {
                              // For glaze_object_t, reflect<T>::values gives member metadata
                              // and get_member accesses the actual member value
                              return get_member(value, get<I>(reflect<U>::values));
                           }
                        }();

                        // member_obj is now a reference to the actual member or its wrapper
                        using member_type = std::decay_t<decltype(member_obj)>;
                        from<TOML, member_type>::template op<Opts>(member_obj, ctx, it, end);
                     }
                     return !bool(ctx.error);
                  },
                  index);

               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
            }
            else {
               if constexpr (Opts.error_on_unknown_keys) {
                  ctx.error = error_code::unknown_key;
                  return;
               }

               skip_value<TOML>::template op<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
            }

            skip_ws_and_comments(it, end);
            if (it == end) {
               if (is_inline_table) ctx.error = error_code::unexpected_end; // Inline table must end with '}'
               break;
            }

            if (is_inline_table) {
               if (*it == '}') {
                  // Handled at the start of the loop
                  continue;
               }
               else if (*it == ',') {
                  ++it; // Consume comma
                  skip_ws_and_comments(it, end);
                  if (it != end && *it == '}') { // Trailing comma case like { key = "val", }
                     // This is allowed by TOML v1.0.0 for inline tables
                     // The '}' will be consumed at the start of the next iteration.
                  }
               }
               else {
                  ctx.error = error_code::syntax_error; // Expected comma or '}'
                  return;
               }
            }
            else {
               // For top-level or standard tables, expect newline or EOF
               if (it != end && (*it == '\n' || *it == '\r')) {
                  skip_to_next_line(ctx, it, end);
               }
               else if (it != end && *it != '#') { // If not a comment, it's an error unless it's EOF
                  // Could be an issue if there's no newline before EOF for the last key-value
               }
            }
         }
      }
   }

   template <auto Opts, class T>
   GLZ_ALWAYS_INLINE bool resolve_nested(T& root, std::span<std::string> path, auto& ctx, auto& it, auto& end)
   {
      if constexpr (!(glz::reflectable<T> || glz::glaze_object_t<T>)) {
         return true;
      }
      else {
         using U = std::remove_cvref_t<T>;
         static constexpr auto N = reflect<U>::size;
         static constexpr auto HashInfo = hash_info<U>;
         const auto index = decode_hash_with_size<TOML, U, HashInfo, HashInfo.type>::op(
            path.front().data(), path.front().data() + path.front().size(), path.front().size());

         const bool key_matches = index < N && path.front() == reflect<U>::keys[index];

         if (key_matches) [[likely]] {
            visit<N>(
               [&]<size_t I>() {
                  if (I == index) {
                     decltype(auto) member_obj = [&]() -> decltype(auto) {
                        if constexpr (reflectable<U>) {
                           return get<I>(to_tie(root));
                        }
                        else {
                           return get_member(root, get<I>(reflect<U>::values));
                        }
                     }();

                     if (!(path.size() - 1)) {
                        using member_type = std::decay_t<decltype(member_obj)>;
                        from<TOML, member_type>::template op<toml::is_internal_on<Opts>()>(member_obj, ctx, it, end);
                     }
                     else {
                        return resolve_nested<Opts>(member_obj, path.subspan(1), ctx, it, end);
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

            skip_value<TOML>::template op<Opts>(ctx, it, end);
            return !bool(ctx.error);
         }
         return !bool(ctx.error);
      }
   }

   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && not custom_read<T>)
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
      {
         while (it != end) {
            // TODO: Introduce OPTS here
            skip_ws_and_comments(it, end);
            if (it == end) { // Empty input for an object is valid (empty object)
               return;
            }
            if (*it == '\n' || *it == '\r') {
               skip_to_next_line(ctx, it, end);
               continue;
            }

            // TODO: We probably should reorder that so that we dont check against less used inline table more often
            if (*it == '{') { // Check if it's an inline table
               ++it; // Consume '{'
               skip_ws_and_comments(it, end);
               if (it != end && *it == '}') { // Empty inline table {}
                  ++it;
                  return;
               }
               // TODO: Rewrite logic here, for now it works just fine so we leave it.
               detail::parse_toml_object_members<Opts>(value, it, end, ctx, true); // true for is_inline_table
               // The parse_toml_object_members should consume the final '}' if successful
            }
            else if (*it == '[') { // Normal table
               std::vector<std::string> path;

               if constexpr (toml::check_is_internal(Opts)) {
                  return; // is it's internal we return to root.
               }
               else {
                  ++it;
                  if (*it == '[') { // Array of tables

                     ++it; // skip the second bracket
                     ctx.error = error_code::feature_not_supported;
                     return;
                     // TODO: the logic should ideally branch here, because arrays should ignore multiple defenition
                     // errors And should also use push back version but for now we just error out, will support
                     // later
                  }
                  skip_ws_and_comments(it, end);

                  if (!parse_toml_key(path, ctx, it, end)) {
                     return;
                  }
                  if (it == end || *it != ']') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  it++; // skip ']'
                  if (!resolve_nested<Opts>(value, std::span{path}, ctx, it, end)) {
                     return;
                  }
               }
            }
            else {
               std::vector<std::string> path;
               skip_ws_and_comments(it, end);

               if (!parse_toml_key(path, ctx, it, end)) {
                  return;
               }

               if (it == end || *it != '=') {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it; // Skip '='
               skip_ws_and_comments(it, end);

               if (it == end) {
                  ctx.error = error_code::unexpected_end; // Value expected
                  return;
               }
               if (!resolve_nested<Opts>(value, std::span{path}, ctx, it, end)) {
                  return;
               }
            }
         }
      }
   };

   template <read_supported<TOML> T, is_buffer Buffer>
   [[nodiscard]] inline error_ctx read_toml(T& value, Buffer&& buffer)
   {
      context ctx{};
      return read<opts{.format = TOML}>(value, std::forward<Buffer>(buffer), ctx);
   }

   template <read_supported<TOML> T, is_buffer Buffer>
   [[nodiscard]] inline expected<T, error_ctx> read_toml(Buffer&& buffer)
   {
      T value{};
      context ctx{};
      const error_ctx ec = read<opts{.format = TOML}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return value;
   }

   template <read_supported<TOML> T, is_buffer Buffer>
   [[nodiscard]] inline error_ctx read_file_toml(T& value, const sv file_name, Buffer&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {ec};
      }

      return read<opts{.format = TOML}>(value, buffer, ctx);
   }
}
