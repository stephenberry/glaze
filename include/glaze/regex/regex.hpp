#pragma once

#include <array>
#include <charconv> // For std::from_chars
#include <concepts>
#include <functional> // For std::function
#include <string>
#include <string_view>
#include <system_error> // For std::errc
#include <type_traits>
#include <utility> // For std::index_sequence

namespace glz
{
   // Compile-time regex construction using fixed_string NTTP
   template <std::size_t N>
   struct fixed_string
   {
      char value[N]; // Includes null terminator

      constexpr fixed_string(const char (&str)[N])
      {
         for (std::size_t i = 0; i < N; ++i) {
            value[i] = str[i];
         }
      }
   };

   // Forward declaration for basic_regex, needed by re_impl
   template <char... Chars>
   class basic_regex;

   // Helper to expand the fixed_string into a character pack for basic_regex
   template <fixed_string Str, std::size_t... Is>
   constexpr auto re_impl(std::index_sequence<Is...>)
   {
      // Str.value includes the null terminator.
      // The Chars... pack for basic_regex should not include the null terminator.
      // Indices Is... will be 0, 1, ..., (sizeof(Str.value) - 2)
      return basic_regex<Str.value[Is]...>{};
   }

   template <fixed_string Str>
   constexpr auto re()
   {
      // sizeof(Str.value) is N (length of literal including null).
      // We need N-1 characters for the pattern (excluding null).
      // std::make_index_sequence<sizeof(Str.value) - 1> generates indices for these N-1 chars.
      // This correctly handles glz::re<"">() which results in basic_regex<>.
      return re_impl<Str>(std::make_index_sequence<sizeof(Str.value) - 1>{});
   }

   // Result type for matches
   template <class Iterator>
   struct match_result
   {
      bool matched = false;
      Iterator begin_pos;
      Iterator end_pos;

      constexpr match_result() = default;
      constexpr match_result(Iterator begin, Iterator end) : matched(true), begin_pos(begin), end_pos(end) {}

      constexpr explicit operator bool() const { return matched; }

      constexpr std::string_view view() const
         requires std::same_as<Iterator, const char*>
      {
         return matched ? std::string_view(begin_pos, end_pos - begin_pos) : std::string_view{};
      }

      constexpr std::string str() const
         requires std::same_as<Iterator, const char*>
      {
         return matched ? std::string(begin_pos, end_pos) : std::string{};
      }
   };

   // Compile-time string from character pack
   template <char... Chars>
   struct pattern_string
   {
      static constexpr std::size_t len = sizeof...(Chars);
      static constexpr char data[len + 1] = {Chars..., '\0'};

      constexpr char operator[](std::size_t i) const { return data[i]; }
      constexpr std::size_t size() const { return len; }
      constexpr const char* c_str() const { return data; }
      constexpr std::string_view view() const { return std::string_view{data, len}; }
   };

   // Compile-time parser that validates and creates type representation
   template <char... Chars>
   struct parse_result
   {
      static constexpr auto pattern_inst = pattern_string<Chars...>{};

      // Simple validation - just check for basic syntax errors
      static constexpr bool validate()
      { // Changed back from consteval to constexpr
         std::size_t bracket_depth = 0;
         std::size_t paren_depth = 0;
         bool in_escape = false;

         for (std::size_t i = 0; i < pattern_inst.size(); ++i) { // Use pattern_inst
            char c = pattern_inst[i]; // Use pattern_inst

            if (in_escape) {
               in_escape = false;
               continue;
            }

            switch (c) {
            case '\\':
               in_escape = true;
               break;
            case '[':
               ++bracket_depth;
               break;
            case ']':
               if (bracket_depth == 0) return false; // Invalid: unmatched closing bracket
               --bracket_depth;
               break;
            case '(':
               ++paren_depth;
               break;
            case ')':
               if (paren_depth == 0) return false; // Invalid: unmatched closing parenthesis
               --paren_depth;
               break;
            }
         }

         if (bracket_depth != 0) return false; // Invalid: unclosed bracket
         if (paren_depth != 0) return false; // Invalid: unclosed parenthesis
         if (in_escape) return false; // Invalid: trailing escape character

         return true;
      }

