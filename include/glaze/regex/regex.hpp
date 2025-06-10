#pragma once

#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <type_traits>
#include <utility>

namespace glz
{
   // Fixed-capacity constexpr string
   template <std::size_t MaxSize>
   struct cx_string
   {
      std::array<char, MaxSize> data{};
      std::size_t size = 0;

      constexpr cx_string() = default;

      constexpr cx_string(std::string_view sv)
      {
         if (sv.size() > MaxSize) {
            std::abort(); // String too long
         }
         for (std::size_t i = 0; i < sv.size(); ++i) {
            data[i] = sv[i];
         }
         size = sv.size();
      }

      constexpr char operator[](std::size_t i) const { return data[i]; }
      constexpr std::size_t length() const { return size; }
      constexpr bool empty() const { return size == 0; }

      constexpr std::string_view view() const { return std::string_view(data.data(), size); }

      constexpr void push_back(char c)
      {
         if (size >= MaxSize) {
            std::abort(); // Capacity exceeded
         }
         data[size++] = c;
      }

      constexpr void append(std::string_view sv)
      {
         for (char c : sv) {
            push_back(c);
         }
      }

      constexpr void clear() { size = 0; }
   };

   // Fixed-capacity constexpr vector
   template <typename T, std::size_t MaxSize>
   struct cx_vector
   {
      std::array<T, MaxSize> data{};
      std::size_t size = 0;

      constexpr cx_vector() = default;

      constexpr void push_back(const T& item)
      {
         if (size >= MaxSize) {
            std::abort(); // Capacity exceeded
         }
         data[size++] = item;
      }

      template <typename... Args>
      constexpr void emplace_back(Args&&... args)
      {
         if (size >= MaxSize) {
            std::abort(); // Capacity exceeded
         }
         data[size++] = T(std::forward<Args>(args)...);
      }

      constexpr T& operator[](std::size_t i) { return data[i]; }
      constexpr const T& operator[](std::size_t i) const { return data[i]; }

      constexpr T& back() { return data[size - 1]; }
      constexpr const T& back() const { return data[size - 1]; }

      constexpr bool empty() const { return size == 0; }
      constexpr std::size_t length() const { return size; }

      constexpr auto begin() { return data.begin(); }
      constexpr auto end() { return data.begin() + size; }
      constexpr auto begin() const { return data.begin(); }
      constexpr auto end() const { return data.begin() + size; }
   };

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

