// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstring>
#include <iterator>
#include <span>

#include "glaze/api/name.hpp"
#include "glaze/util/string_view.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/expected.hpp"
#include "glaze/core/context.hpp"
#include "glaze/util/string_literal.hpp"

#if defined(__clang__) || defined(__GNUC__)
   #ifndef GLZ_USE_ALWAYS_INLINE
      #define GLZ_USE_ALWAYS_INLINE
   #endif
#endif

#if defined(GLZ_USE_ALWAYS_INLINE) && defined(NDEBUG)
   #ifndef GLZ_ALWAYS_INLINE
      #define GLZ_ALWAYS_INLINE inline __attribute__((always_inline))
   #endif
#endif

#ifndef GLZ_ALWAYS_INLINE
   #define GLZ_ALWAYS_INLINE inline
#endif

#if defined(__clang__) && defined(NDEBUG)
   #ifndef GLZ_FLATTEN
      #define GLZ_FLATTEN inline __attribute__((flatten))
   #endif
#endif

#ifndef GLZ_FLATTEN
   #define GLZ_FLATTEN inline
#endif

namespace glz::detail
{
   // assumes null terminated
   template <char c>
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it) noexcept
   {
      if (*it != c) [[unlikely]] {
         ctx.error = error_code::syntax_error;
      }
      else [[likely]] {
         ++it;
      }
   }

   template <string_literal str>
   GLZ_ALWAYS_INLINE void match(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      const auto n = static_cast<size_t>(std::distance(it, end));
      if ((n < str.size) || (std::memcmp(it, str.value, str.size) != 0)) [[unlikely]] {
         ctx.error = error_code::syntax_error;
      }
      else {
         it += str.size;
      }
   }
   
   GLZ_ALWAYS_INLINE void skip_comment(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
      
      ++it;
      if (it == end) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
      }
      else if (*it == '/') {
         while (++it != end && *it != '\n')
            ;
      }
      else if (*it == '*') {
         while (++it != end) {
            if (*it == '*') [[unlikely]] {
               if (++it == end) [[unlikely]]
                  break;
               else if (*it == '/') [[likely]] {
                  ++it;
                  break;
               }
            }
         }
      }
      else [[unlikely]] {
         ctx.error = error_code::expected_end_comment;
      }
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_ws(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
      
      while (true) {
         switch (*it)
         {
            case '\t':
            case '\n':
            case '\r':
            case ' ':
               ++it;
               break;
            case '/': {
               if constexpr (Opts.force_conformance) {
                  ctx.error = error_code::syntax_error;
               }
               else {
                  skip_comment(ctx, it, end);
                  break;
               }
            }
            default:
               return;
         }
      }
   }
   
   GLZ_ALWAYS_INLINE void skip_ws_no_comments(auto&& it) noexcept
   {
      while (true) {
         switch (*it)
         {
            case '\t':
            case '\n':
            case '\r':
            case ' ':
               ++it;
               break;
            default:
               return;
         }
      }
   }

   GLZ_ALWAYS_INLINE void skip_till_escape_or_quote(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);
      
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }

      auto has_zero = [](uint64_t chunk) { return (((chunk - 0x0101010101010101) & ~chunk) & 0x8080808080808080); };

      auto has_qoute = [&](uint64_t chunk) {
         return has_zero(chunk ^ 0b0010001000100010001000100010001000100010001000100010001000100010);
      };

      auto has_escape = [&](uint64_t chunk) {
         return has_zero(chunk ^ 0b0101110001011100010111000101110001011100010111000101110001011100);
      };

      const auto end_m7 = end - 7;
      for (; it < end_m7; it += 8) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         uint64_t test_chars = has_qoute(chunk) | has_escape(chunk);
         if (test_chars != 0) {
               it += (std::countr_zero(test_chars) >> 3);
            return;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         switch (*it) {
         case '\\':
         case '"':
            return;
         }
         ++it;
      }
      
      ctx.error = error_code::expected_quote;
   }
   
   GLZ_ALWAYS_INLINE void skip_till_quote(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
      
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      auto has_zero = [](uint64_t chunk) { return (((chunk - 0x0101010101010101) & ~chunk) & 0x8080808080808080); };

      auto has_qoute = [&](uint64_t chunk) {
         return has_zero(chunk ^ 0b0010001000100010001000100010001000100010001000100010001000100010);
      };

      const auto end_m7 = end - 7;
      for (; it < end_m7; it += 8) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         uint64_t char_test = has_qoute(chunk);
         if (char_test != 0) {
            it += (std::countr_zero(char_test) >> 3);
            return;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         switch (*it) {
            case '"': {
               return;
            }
         }
         ++it;
      }
      ctx.error = error_code::expected_quote;
   }
   
   // very similar code to skip_till_quote, but it consumes the iterator and returns the key
   [[nodiscard]] GLZ_ALWAYS_INLINE const sv parse_unescaped_key(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] {
         return {};
      }
      
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      auto has_zero = [](uint64_t chunk) { return (((chunk - 0x0101010101010101) & ~chunk) & 0x8080808080808080); };

      auto has_qoute = [&](uint64_t chunk) {
         return has_zero(chunk ^ 0b0010001000100010001000100010001000100010001000100010001000100010);
      };
      
      auto start = it;

      const auto end_m7 = end - 7;
      for (; it < end_m7; it += 8) {
         uint64_t chunk;
         std::memcpy(&chunk, it, 8);
         uint64_t test_chars = has_qoute(chunk);
         if (test_chars != 0) {
            it += (std::countr_zero(test_chars) >> 3);
            
            sv ret{ start, static_cast<size_t>(it - start) };
            ++it;
            return ret;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (it < end) {
         switch (*it) {
            case '"': {
               sv ret{ start, static_cast<size_t>(it - start) };
               ++it;
               return ret;
            }
         }
         ++it;
      }
      ctx.error = error_code::expected_quote;
      return {};
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_string(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }

      ++it;

      if constexpr (Opts.force_conformance) {
         while (true) {
            switch (*it) {
            case '"':
               ++it;
               return;
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t': {
               ctx.error = error_code::syntax_error;
               return;
            }
            case '\\': {
               ++it;
               switch (*it) {
                  case '"':
                  case '\\':
                  case '/':
                  case 'b':
                  case 'f':
                  case 'n':
                  case 'r':
                  case 't': {
                     ++it;
                     continue;
                  }
                  case 'u': {
                     ++it;
                     if ((end - it) < 4) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     else if (std::all_of(it, it + 4, ::isxdigit)) [[likely]] {
                        it += 4;
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     continue;
                  }
                  [[unlikely]] default: {
                     ctx.error = error_code::syntax_error;
                     return;
                  }   
               }
            }
            [[likely]] default :
               ++it;
            }
         }
      }
      else {
         while (it < end) {
            if (*it == '"') {
               ++it;
               break;
            }
            else if (*it == '\\' && ++it == end) [[unlikely]]
               break;
            ++it;
         }
      }
   }

   template <char open, char close>
   GLZ_ALWAYS_INLINE void skip_until_closed(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }

      ++it;
      size_t open_count = 1;
      size_t close_count = 0;
      while (true) {
         switch (*it) {
         case '\0':
            ctx.error = error_code::unexpected_end;
            return;
         case '/':
            skip_comment(ctx, it, end);
            break;
         case '"':
            skip_string<opts{}>(ctx, it, end);
            break;
         case open:
            ++open_count;
            ++it;
            break;
         case close:
            ++close_count;
            ++it;
            if (close_count >= open_count) {
               return;
            }
            break;
         default:
            ++it;
         }
      }
   }
   
   GLZ_ALWAYS_INLINE constexpr bool is_numeric(const auto c) noexcept
   {
      switch (c) {
      case '0': case '1': case '2': case '3': //
      case '4': case '5': case '6': case '7': //
      case '8': case '9': //
      case '.': case '+': case '-': //
      case 'e': case 'E': //
         return true;
      }
      return false;
   }

   GLZ_ALWAYS_INLINE constexpr bool is_digit(char c) noexcept { return c <= '9' && c >= '0'; }

   // TODO: don't recurse
   inline constexpr std::optional<size_t> stoui(std::string_view s, size_t value = 0) noexcept
   {
      if (s.empty()) {
         return value;
      }

      else if (is_digit(s[0])) {
         return stoui(s.substr(1), (s[0] - '0') + value * 10);
      }
      else {
         return {}; // not a digit
      }
   }

   GLZ_ALWAYS_INLINE void skip_number_with_validation(is_context auto&& ctx, auto&& it, auto&& end) noexcept {
        it += *it == '-';
        const auto sig_start_it = it;
        auto frac_start_it = end;
        if (*it == '0') {
           ++it;
           if (*it != '.') {
              return;
           }
           ++it;
           goto frac_start;
        }
        it = std::find_if_not(it, end, is_digit);
        if (it == sig_start_it) {
           ctx.error = error_code::syntax_error;
           return;
        }
        if ((*it | ('E' ^ 'e')) == 'e') {
            ++it;
            goto exp_start;
        }
        if (*it != '.') return;
        ++it;
      frac_start:
        frac_start_it = it;
        it = std::find_if_not(it, end, is_digit);
        if (it == frac_start_it) {
            ctx.error = error_code::syntax_error;
            return;
        }
        if ((*it | ('E' ^ 'e')) != 'e') return;
        ++it;
      exp_start:
        it += *it == '+' || *it == '-';
        const auto exp_start_it = it;
        it = std::find_if_not(it, end, is_digit);
        if (it == exp_start_it) {
            ctx.error = error_code::syntax_error;
            return;
        }
    }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_number(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] {
         return;
      }

      if constexpr (!Opts.force_conformance) {
         it = std::find_if_not(it + 1, end, is_numeric<char>);
      }
      else {
          skip_number_with_validation(ctx, it, end);
      }
   }

   template <opts Opts>
   GLZ_ALWAYS_INLINE void skip_value(is_context auto&& ctx, auto&& it, auto&& end) noexcept;
   
   // expects opening whitespace to be handled
   GLZ_ALWAYS_INLINE expected<sv, error_code> parse_key(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return unexpected(ctx.error); }
      
      match<'"'>(ctx, it);
      auto start = it;
      skip_till_quote(ctx, it, end);
      if (static_cast<bool>(ctx.error)) [[unlikely]] { return unexpected(ctx.error); }
      return sv{ start, static_cast<size_t>(it++ - start) };
   }

   // Can't use GLZ_ALWAYS_INLINE here because we have to allow infinite recursion
   template <opts Opts>
   inline void skip_object(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] {
         return;
      }

      if constexpr (!Opts.force_conformance) {
         skip_until_closed<'{', '}'>(ctx, it, end);
      }
      else {
         ++it;
         skip_ws<Opts>(ctx, it, end);
         if (*it == '}') {
            ++it;
            return;
         }
         while (true) {
            if (*it != '"') {
               ctx.error = error_code::syntax_error;
               return;
            }
            skip_string<Opts>(ctx, it, end);
            skip_ws<Opts>(ctx, it, end);
            match<':'>(ctx, it);
            skip_ws<Opts>(ctx, it, end);
            skip_value<Opts>(ctx, it, end);
            skip_ws<Opts>(ctx, it, end);
            if (*it != ',') break;
            skip_ws<Opts>(ctx, ++it, end);
         }
         match<'}'>(ctx, it);
      }
   }

   // Can't use GLZ_ALWAYS_INLINE here because we have to allow infinite recursion
   template <opts Opts>
   inline void skip_array(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] {
         return;
      }

      if constexpr (!Opts.force_conformance) {
         skip_until_closed<'[', ']'>(ctx, it, end);
      }
      else {
         ++it;
         skip_ws<Opts>(ctx, it, end);
         if (*it == ']') {
            ++it;
            return;
         }
         while (true) {
            skip_value<Opts>(ctx, it, end);
            skip_ws<Opts>(ctx, it, end);
            if (*it != ',') break;
            skip_ws<Opts>(ctx, ++it, end);
         }
         match<']'>(ctx, it);
      }
   }

   // Can't use GLZ_ALWAYS_INLINE here because we have to allow infinite recursion
   template <opts Opts>
   inline void skip_value(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      if (static_cast<bool>(ctx.error)) [[unlikely]] {
         return;
      }

      if constexpr (!Opts.force_conformance) {
         skip_ws<Opts>(ctx, it, end);
         while (true) {
            switch (*it) {
            case '{':
               skip_until_closed<'{', '}'>(ctx, it, end);
               break;
            case '[':
               skip_until_closed<'[', ']'>(ctx, it, end);
               break;
            case '"':
               skip_string<Opts>(ctx, it, end);
               break;
            case '/':
               skip_comment(ctx, it, end);
               continue;
            case ',':
            case '}':
            case ']':
               break;
            case '\0':
               break;
            default: {
               ++it;
               continue;
            }
            }

            break;
         }
      }
      else {
          skip_ws<Opts>(ctx, it, end);
          switch (*it) {
             case '{': {
                skip_object<Opts>(ctx, it, end);
                break;
             }
             case '[': {
                skip_array<Opts>(ctx, it, end);
                break;
             }
             case '"': {
                skip_string<Opts>(ctx, it, end);
                break;
             }
             case 'n': {
                ++it;
                match<"ull">(ctx, it, end);
                break;
             }
             case 'f': {
                ++it;
                match<"alse">(ctx, it, end);
                break;
             }
             case 't': {
                ++it;
                match<"rue">(ctx, it, end);
                break;
             }
             case '\0': {
                ctx.error = error_code::unexpected_end;
                break;
             }
            default: {
               skip_number<Opts>(ctx, it, end);
            }
         }
      }
   }
   
   // expects opening whitespace to be handled
   template <opts Opts>
   GLZ_ALWAYS_INLINE auto parse_value(is_context auto&& ctx, auto&& it, auto&& end) noexcept
   {
      auto start = it;
      skip_value<Opts>(ctx, it, end);
      return std::span{ start, static_cast<size_t>(std::distance(start, it)) };
   }
}
