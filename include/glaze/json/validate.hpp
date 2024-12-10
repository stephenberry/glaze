// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/atoi.hpp"
#include "glaze/json/read.hpp"

namespace glz {
   namespace detail {
      inline constexpr std::array<uint8_t, 256> hex_digits = [] {
         std::array<uint8_t, 256> t{};
         t['0'] = 1;
         t['1'] = 1;
         t['2'] = 1;
         t['3'] = 1;
         t['4'] = 1;
         t['5'] = 1;
         t['6'] = 1;
         t['7'] = 1;
         t['8'] = 1;
         t['9'] = 1;
         t['a'] = 1;
         t['b'] = 1;
         t['c'] = 1;
         t['d'] = 1;
         t['e'] = 1;
         t['f'] = 1;
         t['A'] = 1;
         t['B'] = 1;
         t['C'] = 1;
         t['D'] = 1;
         t['E'] = 1;
         t['F'] = 1;
         return t;
      }();

      template <bool NullTerminated>
      GLZ_ALWAYS_INLINE void skip_whitespace_json(const auto*& it, const auto* end) noexcept {
         if constexpr (NullTerminated) {
            while (*it && whitespace_table[uint8_t(*it)]) ++it;
         } else {
            while (it < end && whitespace_table[uint8_t(*it)]) ++it;
         }
      }
      
      // Helper function to parse four hex digits into a uint16_t code unit
      inline uint16_t parse_hex_4_digits(const char hex[4]) noexcept {
         uint16_t val = 0;
         for (int i = 0; i < 4; ++i) {
            char c = hex[i];
            val <<= 4;
            if (c >= '0' && c <= '9') {
               val |= (c - '0');
            }
            else if (c >= 'a' && c <= 'f') {
               val |= (c - 'a' + 10);
            }
            else {
               val |= (c - 'A' + 10);
            }
         }
         return val;
      }

