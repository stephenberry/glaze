// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

namespace glaze::detail
{
   template <char c>
   inline void match(auto&& it, auto&& end)
   {
      if (it == end || *it != c) [[unlikely]] {
         static constexpr char b[] = {c, '\0'};
         static constexpr auto error = concat_arrays("Expected:", b);
         throw std::runtime_error(error.data());
      }
      else [[likely]] {
         ++it;
      }
   }

   template <string_literal str>
   inline void match(auto&& it, auto&& end)
   {
      const auto n = static_cast<size_t>(std::distance(it, end));
      if (n < str.size) [[unlikely]] {
         // TODO: compile time generate this message, currently borken with
         // MSVC
         static constexpr auto error = "Unexpected end of buffer. Expected:";
         throw std::runtime_error(error);
      }
      size_t i{};
      // clang and gcc will vectorize this loop
      for (auto* c = str.value; c < str.end(); ++it, ++c) {
         i += *it != *c;
      }
      if (i != 0) [[unlikely]] {
         // TODO: compile time generate this message, currently borken with
         // MSVC
         static constexpr auto error = "Expected: ";
         throw std::runtime_error(error);
      }
   }

   inline void skip_comment(auto&& it, auto&& end)
   {
      match<'/'>(it, end);
      if (it == end) [[unlikely]]
         throw std::runtime_error("Unexpected end, expected comment");
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
      else [[unlikely]]
         throw std::runtime_error("Expected / or * after /");
   }

   inline void skip_ws(auto&& it, auto&& end)
   {
      while (it != end) {
         switch (*it) {
         case ' ':
         case '\f':
         case '\r':
         case '\t':
         case '\v':
         case '\n': {
            ++it;
            continue;
         }
         case '/': {
            skip_comment(it, end);
            continue;
         }
         default:
            break;
         }
         break;  // if not white space break
      }
   }

   inline void skip_string(auto&& it, auto&& end) noexcept
   {
      match<'"'>(it, end);
      while (it < end) {
         if (*it == '"') {
            ++it;
            break;
         }
         else if (*it == '\\')
            if (++it == end) [[unlikely]]
               break;
         ++it;
      }
   }

   template <char open, char close>
   inline void skip_until_closed(auto&& it, auto&& end)
   {
      match<open>(it, end);
      size_t open_count = 1;
      size_t close_count = 0;
      while (it < end && open_count > close_count) {
         switch (*it) {
         case '/':
            skip_comment(it, end);
            break;
         case '"':
            skip_string(it, end);
            break;
         case open:
            ++open_count;
            ++it;
            break;
         case close:
            ++close_count;
            ++it;
            break;
         default:
            ++it;
         }
      }
   }
   
   inline constexpr bool is_numeric(const auto c) noexcept
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

   constexpr bool is_digit(char c) { return c <= '9' && c >= '0'; }

   constexpr size_t stoui(std::string_view s, size_t value = 0)
   {
      if (s.empty()) {
         return value;
      }

      else if (is_digit(s[0])) {
         return stoui(s.substr(1), (s[0] - '0') + value * 10);
      }

      else {
         throw std::runtime_error("not a digit");
      }
   }
}
