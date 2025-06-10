#pragma once

#include <string_view>
#include <array>
#include <type_traits>
#include <concepts>
#include <string>

namespace glz {

// Result type for matches
template<typename Iterator>
struct match_result {
    bool matched = false;
    Iterator begin_pos;
    Iterator end_pos;
    
    constexpr match_result() = default;
    constexpr match_result(Iterator begin, Iterator end) 
        : matched(true), begin_pos(begin), end_pos(end) {}
    
    constexpr explicit operator bool() const { return matched; }
    
    constexpr std::string_view view() const requires std::same_as<Iterator, const char*> {
        return matched ? std::string_view(begin_pos, end_pos - begin_pos) : std::string_view{};
    }
    
    constexpr std::string str() const requires std::same_as<Iterator, const char*> {
        return matched ? std::string(begin_pos, end_pos) : std::string{};
    }
};

// AST node types as simple template classes
template<char C> 
struct char_literal {
    static constexpr char value = C;
};

template<char Start, char End> 
struct char_range {
    static constexpr char start = Start;
    static constexpr char end = End;
};

struct any_char {};
struct digit {};
struct word_char {};
struct whitespace {};
struct line_start {};
struct line_end {};

template<typename... Items> 
struct sequence {};

template<typename... Alternatives> 
struct alternation {};

template<typename Content, int Min = 1, int Max = -1> 
struct repeat {};

// Compile-time string from character pack
template<char... Chars>
struct pattern_string {
    static constexpr std::size_t len = sizeof...(Chars);
    static constexpr char data[len + 1] = {Chars..., '\0'};
    
    constexpr char operator[](std::size_t i) const { return data[i]; }
    constexpr std::size_t size() const { return len; }
    constexpr const char* c_str() const { return data; }
    constexpr std::string_view view() const { return std::string_view{data, len}; }
};

// Compile-time parser that validates and creates type representation
template<char... Chars>
struct parse_result {
    static constexpr auto pattern = pattern_string<Chars...>{};
    static constexpr bool valid = true;
    
    // Simple validation - just check for basic syntax errors
    static constexpr bool validate() {
        std::size_t bracket_depth = 0;
        std::size_t paren_depth = 0;
        bool in_escape = false;
        
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            char c = pattern[i];
            
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
                    if (bracket_depth == 0) return false;
                    --bracket_depth;
                    break;
                case '(':
                    ++paren_depth;
                    break;
                case ')':
                    if (paren_depth == 0) return false;
                    --paren_depth;
                    break;
            }
        }
        
        return bracket_depth == 0 && paren_depth == 0 && !in_escape;
    }
    
    static_assert(validate(), "Invalid regex pattern");
};

// Matcher implementations using function templates for different pattern types
class matcher {
public:
    // Match a single character literal
    template<char C, typename Iterator>
    static bool match_char(Iterator& current, Iterator end) {
        if (current != end && *current == C) {
            ++current;
            return true;
        }
        return false;
    }
    
    // Match a character range
    template<char Start, char End, typename Iterator>
    static bool match_range(Iterator& current, Iterator end) {
        if (current != end && *current >= Start && *current <= End) {
            ++current;
            return true;
        }
        return false;
    }
    
    // Match any character
    template<typename Iterator>
    static bool match_any(Iterator& current, Iterator end) {
        if (current != end) {
            ++current;
            return true;
        }
        return false;
    }
    
    // Match digit
    template<typename Iterator>
    static bool match_digit(Iterator& current, Iterator end) {
        if (current != end && *current >= '0' && *current <= '9') {
            ++current;
            return true;
        }
        return false;
    }
    
    // Match word character
    template<typename Iterator>
    static bool match_word(Iterator& current, Iterator end) {
        if (current != end && ((*current >= 'a' && *current <= 'z') || 
                               (*current >= 'A' && *current <= 'Z') || 
                               (*current >= '0' && *current <= '9') || 
                               *current == '_')) {
            ++current;
            return true;
        }
        return false;
    }
    
    // Match whitespace
    template<typename Iterator>
    static bool match_whitespace(Iterator& current, Iterator end) {
        if (current != end && (*current == ' ' || *current == '\t' || 
                              *current == '\n' || *current == '\r')) {
            ++current;
            return true;
        }
        return false;
    }
    