      static constexpr bool is_valid = validate(); // validate() now uses pattern_inst
      static_assert(is_valid,
                    "Invalid regex pattern: The provided pattern has a syntax error (e.g., unclosed "
                    "brackets/parentheses, or trailing escape).");
   };

   // Matcher implementations using function templates for different pattern types
   struct matcher
   {
      // Match a single character literal
      template <char C, typename Iterator>
      static bool match_char(Iterator& current, Iterator end)
      {
         if (current != end && *current == C) {
            ++current;
            return true;
         }
         return false;
      }

      // Match a character range
      template <char Start, char End, typename Iterator>
      static bool match_range(Iterator& current, Iterator end)
      {
         if (current != end && *current >= Start && *current <= End) {
            ++current;
            return true;
         }
         return false;
      }

      // Match any character
      template <class Iterator>
      static bool match_any(Iterator& current, Iterator end)
      {
         if (current != end) {
            ++current;
            return true;
         }
         return false;
      }

      // Match digit
      template <class Iterator>
      static bool match_digit(Iterator& current, Iterator end)
      {
         if (current != end && *current >= '0' && *current <= '9') {
            ++current;
            return true;
         }
         return false;
      }

      // Match word character
      template <class Iterator>
      static bool match_word(Iterator& current, Iterator end)
      {
         if (current != end && ((*current >= 'a' && *current <= 'z') || (*current >= 'A' && *current <= 'Z') ||
                                (*current >= '0' && *current <= '9') || *current == '_')) {
            ++current;
            return true;
         }
         return false;
      }

