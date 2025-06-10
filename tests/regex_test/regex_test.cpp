#include "glaze/regex/regex.hpp"
#include "ut/ut.hpp"
#include <vector>
#include <string>

using namespace ut;

suite basic_pattern_matching_tests = [] {
   "hello_pattern_matches_hello_string"_test = [] {
      auto hello_regex = glz::re<"hello">();
      auto result = hello_regex.match("hello");
      expect(result.matched) << "Pattern 'hello' should match string 'hello'\n";
      expect(result.view() == "hello") << "Matched text should be 'hello'\n";
   };
   
   "hello_pattern_does_not_match_world_string"_test = [] {
      auto hello_regex = glz::re<"hello">();
      auto result = hello_regex.match("world");
      expect(not result.matched) << "Pattern 'hello' should not match string 'world'\n";
   };
   
   "pattern_returns_correct_view"_test = [] {
      auto hello_regex = glz::re<"hello">();
      auto result = hello_regex.match("hello");
      expect(result.view() == "hello") << "view() should return the matched text\n";
   };
};

suite character_class_tests = [] {
   "digit_regex_finds_numbers_in_text"_test = [] {
      auto digit_regex = glz::re<R"(\d+)">();
      std::string text = "Hello123 World";
      auto digit_match = digit_regex.search(text);
      
      expect(digit_match.matched) << "Digit regex should find numbers in text\n";
      expect(digit_match.view() == "123") << "Should extract '123' from 'Hello123 World'\n";
   };
   
   "word_regex_finds_words_in_text"_test = [] {
      auto word_regex = glz::re<R"(\w+)">();
      std::string text = "Hello123 World";
      auto word_match = word_regex.search(text);
      
      expect(word_match.matched) << "Word regex should find word characters in text\n";
      expect(word_match.view() == "Hello123") << "Should extract 'Hello123' from 'Hello123 World'\n";
   };
   
   "whitespace_regex_matches_spaces"_test = [] {
      auto whitespace_regex = glz::re<R"(\s+)">();
      std::string text = "Hello World";
      auto ws_match = whitespace_regex.search(text);
      
      expect(ws_match.matched) << "Whitespace regex should find spaces\n";
      expect(ws_match.view() == " ") << "Should match the space between words\n";
   };
};

suite email_validation_tests = [] {
   "valid_email_addresses_should_match"_test = [] {
      auto email_regex = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
      
      std::vector<std::string> valid_emails = {
         "valid@example.com",
         "test.email@domain.org",
         "user@test.co.uk"
      };
      
      for (const auto& email : valid_emails) {
         auto result = email_regex.match(email);
         expect(result.matched) << "Email '" << email << "' should be valid\n";
         expect(result.view() == email) << "Should match the entire email string\n";
      }
   };
   
   "invalid_email_addresses_should_not_match"_test = [] {
      auto email_regex = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
      
      std::vector<std::string> invalid_emails = {
         "invalid.email",
         "@domain.com",
         "user@",
         "user@domain"
      };
      
      for (const auto& email : invalid_emails) {
         auto result = email_regex.match(email);
         expect(not result.matched) << "Email '" << email << "' should be invalid\n";
      }
   };
};

suite text_extraction_tests = [] {
   "phone_number_extraction"_test = [] {
      auto phone_regex = glz::re<R"(\d{3}-\d{3}-\d{4})">();
      std::string contact_info = "Call us at 555-123-4567 or visit our website";
      auto phone_match = phone_regex.search(contact_info);
      
      expect(phone_match.matched) << "Should find phone number in contact info\n";
      expect(phone_match.view() == "555-123-4567") << "Should extract correct phone number\n";
   };
   
   "url_extraction"_test = [] {
      auto url_regex = glz::re<R"(https?://[^\s]+)">();
      std::string contact_info = "Call us at 555-123-4567 or visit https://example.com";
      auto url_match = url_regex.search(contact_info);
      
      expect(url_match.matched) << "Should find URL in contact info\n";
      expect(url_match.view() == "https://example.com") << "Should extract correct URL\n";
   };
   
   "multiple_pattern_extraction_from_same_text"_test = [] {
      auto phone_regex = glz::re<R"(\d{3}-\d{3}-\d{4})">();
      auto url_regex = glz::re<R"(https?://[^\s]+)">();
      std::string contact_info = "Call us at 555-123-4567 or visit https://example.com";
      
      auto phone_match = phone_regex.search(contact_info);
      auto url_match = url_regex.search(contact_info);
      
      expect(phone_match.matched and url_match.matched)
      << "Should extract both phone and URL from same text\n";
      expect(phone_match.view() == "555-123-4567") << "Phone extraction should be correct\n";
      expect(url_match.view() == "https://example.com") << "URL extraction should be correct\n";
   };
};