    // Simple pattern matcher using string processing
    template<typename Iterator>
    static match_result<Iterator> match_pattern(std::string_view pattern, Iterator begin, Iterator end, bool anchored) {
        Iterator current = begin;
        
        if (anchored) {
            if (match_string(pattern, current, end)) {
                return match_result<Iterator>{begin, current};
            }
            return {};
        }
        
        // Search mode - try at each position
        for (auto it = begin; it != end; ++it) {
            current = it;
            if (match_string(pattern, current, end)) {
                return match_result<Iterator>{it, current};
            }
        }
        return {};
    }
    
private:
    template<typename Iterator>
    static bool match_string(std::string_view pattern, Iterator& current, Iterator end) {
        std::size_t pos = 0;
        
        while (pos < pattern.size() && current != end) {
            char c = pattern[pos];
            
            if (c == '\\' && pos + 1 < pattern.size()) {
                ++pos;
                char escaped = pattern[pos];
                switch (escaped) {
                    case 'd':
                        if (!match_digit(current, end)) return false;
                        break;
                    case 'w':
                        if (!match_word(current, end)) return false;
                        break;
                    case 's':
                        if (!match_whitespace(current, end)) return false;
                        break;
                    default:
                        if (!match_char_literal(escaped, current, end)) return false;
                        break;
                }
            } else if (c == '.') {
                if (!match_any(current, end)) return false;
            } else if (c == '[') {
                auto close_pos = pattern.find(']', pos + 1);
                if (close_pos == std::string_view::npos) return false;
                
                auto char_class = pattern.substr(pos + 1, close_pos - pos - 1);
                if (!match_char_class(char_class, current, end)) return false;
                pos = close_pos;
            } else if (c == '*' || c == '+' || c == '?') {
                // Handle quantifiers (simplified)
                return handle_quantifier(pattern, pos, current, end);
            } else if (c == '^') {
                // Start anchor - only match if at beginning
                if (current != begin) return false;
            } else if (c == '$') {
                // End anchor - only match if at end
                return current == end;
            } else if (c == '|') {
                // Alternation - for simplicity, just match the first alternative
                return true;
            } else {
                if (!match_char_literal(c, current, end)) return false;
            }
            
            ++pos;
        }
        
        return pos >= pattern.size();
    }
    
    template<typename Iterator>
    static bool match_char_literal(char expected, Iterator& current, Iterator end) {
        if (current != end && *current == expected) {
            ++current;
            return true;
        }
        return false;
    }
    
    template<typename Iterator>
    static bool match_char_class(std::string_view char_class, Iterator& current, Iterator end) {
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
            } else {
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
    
    template<typename Iterator>
    static bool handle_quantifier(std::string_view pattern, std::size_t& pos, Iterator& current, Iterator end) {
        // Simplified quantifier handling
        char quantifier = pattern[pos];
        
        switch (quantifier) {
            case '*': // Zero or more
                return true; // Always succeeds for *
            case '+': // One or more
                return current != end; // Needs at least one character
            case '?': // Zero or one
                return true; // Always succeeds for ?
            default:
                return false;
        }
    }
};

// Main regex class - much simpler than the constexpr version
template<char... Chars>
class basic_regex {
    static constexpr auto validation = parse_result<Chars...>{}; // Validates at compile time
    static constexpr auto pattern_obj = pattern_string<Chars...>{};
    static constexpr std::string_view pattern_view{pattern_obj.data, pattern_obj.len};
    
public:
    constexpr basic_regex() = default;
    
    template<typename Iterator>
    match_result<Iterator> match(Iterator begin, Iterator end) const {
        return matcher::match_pattern(pattern_view, begin, end, true);
    }
    
    template<typename Iterator>
    match_result<Iterator> search(Iterator begin, Iterator end) const {
        return matcher::match_pattern(pattern_view, begin, end, false);
    }
    
    match_result<const char*> match(std::string_view text) const {
        return match(text.data(), text.data() + text.size());
    }
    
    match_result<const char*> search(std::string_view text) const {
        return search(text.data(), text.data() + text.size());
    }
    
    // Get the pattern for debugging
    constexpr std::string_view pattern() const {
        return pattern_view;
    }
};

// Convenience functions
template<char... Chars>
constexpr auto make_regex() {
    return basic_regex<Chars...>{};
}

// User-defined literal - FIXED VERSION
namespace literals {
    // Character pack template - this is the correct way for string literals
    template<char... Chars>
    constexpr auto operator""_re() {
        return basic_regex<Chars...>{};
    }
}

} // namespace glz