      template <bool NullTerminated>
      GLZ_ALWAYS_INLINE void validate_json_string(context& ctx, const auto*& it, const auto* end) noexcept {
         if constexpr (NullTerminated) {
            if (*it != '"') [[unlikely]] { ctx.error = error_code::expected_quote; return; }
         } else {
            if (it == end || *it != '"') [[unlikely]] { ctx.error = error_code::expected_quote; return; }
         }
         ++it;

         while (true) {
            if constexpr (NullTerminated) {
               if (*it == '\0') [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
            } else {
               if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
            }
            if (*it == '"') break;
            const auto c = uint8_t(*it);
            if (c < 0x20) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (c == '\\') {
               ++it;
               if constexpr (NullTerminated) {
                  if (*it == '\0') [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
               } else {
                  if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
               }
               const auto esc = *it;
               switch (esc) {
                  case '"': case '\\': case '/': case 'b': case 'f': case 'n': case 'r': case 't':
                     ++it;
                     break;
                  case 'u': {
                     ++it;
                     char hex[4];
                     for (int i = 0; i < 4; ++i) {
                        if constexpr (NullTerminated) {
                           if (*it == '\0' || !hex_digits[uint8_t(*it)]) { ctx.error = error_code::unexpected_end; return; }
                        } else {
                           if (it >= end || !hex_digits[uint8_t(*it)]) { ctx.error = error_code::unexpected_end; return; }
                        }
                        hex[i] = *it++;
                     }

                     uint16_t code_unit = parse_hex_4_digits(hex);

                     // Check if this code unit is a high surrogate
                     if (code_unit >= 0xD800 && code_unit <= 0xDBFF) {
                        // Expect another \u
                        if constexpr (NullTerminated) {
                           if (*it == '\0' || *it != '\\') [[unlikely]] { ctx.error = error_code::syntax_error; return; }
                           ++it;
                           if (*it == '\0' || *it != 'u') [[unlikely]] { ctx.error = error_code::syntax_error; return; }
                           ++it;
                        } else {
                           if (it >= end || *it != '\\') [[unlikely]] { ctx.error = error_code::syntax_error; return; }
                           ++it;
                           if (it >= end || *it != 'u') [[unlikely]] { ctx.error = error_code::syntax_error; return; }
                           ++it;
                        }

                        char hex2[4];
                        for (int i = 0; i < 4; ++i) {
                           if constexpr (NullTerminated) {
                              if (*it == '\0' || !hex_digits[uint8_t(*it)]) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
                           } else {
                              if (it >= end || !hex_digits[uint8_t(*it)]) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
                           }
                           hex2[i] = *it++;
                        }

                        uint16_t code_unit2 = parse_hex_4_digits(hex2);
                        // Check if the second code unit is a low surrogate
                        if (code_unit2 < 0xDC00 || code_unit2 > 0xDFFF) {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                     else if (code_unit >= 0xDC00 && code_unit <= 0xDFFF) {
                        // Low surrogate without a preceding high surrogate is invalid
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     // Otherwise, a single non-surrogate code unit is fine.
                     break;
                  }
                  default:
                     ctx.error = error_code::syntax_error;
                     return;
               }
            } else {
               ++it;
            }
         }

         // now *it == '"'
         ++it;
      }

      // Validate bool ("true" or "false")
      template <bool NullTerminated>
      GLZ_ALWAYS_INLINE void validate_json_bool(context& ctx, const auto*& it, const auto* end) noexcept {
         if constexpr (NullTerminated) {
            if (std::memcmp(it, "true", 4) == 0) {
               it += 4;
               return;
            } else if (std::memcmp(it, "false", 5) == 0) {
               it += 5;
               return;
            }
         } else {
            if ((end - it) >= 4 && std::memcmp(it, "true", 4) == 0) {
               it += 4;
               return;
            } else if ((end - it) >= 5 && std::memcmp(it, "false", 5) == 0) {
               it += 5;
               return;
            }
         }
         ctx.error = error_code::syntax_error;
      }

      template <bool NullTerminated>
      GLZ_ALWAYS_INLINE void validate_json_null(context& ctx, const auto*& it, const auto* end) noexcept {
         if constexpr (NullTerminated) {
            if (std::memcmp(it, "null", 4) == 0) [[likely]] {
               it += 4;
               return;
            }
         } else {
            if ((end - it) >= 4 && std::memcmp(it, "null", 4) == 0) [[likely]] {
               it += 4;
               return;
            }
         }
         ctx.error = error_code::syntax_error;
      }

      template <bool NullTerminated>
      GLZ_ALWAYS_INLINE void validate_number(context& ctx, const auto*& it, const auto* end) noexcept {
         // optional sign
         if constexpr (NullTerminated) {
            if (*it == '-' || *it == '+') ++it;
            // must have digit
            if (*it == '\0' || !is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
         } else {
            if (it < end && (*it == '-' || *it == '+')) ++it;
            if (it == end || !is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
         }

         // leading zero check
         if (*it == '0') {
            ++it;
            if constexpr (NullTerminated) {
               if (is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
            } else {
               if (it < end && is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
            }
         } else {
            // consume digits
            while (true) {
               if constexpr (NullTerminated) {
                  if (!is_digit(*it)) break;
               } else {
                  if (it == end || !is_digit(*it)) break;
               }
               ++it;
            }
         }

         // fraction
         if constexpr (NullTerminated) {
            if (*it == '.') {
               ++it;
               if (!is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
               while (is_digit(*it)) ++it;
            }
         } else {
            if (it < end && *it == '.') {
               ++it;
               if (it == end || !is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
               while (it < end && is_digit(*it)) ++it;
            }
         }

         // exponent
         if constexpr (NullTerminated) {
            if (*it == 'e' || *it == 'E') {
               ++it;
               if (*it == '-' || *it == '+') ++it;
               if (!is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
               while (is_digit(*it)) ++it;
            }
         } else {
            if (it < end && (*it == 'e' || *it == 'E')) {
               ++it;
               if (it < end && (*it == '-' || *it == '+')) ++it;
               if (it == end || !is_digit(*it)) [[unlikely]] { ctx.error = error_code::parse_number_failure; return; }
               while (it < end && is_digit(*it)) ++it;
            }
         }
      }

      // Forward declarations
      template <bool NullTerminated>
      inline void validate_json_value(context& ctx, const auto*& it, const auto* end);

      template <bool NullTerminated>
      inline void validate_json_object(context& ctx, const auto*& it, const auto* end, uint64_t& depth) noexcept {
         if constexpr (NullTerminated) {
            if (*it != '{') [[unlikely]] { ctx.error = error_code::expected_brace; return; }
         } else {
            if (it == end || *it != '{') [[unlikely]] { ctx.error = error_code::expected_brace; return; }
         }

         ++depth;
         ++it;
         skip_whitespace_json<NullTerminated>(it, end);
         if constexpr (NullTerminated) {
            if (*it == '\0') [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
         } else {
            if (it == end) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
         }

         if (*it == '}') {
            ++it;
            --depth;
            return;
         }

         while (true) {
            skip_whitespace_json<NullTerminated>(it, end);
            validate_json_string<NullTerminated>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            skip_whitespace_json<NullTerminated>(it, end);
            if constexpr (NullTerminated) {
               if (*it != ':') { ctx.error = error_code::expected_colon; return; }
            } else {
               if (it == end || *it != ':') { ctx.error = error_code::expected_colon; return; }
            }
            ++it;
            skip_whitespace_json<NullTerminated>(it, end);
            validate_json_value<NullTerminated>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            skip_whitespace_json<NullTerminated>(it, end);
            if constexpr (NullTerminated) {
               if (*it == ',') {
                  ++it;
                  continue;
               } else if (*it == '}') {
                  ++it;
                  skip_whitespace_json<NullTerminated>(it, end);
                  if (*it && *it != ',' && *it != ']' && *it != '}') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  --depth;
                  return;
               } else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            } else {
               if (it < end && *it == ',') {
                  ++it;
                  continue;
               } else if (it < end && *it == '}') {
                  ++it;
                  skip_whitespace_json<NullTerminated>(it, end);
                  if (it < end && *it != ',' && *it != ']' && *it != '}') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  --depth;
                  return;
               } else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
         
         unreachable();
      }

      template <bool NullTerminated>
      inline void validate_json_array(context& ctx, const auto*& it, const auto* end, uint64_t& depth) noexcept {
         if constexpr (NullTerminated) {
            if (*it != '[') [[unlikely]] { ctx.error = error_code::expected_bracket; return; }
         } else {
            if (it == end || *it != '[') [[unlikely]] { ctx.error = error_code::expected_bracket; return; }
         }
         ++depth;
         ++it;
         skip_whitespace_json<NullTerminated>(it, end);
         if constexpr (NullTerminated) {
            if (*it == '\0') [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
         } else {
            if (it == end) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
         }

         if (*it == ']') {
            ++it;
            --depth;
            return;
         }

         while (true) {
            skip_whitespace_json<NullTerminated>(it, end);
            validate_json_value<NullTerminated>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            skip_whitespace_json<NullTerminated>(it, end);
            if constexpr (NullTerminated) {
               if (*it == ',') {
                  ++it;
                  continue;
               } else if (*it == ']') {
                  ++it;
                  skip_whitespace_json<NullTerminated>(it, end);
                  if (*it && *it != ',' && *it != ']' && *it != '}') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  --depth;
                  return;
               } else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            } else {
               if (it < end && *it == ',') {
                  ++it;
                  continue;
               } else if (it < end && *it == ']') {
                  ++it;
                  skip_whitespace_json<NullTerminated>(it, end);
                  if (it < end && *it != ',' && *it != ']' && *it != '}') {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  --depth;
                  return;
               } else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
         
         unreachable();
      }

      // Validate a generic value
      template <bool NullTerminated>
      inline void validate_json_value(context& ctx, const auto*& it, const auto* end) {
         skip_whitespace_json<NullTerminated>(it, end);
         if constexpr (NullTerminated) {
            if (*it == '\0') { ctx.error = error_code::syntax_error; return; }
         } else {
            if (it == end) { ctx.error = error_code::syntax_error; return; }
         }

         const auto c = *it;
         uint64_t depth = 0;
         switch (c)
         {
            case '{': {
               validate_json_object<NullTerminated>(ctx, it, end, depth);
               break;
            }
            case '[': {
               validate_json_array<NullTerminated>(ctx, it, end, depth);
               break;
            }
            case '"': {
               validate_json_string<NullTerminated>(ctx, it, end);
               break;
            }
            case 't': {
               [[fallthrough]];
            }
            case 'f': {
               validate_json_bool<NullTerminated>(ctx, it, end);
               break;
            }
            case 'n': {
               validate_json_null<NullTerminated>(ctx, it, end);
               break;
            }
            default: {
               // number possibility
               if (c == '-' || c == '+' || is_digit(c)) {
                  validate_number<NullTerminated>(ctx, it, end);
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
      }

      template <auto Opts, class In>
      [[nodiscard]] error_ctx validate_json(is_context auto&& ctx, In&& in) noexcept {
         if constexpr (resizable<In>) {
            in.resize(in.size() + padding_bytes);
         }

         const auto* start = in.data();
         const auto* end = start + in.size();
         const auto* it = start;

         if (bool(ctx.error)) [[unlikely]] {
            return {ctx.error, ctx.custom_error_message, 0, ctx.includer_error};
         }

         if constexpr (string_t<In>) {
            validate_json_value<true>(ctx, it, end);
         } else {
            validate_json_value<false>(ctx, it, end);
         }
         
         if (bool(ctx.error)) {
            return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
         }

         if constexpr (resizable<In>) {
            while (*it && whitespace_table[uint8_t(*it)]) ++it;
            if (*it != '\0') [[unlikely]] {
               ctx.error = error_code::syntax_error;
            }
         } else {
            while (it < end && whitespace_table[uint8_t(*it)]) ++it;
            if (it < end) [[unlikely]] {
               ctx.error = error_code::syntax_error;
            }
         }

         error_ctx result{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};

         if constexpr (resizable<In>) {
            in.resize(in.size() - padding_bytes);
         }

         return result;
      }
   }

   template <opts Opts = opts{}>
   [[nodiscard]] error_ctx validate_json(resizable auto& in) noexcept {
      context ctx{};
      return detail::validate_json<Opts>(ctx, in);
   }

   template <opts Opts = opts{}>
   [[nodiscard]] error_ctx validate_json(std::string_view in) noexcept {
      context ctx{};
      return detail::validate_json<Opts>(ctx, in);
   }
}