      // Match whitespace
      template <class Iterator>
      static bool match_whitespace(Iterator& current, Iterator end)
      {
         if (current != end && (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r')) {
            ++current;
            return true;
         }
         return false;
      }

     private: // Moved private keyword up, match_pattern and match_string are the main interface from basic_regex
      template <class Iterator>
      static bool match_char_literal(char expected, Iterator& current, Iterator end)
      {
         if (current != end && *current == expected) {
            ++current;
            return true;
         }
         return false;
      }

      template <class Iterator>
      static bool match_char_class(std::string_view char_class, Iterator& current, Iterator end)
      {
         if (current == end) return false;

         char ch = *current;
         bool negate = false;
         std::size_t start = 0;

         if (!char_class.empty() && char_class[0] == '^') {
            negate = true;
            start = 1;
         }

         bool found = false;
         for (std::size_t i = start; i < char_class.size(); ++i) {
            if (i + 2 < char_class.size() && char_class[i + 1] == '-') {
               // Character range
               if (ch >= char_class[i] && ch <= char_class[i + 2]) {
                  found = true;
                  break;
               }
               i += 2; // Skip the range
            }
            else {
               if (ch == char_class[i]) {
                  found = true;
                  break;
               }
            }
         }

         if (negate) found = !found;

         if (found) {
            ++current;
            return true;
         }
         return false;
      }

      // Main matching logic for a pattern sequence
      template <class Iterator>
      static bool match_string(std::string_view pattern, Iterator& current_ref, Iterator end,
                               Iterator begin_of_this_attempt)
      {
         Iterator current = current_ref; // Work on a local copy, commit on full match
         std::size_t pat_idx = 0;

         if (!pattern.empty() && pattern[0] == '^') {
            if (current != begin_of_this_attempt) {
               return false; // '^' anchor failed
            }
            pat_idx++; // Consume '^'
         }

         while (pat_idx < pattern.size()) {
            std::function<bool(Iterator&, Iterator)> atom_match_logic;
            std::size_t atom_pattern_len = 0;

            char p_char = pattern[pat_idx];

            if (p_char == '\\' && pat_idx + 1 < pattern.size()) {
               char escaped = pattern[pat_idx + 1];
               atom_pattern_len = 2;
               switch (escaped) {
               case 'd':
                  atom_match_logic = match_digit<Iterator>;
                  break;
               case 'w':
                  atom_match_logic = match_word<Iterator>;
                  break;
               case 's':
                  atom_match_logic = match_whitespace<Iterator>;
                  break;
               // Handle escaped special characters like \., \*, \+, etc.
               case '.':
               case '*':
               case '+':
               case '?':
               case '{':
               case '}':
               case '(':
               case ')':
               case '[':
               case ']':
               case '\\':
               case '^':
               case '$':
               case '|':
                  atom_match_logic = [c = escaped](Iterator& i, Iterator e) { return match_char_literal(c, i, e); };
                  break;
               default:
                  atom_match_logic = [c = escaped](Iterator& i, Iterator e) { return match_char_literal(c, i, e); };
                  break;
               }
            }
            else if (p_char == '.') {
               atom_pattern_len = 1;
               atom_match_logic = match_any<Iterator>;
            }
            else if (p_char == '[') {
               auto close_pos = pattern.find(']', pat_idx + 1);
               if (close_pos == std::string_view::npos) return false; // Malformed pattern
               std::string_view char_class_str = pattern.substr(pat_idx + 1, close_pos - (pat_idx + 1));
               atom_pattern_len = (close_pos + 1) - pat_idx;
               atom_match_logic = [char_class_str](Iterator& i, Iterator e) {
                  return match_char_class(char_class_str, i, e);
               };
            }
            else if (p_char == '$') {
               if (pat_idx == pattern.size() - 1) { // $ must be last char in pattern
                  current_ref = current; // Commit progress
                  return current == end; // Consumes no input, checks if at end
               }
               return false; // $ not last in pattern or other complex use not supported
            }
            else if (p_char == '*' || p_char == '+' || p_char == '?' || p_char == '{' || p_char == '|') {
               // Quantifier/alternator at start of atom - should be caught by validation or implies error
               return false;
            }
            else { // Literal character
               atom_pattern_len = 1;
               atom_match_logic = [c = p_char](Iterator& i, Iterator e) { return match_char_literal(c, i, e); };
            }

            pat_idx += atom_pattern_len;

            int min_repeats = 1, max_repeats = 1;
            // bool quantified = false; // unused

            if (pat_idx < pattern.size()) {
               char q_char = pattern[pat_idx];
               if (q_char == '?') {
                  min_repeats = 0;
                  max_repeats = 1; /*quantified = true;*/
                  pat_idx++;
               }
               else if (q_char == '*') {
                  min_repeats = 0;
                  max_repeats = -1; /*quantified = true;*/
                  pat_idx++;
               }
               else if (q_char == '+') {
                  min_repeats = 1;
                  max_repeats = -1; /*quantified = true;*/
                  pat_idx++;
               }
               else if (q_char == '{') {
                  std::size_t quant_spec_start = pat_idx + 1;
                  std::size_t quant_spec_end = pattern.find('}', quant_spec_start);
                  if (quant_spec_end == std::string_view::npos) return false; // Malformed {

                  std::string_view quant_spec_str = pattern.substr(quant_spec_start, quant_spec_end - quant_spec_start);
                  pat_idx = quant_spec_end + 1;
                  // quantified = true;

                  const char* spec_start_ptr = quant_spec_str.data();
                  const char* spec_end_ptr = quant_spec_str.data() + quant_spec_str.size();

                  auto result_min = std::from_chars(spec_start_ptr, spec_end_ptr, min_repeats);
                  if (result_min.ec != std::errc() || result_min.ptr == spec_start_ptr) return false;

                  if (result_min.ptr == spec_end_ptr) { // Format {N}
                     max_repeats = min_repeats;
                  }
                  else if (*result_min.ptr == ',') {
                     spec_start_ptr = result_min.ptr + 1;
                     if (spec_start_ptr == spec_end_ptr) { // Format {N,}
                        max_repeats = -1;
                     }
                     else { // Format {N,M}
                        auto result_max = std::from_chars(spec_start_ptr, spec_end_ptr, max_repeats);
                        if (result_max.ec != std::errc() || result_max.ptr != spec_end_ptr ||
                            result_max.ptr == spec_start_ptr)
                           return false;
                        if (max_repeats < min_repeats && max_repeats != -1)
                           return false; // M < N is invalid (unless M is -1 for infinity)
                     }
                  }
                  else {
                     return false; // Invalid char after N in {N...}
                  }
               }
            }

            int match_count = 0;
            // Iterator iter_before_quantified_match = current; // Unused variable

            for (int i = 0; (max_repeats == -1 || i < max_repeats); ++i) {
               Iterator iter_before_this_atom_match = current;
               if (atom_match_logic(current, end)) {
                  match_count++;
                  if (iter_before_this_atom_match == current &&
                      max_repeats == -1) { // Matched empty and infinite quantifier
                     break;
                  }
               }
               else {
                  current = iter_before_this_atom_match; // Backtrack this failed atom match
                  break;
               }
            }

            if (match_count < min_repeats) {
               // If not enough matches, the whole pattern piece fails.
               // For greedy matching, we don't backtrack previous atoms here.
               // A full regex engine would need backtracking for complex cases.
               return false;
            }
            // current is now positioned after the greedily matched sequence.
         }

         current_ref = current; // Commit progress
         return true;
      }

     public: // Moved public keyword up, match_pattern is part of public API of matcher
      // Simple pattern matcher using string processing
      template <class Iterator>
      static match_result<Iterator> match_pattern(std::string_view pattern, Iterator begin, Iterator end, bool anchored)
      {
         if (anchored) {
            Iterator current = begin;
            if (match_string(pattern, current, end, begin)) { // Pass 'begin' for ^ anchor context
               return match_result<Iterator>{begin, current};
            }
            // Handle cases like pattern "a*" on empty string ""
            // If pattern can match an empty string.
            if (pattern.empty()) return match_result<Iterator>{begin, begin};
            Iterator temp_current = begin;
            // Check if pattern can match an empty string at 'begin'
            if (match_string(pattern, temp_current, begin, begin) && temp_current == begin) {
               return match_result<Iterator>{begin, begin};
            }

            return {};
         }

         // Search mode - try at each position
         for (auto it = begin; it != end; ++it) {
            Iterator current = it;
            // For ^ in search mode, it should match against `it` (start of this attempt)
            if (match_string(pattern, current, end, it)) {
               return match_result<Iterator>{it, current};
            }
         }

         // After loop, check if pattern can match an empty string at 'end'
         // This handles cases like "abc" with pattern "d*", expecting empty match at end of "abc"
         Iterator current_at_end = end;
         if (match_string(pattern, current_at_end, end, end) && current_at_end == end) {
            return match_result<Iterator>{end, end};
         }

         return {};
      }
   };

   // Main regex class
   template <char... Chars>
   class basic_regex
   {
      // Ensure parse_result is instantiated for its static_assert to run.
      [[maybe_unused]] static constexpr auto validation_check = parse_result<Chars...>{};
      // Get the pattern string view from pattern_string
      static constexpr auto pattern_obj = pattern_string<Chars...>{};
      static constexpr std::string_view pattern_view{pattern_obj.data, pattern_obj.len};

     public:
      constexpr basic_regex() = default;

      template <class Iterator>
      match_result<Iterator> match(Iterator begin, Iterator end) const
      {
         return matcher::match_pattern(pattern_view, begin, end, true);
      }

      template <class Iterator>
      match_result<Iterator> search(Iterator begin, Iterator end) const
      {
         return matcher::match_pattern(pattern_view, begin, end, false);
      }

      match_result<const char*> match(std::string_view text) const
      {
         return match(text.data(), text.data() + text.size());
      }

      match_result<const char*> search(std::string_view text) const
      {
         return search(text.data(), text.data() + text.size());
      }

      // Get the pattern for debugging
      constexpr std::string_view pattern() const { return pattern_view; }
   };
}
