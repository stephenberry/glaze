// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <iterator>

namespace glz::detail
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
      ++it;
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
         // assuming ascii
         if (static_cast<uint8_t>(*it) < 33) {
            ++it;
         }
         else if (*it == '/') {
            skip_comment(it, end);
         }
         else {
            break;
         }
      }
   }
   
   inline void skip_ws_no_comments(auto&& it, auto&& end)
   {
      while (it != end) {
         // assuming ascii
         if (static_cast<uint8_t>(*it) < 33) {
            ++it;
         }
         else {
            break;
         }
      }
   }

   inline void skip_till_escape_or_quote(auto&& it, auto&& end)
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      if (it == end) [[unlikely]]
         throw std::runtime_error("Unexpected end, expected escape or quote");

      const char* start = &(*it);
      const char* current = start;
      const char* stop = start + std::distance(it, end);

      auto has_zero = [](uint64_t chunk) { return (((chunk - 0x0101010101010101) & ~chunk) & 0x8080808080808080); };

      auto has_qoute = [&](uint64_t chunk) {
         return has_zero(chunk ^ 0b0010001000100010001000100010001000100010001000100010001000100010);
      };

      auto has_escape = [&](uint64_t chunk) {
         return has_zero(chunk ^ 0b0101110001011100010111000101110001011100010111000101110001011100);
      };

      const auto end_m7 = stop - 7;
      for (; current < end_m7; current += 8) {
         // TODO: Change to memcpy
         const auto chunk = *reinterpret_cast<const uint64_t*>(current);
         uint64_t test = has_qoute(chunk) | has_escape(chunk);
         if (test != 0) {
            current += (std::countr_zero(test) >> 3);
            it += current - start;
            return;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (current < stop) {
         switch (*current) {
         case '\\':
         case '"':
            it += current - start;
            return;
         }
         ++current;
      }
      it += current - start;
      throw std::runtime_error("Expected \"");
   }
   
   inline void skip_till_quote(auto&& it, auto&& end)
   {
      static_assert(std::contiguous_iterator<std::decay_t<decltype(it)>>);

      if (it == end) [[unlikely]]
         throw std::runtime_error("Unexpected end, expected escape or quote");

      const char* start = &(*it);
      const char* current = start;
      const char* stop = start + std::distance(it, end);

      auto has_zero = [](uint64_t chunk) { return (((chunk - 0x0101010101010101) & ~chunk) & 0x8080808080808080); };

      auto has_qoute = [&](uint64_t chunk) {
         return has_zero(chunk ^ 0b0010001000100010001000100010001000100010001000100010001000100010);
      };

      const auto end_m7 = stop - 7;
      for (; current < end_m7; current += 8) {
         // TODO: Change to memcpy
         const auto chunk = *reinterpret_cast<const uint64_t*>(current);
         uint64_t test = has_qoute(chunk);
         if (test != 0) {
            current += (std::countr_zero(test) >> 3);
            it += current - start;
            return;
         }
      }

      // Tail end of buffer. Should be rare we even get here
      while (current < stop) {
         switch (*current) {
         case '\\':
         case '"':
            it += current - start;
            return;
         }
         ++current;
      }
      it += current - start;
      throw std::runtime_error("Expected \"");
   }

   inline void skip_string(auto&& it, auto&& end) noexcept
   {
      ++it;
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

   template <char open, char close>
   inline void skip_until_closed(auto&& it, auto&& end)
   {
      ++it;
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
   
   inline std::string_view parse_key(auto&& it, auto&& end)
   {
      skip_ws(it, end);
      match<'"'>(it, end);
      auto start = it;
      skip_till_quote(it, end);
      return { start, static_cast<size_t>(it++ - start) };
   }
}