      constexpr operator std::string_view() const { return std::string_view{value, N - 1}; }
   };

   // Atom types for regex patterns
   enum class atom_type : std::uint8_t { literal, char_class, dot, digit, word, whitespace, start_anchor, end_anchor };

   // Regex atom representation
   struct regex_atom
   {
      atom_type type;
      cx_string<256> data; // For literals and character classes
      int min_repeats = 1;
      int max_repeats = 1; // -1 for unlimited

      constexpr regex_atom() = default;
      constexpr regex_atom(atom_type t, std::string_view d = "", int min_rep = 1, int max_rep = 1)
         : type(t), data(d), min_repeats(min_rep), max_repeats(max_rep)
      {}
   };

   // Maximum reasonable number of atoms in a regex pattern
   static constexpr std::size_t MAX_ATOMS = 128;

   // Parsed pattern representation
   struct parsed_pattern
   {
      cx_vector<regex_atom, MAX_ATOMS> atoms;
      bool has_quantifiers = false;
      bool has_anchors = false;
      bool has_start_anchor = false;
      bool has_end_anchor = false;
      bool is_literal_only = false;

      constexpr parsed_pattern() = default;
   };

   // Compile-time pattern validation
   template <const std::string_view& Pattern>
   consteval bool validate_regex()
   {
      std::size_t bracket_depth = 0;
      std::size_t paren_depth = 0;
      bool in_escape = false;

      for (std::size_t i = 0; i < Pattern.size(); ++i) {
         char c = Pattern[i];

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
            if (bracket_depth == 0) std::abort();
            --bracket_depth;
            break;
         case '(':
            ++paren_depth;
            break;
         case ')':
            if (paren_depth == 0) std::abort();
            --paren_depth;
            break;
         }
      }

      if (bracket_depth != 0) std::abort();
      if (paren_depth != 0) std::abort();
      if (in_escape) std::abort();

      return true;
   }

   // Compile-time pattern parser
   template <const std::string_view& Pattern>
   consteval parsed_pattern parse_pattern()
   {
      parsed_pattern result;

      if (Pattern.empty()) {
         return result;
      }

      std::size_t i = 0;
      bool literal_only = true;

      while (i < Pattern.size()) {
         char c = Pattern[i];

         if (c == '^') {
            result.atoms.emplace_back(atom_type::start_anchor);
            result.has_anchors = true;
            result.has_start_anchor = true;
            literal_only = false;
            i++;
         }
         else if (c == '$') {
            result.atoms.emplace_back(atom_type::end_anchor);
            result.has_anchors = true;
            result.has_end_anchor = true;
            literal_only = false;
            i++;
         }
         else if (c == '\\' && i + 1 < Pattern.size()) {
            char escaped = Pattern[i + 1];
            switch (escaped) {
            case 'd':
               result.atoms.emplace_back(atom_type::digit);
               literal_only = false;
               break;
            case 'w':
               result.atoms.emplace_back(atom_type::word);
               literal_only = false;
               break;
            case 's':
               result.atoms.emplace_back(atom_type::whitespace);
               literal_only = false;
               break;
            default: {
               // Escaped literal
               std::string_view lit_data(&escaped, 1);
               result.atoms.emplace_back(atom_type::literal, lit_data);
               break;
            }
            }
            i += 2;
         }
         else if (c == '.') {
            result.atoms.emplace_back(atom_type::dot);
            literal_only = false;
            i++;
         }
         else if (c == '[') {
            auto close_pos = Pattern.find(']', i + 1);
            if (close_pos == std::string_view::npos) {
               std::abort(); // Malformed pattern
            }

            std::string_view class_data = Pattern.substr(i + 1, close_pos - (i + 1));
            result.atoms.emplace_back(atom_type::char_class, class_data);
            literal_only = false;
            i = close_pos + 1;
         }
         else if (c == '*' || c == '+' || c == '?' || c == '{') {
            // This is a quantifier for the previous atom
            if (result.atoms.empty()) {
               std::abort(); // Quantifier without preceding atom
            }

            auto& last_atom = result.atoms.back();
            result.has_quantifiers = true;
            literal_only = false;

            if (c == '*') {
               last_atom.min_repeats = 0;
               last_atom.max_repeats = -1;
               i++;
            }
            else if (c == '+') {
               last_atom.min_repeats = 1;
               last_atom.max_repeats = -1;
               i++;
            }
            else if (c == '?') {
               last_atom.min_repeats = 0;
               last_atom.max_repeats = 1;
               i++;
            }
            else if (c == '{') {
               auto close_pos = Pattern.find('}', i + 1);
               if (close_pos == std::string_view::npos) {
                  std::abort(); // Malformed quantifier
               }

               std::string_view quant_spec = Pattern.substr(i + 1, close_pos - (i + 1));

               // Parse {n}, {n,}, or {n,m}
               auto comma_pos = quant_spec.find(',');

               if (comma_pos == std::string_view::npos) {
                  // {n} format
                  int n = 0;
                  auto result_parse = std::from_chars(quant_spec.data(), quant_spec.data() + quant_spec.size(), n);
                  if (result_parse.ec != std::errc{}) {
                     std::abort();
                  }
                  last_atom.min_repeats = n;
                  last_atom.max_repeats = n;
               }
               else {
                  // {n,} or {n,m} format
                  std::string_view min_part = quant_spec.substr(0, comma_pos);
                  std::string_view max_part = quant_spec.substr(comma_pos + 1);

                  int min_val = 0;
                  auto result_min = std::from_chars(min_part.data(), min_part.data() + min_part.size(), min_val);
                  if (result_min.ec != std::errc{}) {
                     std::abort();
                  }
                  last_atom.min_repeats = min_val;

                  if (max_part.empty()) {
                     // {n,} format
                     last_atom.max_repeats = -1;
                  }
                  else {
                     // {n,m} format
                     int max_val = 0;
                     auto result_max = std::from_chars(max_part.data(), max_part.data() + max_part.size(), max_val);
                     if (result_max.ec != std::errc{}) {
                        std::abort();
                     }
                     last_atom.max_repeats = max_val;
                  }
               }

               i = close_pos + 1;
            }
         }
         else {
            // Regular literal character
            std::string_view lit_data(&c, 1);
            result.atoms.emplace_back(atom_type::literal, lit_data);
            i++;
         }
      }

      result.is_literal_only = literal_only;
      return result;
   }

   struct analysis
   {
      parsed_pattern pattern;
      bool has_quantifiers;
      bool has_anchors;
      bool is_literal_only;
      bool is_simple;
      std::size_t atom_count;
   };

   // Compile-time pattern analysis
   template <const std::string_view& Pattern>
   consteval auto analyze_pattern()
   {
      constexpr auto parsed = parse_pattern<Pattern>();

      return analysis{parsed,
                      parsed.has_quantifiers,
                      parsed.has_anchors,
                      parsed.is_literal_only,
                      !parsed.has_quantifiers && !parsed.has_anchors,
                      parsed.atoms.length()};
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
   };

   // Optimized matchers based on compile-time analysis
   struct optimized_matcher
   {
      // Fast literal-only matcher
      template <const auto& Analysis, class Iterator>
      static constexpr match_result<Iterator> match_literal_only(Iterator begin, Iterator end, bool anchored)
         requires(Analysis.is_literal_only)
      {
         constexpr auto& atoms = Analysis.pattern.atoms;

         if constexpr (atoms.length() == 0) {
            return match_result<Iterator>{begin, begin};
         }

         // Build the literal string at compile time
         constexpr auto literal_str = []() {
            cx_string<1024> result;
            for (std::size_t i = 0; i < atoms.length(); ++i) {
               const auto& atom = atoms[i];
               if (atom.type == atom_type::literal) {
                  result.append(atom.data.view());
               }
            }
            return result;
         }();

         if (anchored) {
            // Exact match required
            if (static_cast<std::size_t>(end - begin) != literal_str.length()) {
               return {};
            }

            for (std::size_t i = 0; i < literal_str.length(); ++i) {
               if (begin[i] != literal_str[i]) {
                  return {};
               }
            }
            return match_result<Iterator>{begin, end};
         }
         else {
            // Search for substring
            if (literal_str.length() == 0) {
               return match_result<Iterator>{begin, begin};
            }

            if (static_cast<std::size_t>(end - begin) < literal_str.length()) {
               return {};
            }

            for (auto it = begin; it <= end - literal_str.length(); ++it) {
               bool matches = true;
               for (std::size_t i = 0; i < literal_str.length(); ++i) {
                  if (it[i] != literal_str[i]) {
                     matches = false;
                     break;
                  }
               }
               if (matches) {
                  return match_result<Iterator>{it, it + literal_str.length()};
               }
            }
            return {};
         }
      }

      // Simple matcher for patterns without quantifiers
      template <const auto& Analysis, class Iterator>
      static constexpr match_result<Iterator> match_simple(Iterator begin, Iterator end, bool anchored)
         requires(Analysis.is_simple && !Analysis.is_literal_only)
      {
         constexpr auto& atoms = Analysis.pattern.atoms;

         auto match_atom = [](const regex_atom& atom, Iterator& current, Iterator end) -> bool {
            if (current == end) return false;

            switch (atom.type) {
            case atom_type::literal:
               if (atom.data.length() == 1 && *current == atom.data[0]) {
                  ++current;
                  return true;
               }
               return false;

            case atom_type::dot:
               ++current;
               return true;

            case atom_type::digit:
               if (*current >= '0' && *current <= '9') {
                  ++current;
                  return true;
               }
               return false;

            case atom_type::word:
               if ((*current >= 'a' && *current <= 'z') || (*current >= 'A' && *current <= 'Z') ||
                   (*current >= '0' && *current <= '9') || *current == '_') {
                  ++current;
                  return true;
               }
               return false;

            case atom_type::whitespace:
               if (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r') {
                  ++current;
                  return true;
               }
               return false;

            case atom_type::char_class: {
               char ch = *current;
               auto class_data = atom.data.view();
               bool negate = false;
               std::size_t start = 0;

               if (class_data.length() > 0 && class_data[0] == '^') {
                  negate = true;
                  start = 1;
               }

               bool found = false;
               for (std::size_t i = start; i < class_data.length();) {
                  if (i + 2 < class_data.length() && class_data[i + 1] == '-') {
                     // Range
                     char range_start = class_data[i];
                     char range_end = class_data[i + 2];
                     if (ch >= range_start && ch <= range_end) {
                        found = true;
                        break;
                     }
                     i += 3;
                  }
                  else {
                     // Literal
                     if (ch == class_data[i]) {
                        found = true;
                        break;
                     }
                     i += 1;
                  }
               }

               if (negate) found = !found;
               if (found) {
                  ++current;
                  return true;
               }
               return false;
            }

            default:
               return false;
            }
         };

         if (anchored) {
            Iterator current = begin;
            for (std::size_t i = 0; i < atoms.length(); ++i) {
               if (!match_atom(atoms[i], current, end)) {
                  return {};
               }
            }
            return match_result<Iterator>{begin, current};
         }
         else {
            for (auto it = begin; it != end; ++it) {
               Iterator current = it;
               bool all_matched = true;

               for (std::size_t i = 0; i < atoms.length(); ++i) {
                  if (!match_atom(atoms[i], current, end)) {
                     all_matched = false;
                     break;
                  }
               }

               if (all_matched) {
                  return match_result<Iterator>{it, current};
               }
            }
            return {};
         }
      }

      // Helper function to match a single atom without quantifiers
      template <class Iterator>
      static constexpr bool match_single_atom(const regex_atom& atom, Iterator& current, Iterator end)
      {
         if (current == end) return false;

         switch (atom.type) {
         case atom_type::literal:
            if (atom.data.length() == 1 && *current == atom.data[0]) {
               ++current;
               return true;
            }
            return false;

         case atom_type::dot:
            ++current;
            return true;

         case atom_type::digit:
            if (*current >= '0' && *current <= '9') {
               ++current;
               return true;
            }
            return false;

         case atom_type::word:
            if ((*current >= 'a' && *current <= 'z') || (*current >= 'A' && *current <= 'Z') ||
                (*current >= '0' && *current <= '9') || *current == '_') {
               ++current;
               return true;
            }
            return false;

         case atom_type::whitespace:
            if (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r') {
               ++current;
               return true;
            }
            return false;

         case atom_type::char_class: {
            char ch = *current;
            auto class_data = atom.data.view();
            bool negate = false;
            std::size_t start = 0;

            if (class_data.length() > 0 && class_data[0] == '^') {
               negate = true;
               start = 1;
            }

            bool found = false;
            for (std::size_t i = start; i < class_data.length();) {
               if (i + 2 < class_data.length() && class_data[i + 1] == '-') {
                  // Range
                  char range_start = class_data[i];
                  char range_end = class_data[i + 2];
                  if (ch >= range_start && ch <= range_end) {
                     found = true;
                     break;
                  }
                  i += 3;
               }
               else {
                  // Literal
                  if (ch == class_data[i]) {
                     found = true;
                     break;
                  }
                  i += 1;
               }
            }

            if (negate) found = !found;
            if (found) {
               ++current;
               return true;
            }
            return false;
         }

         case atom_type::start_anchor:
         case atom_type::end_anchor:
            // Anchors don't consume input, handled separately
            return true;

         default:
            return false;
         }
      }

      // Recursive helper for backtracking through pattern atoms
      template <const auto& Analysis, class Iterator>
      static constexpr bool match_atoms_recursive(std::size_t atom_idx, Iterator& current, Iterator end,
                                                  Iterator line_begin, int depth = 0)
      {
         // Prevent infinite recursion
         if (depth > 200) return false;

         constexpr auto& atoms = Analysis.pattern.atoms;

         // If we've matched all atoms, success
         if (atom_idx >= atoms.length()) {
            return true;
         }

         const auto& atom = atoms[atom_idx];

         // Handle anchors
         if (atom.type == atom_type::start_anchor) {
            if (current != line_begin) return false;
            return match_atoms_recursive<Analysis>(atom_idx + 1, current, end, line_begin, depth + 1);
         }

         if (atom.type == atom_type::end_anchor) {
            if (current != end) return false;
            return match_atoms_recursive<Analysis>(atom_idx + 1, current, end, line_begin, depth + 1);
         }

         // For quantified atoms, try different numbers of matches
         int min_matches = atom.min_repeats;
         int max_matches = atom.max_repeats;

         // Limit unlimited quantifiers to prevent infinite loops
         int try_max = (max_matches == -1) ? 200 : max_matches;

         // Try from max down to min (greedy matching with backtracking)
         for (int match_count = try_max; match_count >= min_matches; --match_count) {
            Iterator saved_pos = current;
            bool can_match_this_many = true;
            int actual_matches = 0;

            // Try to match exactly match_count times
            for (int i = 0; i < match_count && can_match_this_many; ++i) {
               Iterator before_match = current;
               if (match_single_atom(atom, current, end)) {
                  actual_matches++;
                  // Prevent infinite loop on empty matches with unlimited quantifiers
                  if (before_match == current && max_matches == -1) {
                     break;
                  }
               }
               else {
                  can_match_this_many = false;
               }
            }

            // If we matched at least the minimum required
            if (actual_matches >= min_matches && (max_matches == -1 || actual_matches <= max_matches)) {
               // Try to match the rest of the pattern
               if (match_atoms_recursive<Analysis>(atom_idx + 1, current, end, line_begin, depth + 1)) {
                  return true;
               }
            }

            // Backtrack
            current = saved_pos;

            // For unlimited quantifiers, reduce the step size after trying large numbers
            if (max_matches == -1 && match_count > 50) {
               match_count = 50; // Jump down to reasonable limit
            }
         }

         return false;
      }

      // General matcher for complex patterns with backtracking
      template <const auto& Analysis, class Iterator>
      static constexpr match_result<Iterator> match_general(Iterator begin, Iterator end, bool anchored)
      {
         constexpr auto& atoms = Analysis.pattern.atoms;

         if (atoms.length() == 0) {
            return match_result<Iterator>{begin, begin};
         }

         // For anchored mode (match), start only at beginning
         if (anchored) {
            Iterator current = begin;
            if (match_atoms_recursive<Analysis>(0, current, end, begin)) {
               // For match(), ensure entire string is consumed unless pattern has start anchor
               if (current != end && !Analysis.pattern.has_start_anchor) {
                  return {};
               }
               return match_result<Iterator>{begin, current};
            }
            return {};
         }

         // For search mode, try at each position (unless ^ anchor)
         if (Analysis.pattern.has_start_anchor) {
            // With ^ anchor in search mode, only try at the very beginning
            Iterator current = begin;
            if (match_atoms_recursive<Analysis>(0, current, end, begin)) {
               return match_result<Iterator>{begin, current};
            }
            return {};
         }

         // Normal search - try at each position
         for (auto it = begin; it != end; ++it) {
            Iterator current = it;
            if (match_atoms_recursive<Analysis>(0, current, end, it)) {
               return match_result<Iterator>{it, current};
            }
         }

         // Also try matching empty string at end for patterns that can match empty
         Iterator current_at_end = end;
         if (match_atoms_recursive<Analysis>(0, current_at_end, end, end) && current_at_end == end) {
            return match_result<Iterator>{end, end};
         }

         return {};
      }
   };

   // Forward declaration for basic_regex
   template <const std::string_view& Pattern>
   class basic_regex;

   // Factory function
   template <fixed_string Str>
   constexpr auto re()
   {
      static constexpr std::string_view pattern_view = Str;
      return basic_regex<pattern_view>{};
   }

   // Main regex class with compile-time optimization
   template <const std::string_view& Pattern>
   class basic_regex
   {
      static_assert(validate_regex<Pattern>(), "Invalid regex pattern");
      static constexpr auto analysis = analyze_pattern<Pattern>();

     public:
      constexpr basic_regex() = default;

      template <class Iterator>
      constexpr match_result<Iterator> match(Iterator begin, Iterator end) const
      {
         if constexpr (analysis.is_literal_only) {
            auto result = optimized_matcher::match_literal_only<analysis>(begin, end, true);
            // For match(), ensure entire string is consumed unless pattern has start anchor
            if (result.matched && result.end_pos != end && !analysis.pattern.has_start_anchor) {
               return {};
            }
            return result;
         }
         else if constexpr (analysis.is_simple) {
            auto result = optimized_matcher::match_simple<analysis>(begin, end, true);
            if (result.matched && result.end_pos != end && !analysis.pattern.has_start_anchor) {
               return {};
            }
            return result;
         }
         else {
            return optimized_matcher::match_general<analysis>(begin, end, true);
         }
      }

      template <class Iterator>
      constexpr match_result<Iterator> search(Iterator begin, Iterator end) const
      {
         if constexpr (analysis.is_literal_only) {
            return optimized_matcher::match_literal_only<analysis>(begin, end, false);
         }
         else if constexpr (analysis.is_simple) {
            return optimized_matcher::match_simple<analysis>(begin, end, false);
         }
         else {
            return optimized_matcher::match_general<analysis>(begin, end, false);
         }
      }

      constexpr match_result<const char*> match(std::string_view text) const
      {
         return match(text.data(), text.data() + text.size());
      }

      constexpr match_result<const char*> search(std::string_view text) const
      {
         return search(text.data(), text.data() + text.size());
      }

      // Get the pattern for debugging
      constexpr std::string_view pattern() const { return Pattern; }

      // Compile-time introspection
      static constexpr bool has_quantifiers() { return analysis.has_quantifiers; }
      static constexpr bool has_anchors() { return analysis.has_anchors; }
      static constexpr bool is_literal_only() { return analysis.is_literal_only; }
      static constexpr std::size_t atom_count() { return analysis.atom_count; }
   };
}