suite advanced_pattern_tests = [] {
   "dot_metacharacter_matches_any_character"_test = [] {
      auto dot_regex = glz::re<"h.llo">();
      
      expect(dot_regex.match("hello").matched) << "Should match 'hello'\n";
      expect(dot_regex.match("hallo").matched) << "Should match 'hallo'\n";
      expect(dot_regex.match("h3llo").matched) << "Should match 'h3llo'\n";
      expect(not dot_regex.match("hllo").matched) << "Should not match 'hllo' (missing character)\n";
   };
   
   "character_ranges_work_correctly"_test = [] {
      auto range_regex = glz::re<"[a-z]+">();
      
      expect(range_regex.match("hello").matched) << "Should match lowercase letters\n";
      expect(not range_regex.match("HELLO").matched) << "Should not match uppercase letters\n";
      expect(not range_regex.match("123").matched) << "Should not match numbers\n";
   };
   
   "quantifier_plus_works"_test = [] {
      auto plus_regex = glz::re<R"(\d+)">();
      
      expect(plus_regex.match("123").matched) << "Should match one or more digits\n";
      expect(plus_regex.match("1").matched) << "Should match single digit\n";
      expect(not plus_regex.match("").matched) << "Should not match empty string\n";
      expect(not plus_regex.match("abc").matched) << "Should not match non-digits\n";
   };
   
   "quantifier_star_works"_test = [] {
      auto star_regex = glz::re<R"(\d*)">();
      
      expect(star_regex.match("123").matched) << "Should match multiple digits\n";
      expect(star_regex.match("").matched) << "Should match empty string (zero digits)\n";
      expect(star_regex.match("1").matched) << "Should match single digit\n";
   };
   
   "quantifier_question_mark_works"_test = [] {
      auto question_regex = glz::re<R"(\d?)">();
      
      expect(question_regex.match("1").matched) << "Should match single digit\n";
      expect(question_regex.match("").matched) << "Should match empty string\n";
      expect(question_regex.match("12").matched) << "Should match (but only consume first digit)\n";
   };
};

suite anchor_tests = [] {
   "start_anchor_matches_beginning_of_string"_test = [] {
      auto start_anchor_regex = glz::re<"^hello">();
      
      expect(start_anchor_regex.match("hello world").matched) << "Should match 'hello' at start\n";
      expect(not start_anchor_regex.match("say hello").matched) << "Should not match 'hello' not at start\n";
   };
   
   "end_anchor_matches_end_of_string"_test = [] {
      auto end_anchor_regex = glz::re<"world$">();
      
      expect(end_anchor_regex.match("hello world").matched) << "Should match 'world' at end\n";
      expect(not end_anchor_regex.match("world hello").matched) << "Should not match 'world' not at end\n";
   };
};

suite compile_time_validation_tests = [] {
   "valid_patterns_compile_successfully"_test = [] {
      // These should all compile without errors
      auto basic_regex = glz::re<"hello">();
      auto digit_regex = glz::re<R"(\d+)">();
      auto email_regex = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
      auto phone_regex = glz::re<R"(\d{3}-\d{3}-\d{4})">();
      
      // Test that we can get patterns
      expect(basic_regex.pattern() == "hello") << "Pattern should be accessible\n";
      expect(not digit_regex.pattern().empty()) << "Digit pattern should not be empty\n";
      expect(not email_regex.pattern().empty()) << "Email pattern should not be empty\n";
      expect(not phone_regex.pattern().empty()) << "Phone pattern should not be empty\n";
   };
   
   "pattern_accessor_returns_correct_string"_test = [] {
      auto hello_regex = glz::re<"hello">();
      expect(hello_regex.pattern() == "hello") << "pattern() should return the original pattern string\n";
      
      auto digit_regex = glz::re<R"(\d+)">();
      expect(digit_regex.pattern() == R"(\d+)") << "pattern() should return the digit pattern\n";
   };
};

suite search_vs_match_tests = [] {
   "match_requires_full_string_match"_test = [] {
      auto hello_regex = glz::re<"hello">();
      
      expect(hello_regex.match("hello").matched) << "match() should succeed for exact match\n";
      expect(not hello_regex.match("hello world").matched) << "match() should fail for partial match\n";
   };
   
   "search_finds_pattern_anywhere_in_string"_test = [] {
      auto hello_regex = glz::re<"hello">();
      
      expect(hello_regex.search("hello").matched) << "search() should find exact match\n";
      expect(hello_regex.search("hello world").matched) << "search() should find pattern at start\n";
      expect(hello_regex.search("say hello").matched) << "search() should find pattern at end\n";
      expect(hello_regex.search("say hello world").matched) << "search() should find pattern in middle\n";
      expect(not hello_regex.search("hi there").matched) << "search() should fail when pattern not found\n";
   };
};

int main() {}
