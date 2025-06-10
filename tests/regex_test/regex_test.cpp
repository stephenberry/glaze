#include "glaze/regex/regex.hpp"
#include "ut/ut.hpp"
#include <vector>
#include <string>

using namespace ut;

suite diagnostic_tests = [] {
   "regex_engine_basic_functionality"_test = [] {
      auto hello_regex = glz::re<"hello">();
      expect(hello_regex.pattern() == "hello") << "Pattern should be accessible\n";
      
      auto result = hello_regex.match("hello");
      expect(result.matched) << "Basic literal match should work\n";
      
      if (result.matched) {
         expect(result.view() == "hello") << "View should return matched text\n";
      }
   };
   
   "basic_character_classes"_test = [] {
      auto digit_regex = glz::re<R"(\d)">();
      expect(digit_regex.match("5").matched) << "Single digit should match \\d\n";
      expect(not digit_regex.match("a").matched) << "Letter should not match \\d\n";
   };
   
   "basic_dot_metacharacter"_test = [] {
      auto dot_regex = glz::re<"a.c">();
      expect(dot_regex.match("abc").matched) << "Should match with any character in middle\n";
      expect(dot_regex.match("a5c").matched) << "Should match with digit in middle\n";
   };
};

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
   "simple_email_pattern_works"_test = [] {
      // Start with a simpler pattern to test basic functionality
      auto simple_email = glz::re<R"(\w+@\w+\.\w+)">();
      auto result = simple_email.match("user@test.com");
      expect(result.matched) << "Simple email pattern should work\n";
   };
   
   "character_class_basic_test"_test = [] {
      // Test if character classes work at all
      auto letter_regex = glz::re<"[a-z]">();
      expect(letter_regex.match("a").matched) << "Single letter in range should match\n";
      expect(not letter_regex.match("A").matched) << "Uppercase letter should not match lowercase range\n";
   };
   
   "complex_email_addresses_test"_test = [] {
      // Use search instead of match to see if the pattern works at all
      auto email_regex = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
      
      std::vector<std::string> valid_emails = {
         "valid@example.com",
         "test.email@domain.org",
         "user@test.co.uk"
      };
      
      for (const auto& email : valid_emails) {
         auto result = email_regex.search(email); // Use search instead of match
         expect(result.matched) << "Email '" << email << "' should be found with search\n";
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
         auto result = email_regex.search(email); // Use search to be consistent
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
      expect(not question_regex.match("12").matched) << "Should not match string longer than pattern\n";
      // Additional test to show search behavior works correctly:
      expect(question_regex.search("12").matched) << "Should find single digit in search mode\n";
      expect(question_regex.search("12").view() == "1") << "Should extract only first digit\n";
   };
};

suite anchor_tests = [] {
   "start_anchor_matches_beginning_of_string"_test = [] {
      auto start_anchor_regex = glz::re<"^hello">();
      
      expect(start_anchor_regex.match("hello world").matched) << "Should match 'hello' at start\n";
      expect(not start_anchor_regex.search("say hello").matched) << "Should not find 'hello' not at start with ^ anchor\n";
   };
   
   "end_anchor_basic_test"_test = [] {
      // Test a simpler case first
      auto end_anchor_regex = glz::re<"world$">();
      auto result = end_anchor_regex.search("hello world");
      
      // Let's see what actually gets matched
      if (result.matched) {
         // This will help us understand what's happening
         expect(result.view() == "world") << "End anchor should match 'world', got: '" << result.view() << "'\n";
      } else {
         expect(false) << "End anchor should match 'world' at end of 'hello world'\n";
      }
   };
   
   "end_anchor_with_match_method"_test = [] {
      // Try with match instead of search
      auto end_anchor_regex = glz::re<"world$">();
      auto result = end_anchor_regex.match("world");
      expect(result.matched) << "Should match 'world' when it's the entire string\n";
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
   "match_works_for_exact_match"_test = [] {
      auto hello_regex = glz::re<"hello">();
      auto result = hello_regex.match("hello");
      expect(result.matched) << "match() should succeed for exact match\n";
      expect(result.view() == "hello") << "Should match entire string\n";
   };
   
   "match_behavior_with_longer_string"_test = [] {
      auto hello_regex = glz::re<"hello">();
      auto result = hello_regex.match("hello world");
      
      // Let's see what actually happens
      if (result.matched) {
         // If it matches, let's see what it matched
         expect(result.view() == "hello world") << "If match() succeeds on 'hello world', it should match the whole string, got: '" << result.view() << "'\n";
      } else {
         // This is what we expect - match should fail for partial matches
         expect(true) << "match() correctly failed for partial match\n";
      }
   };
   
   "search_finds_pattern_anywhere_in_string"_test = [] {
      auto hello_regex = glz::re<"hello">();
      
      expect(hello_regex.search("hello").matched) << "search() should find exact match\n";
      expect(hello_regex.search("hello world").matched) << "search() should find pattern at start\n";
      expect(hello_regex.search("say hello").matched) << "search() should find pattern at end\n";
      expect(hello_regex.search("say hello world").matched) << "search() should find pattern in middle\n";
      expect(not hello_regex.search("hi there").matched) << "search() should fail when pattern not found\n";
   };
   
   "search_returns_correct_substring"_test = [] {
      auto hello_regex = glz::re<"hello">();
      auto result = hello_regex.search("say hello world");
      expect(result.matched) << "Should find 'hello' in middle of string\n";
      expect(result.view() == "hello") << "Should return just the matched part\n";
   };
};

suite email_regex_debugging = [] {
   "test_individual_email_components"_test = [] {
      // Test each part of the email pattern separately
      
      // Test first character class: should match "valid"
      auto first_part = glz::re<R"([a-zA-Z0-9._%+-]+)">();
      auto result1 = first_part.search("valid");
      expect(result1.matched) << "First part should match 'valid'\n";
      if (result1.matched) {
         expect(result1.view() == "valid") << "Should extract 'valid', got: '" << result1.view() << "'\n";
      }
      
      // Test @ symbol
      auto at_symbol = glz::re<"@">();
      auto result2 = at_symbol.search("@");
      expect(result2.matched) << "@ symbol should match\n";
      
      // Test second character class: should match "example"
      auto second_part = glz::re<R"([a-zA-Z0-9.-]+)">();
      auto result3 = second_part.search("example");
      expect(result3.matched) << "Second part should match 'example'\n";
      if (result3.matched) {
         expect(result3.view() == "example") << "Should extract 'example', got: '" << result3.view() << "'\n";
      }
      
      // Test escaped dot
      auto dot_part = glz::re<R"(\.)">();
      auto result4 = dot_part.search(".");
      expect(result4.matched) << "Escaped dot should match literal dot\n";
      
      // Test final character class with quantifier: should match "com"
      auto final_part = glz::re<R"([a-zA-Z]{2,})">();
      auto result5 = final_part.search("com");
      expect(result5.matched) << "Final part should match 'com'\n";
      if (result5.matched) {
         expect(result5.view() == "com") << "Should extract 'com', got: '" << result5.view() << "'\n";
      }
   };
   
   "test_simplified_email_patterns"_test = [] {
      // Try progressively simpler email patterns to isolate the issue
      
      // Simplest: just letters
      auto simple1 = glz::re<R"([a-z]+@[a-z]+\.[a-z]{2,})">();
      auto result1 = simple1.search("valid@example.com");
      expect(result1.matched) << "Simple lowercase pattern should work\n";
      
      // Add uppercase
      auto simple2 = glz::re<R"([a-zA-Z]+@[a-zA-Z]+\.[a-zA-Z]{2,})">();
      auto result2 = simple2.search("valid@example.com");
      expect(result2.matched) << "Letter-only pattern should work\n";
      
      // Add digits
      auto simple3 = glz::re<R"([a-zA-Z0-9]+@[a-zA-Z0-9]+\.[a-zA-Z]{2,})">();
      auto result3 = simple3.search("valid@example.com");
      expect(result3.matched) << "Letters and digits pattern should work\n";
      
      // Add just the dot in first part
      auto simple4 = glz::re<R"([a-zA-Z0-9.]+@[a-zA-Z0-9]+\.[a-zA-Z]{2,})">();
      auto result4 = simple4.search("test.email@domain.org");
      expect(result4.matched) << "Pattern with dot in first part should work\n";
   };
   
   "test_character_class_edge_cases"_test = [] {
      // Test the specific character classes that might be problematic
      
      // Test character class with dash at end
      auto dash_end = glz::re<R"([a-zA-Z0-9.-])">();
      expect(dash_end.search("a").matched) << "Should match letter\n";
      expect(dash_end.search("5").matched) << "Should match digit\n";
      expect(dash_end.search(".").matched) << "Should match dot\n";
      expect(dash_end.search("-").matched) << "Should match dash at end\n";
      
      // Test character class with special chars
      auto special_chars = glz::re<R"([._%+-])">();
      expect(special_chars.search(".").matched) << "Should match dot\n";
      expect(special_chars.search("_").matched) << "Should match underscore\n";
      expect(special_chars.search("%").matched) << "Should match percent\n";
      expect(special_chars.search("+").matched) << "Should match plus\n";
      expect(special_chars.search("-").matched) << "Should match dash\n";
      
      // Test the full complex character class
      auto full_class = glz::re<R"([a-zA-Z0-9._%+-])">();
      expect(full_class.search("v").matched) << "Full class should match 'v'\n";
      expect(full_class.search("@").matched == false) << "Full class should NOT match '@'\n";
   };
   
   "test_quantifier_edge_cases"_test = [] {
      // Test the {2,} quantifier specifically
      auto quant_test = glz::re<R"([a-zA-Z]{2,})">();
      expect(quant_test.search("a").matched == false) << "Should not match single character\n";
      expect(quant_test.search("ab").matched) << "Should match 2 characters\n";
      expect(quant_test.search("abc").matched) << "Should match 3 characters\n";
      expect(quant_test.search("com").matched) << "Should match 'com'\n";
      
      if (quant_test.search("com").matched) {
         expect(quant_test.search("com").view() == "com") << "Should extract 'com'\n";
      }
   };
   
   "test_full_pattern_step_by_step"_test = [] {
      // Test building up the full pattern piece by piece
      auto email_regex = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
      
      // Test with a very simple email first
      auto simple_result = email_regex.search("a@b.co");
      expect(simple_result.matched) << "Should match simple email 'a@b.co'\n";
      
      // Test with the problematic email
      auto complex_result = email_regex.search("valid@example.com");
      
      // Print debug info regardless of success/failure
      std::string debug_msg = "Pattern: " + std::string(email_regex.pattern()) +
      ", Text: 'valid@example.com', Matched: " +
      (complex_result.matched ? "true" : "false");
      expect(true) << debug_msg << "\n"; // Always passes, just for debug output
      
      expect(complex_result.matched) << "Should match 'valid@example.com'\n";
   };
};

suite comprehensive_regex_debug = [] {
   "debug_character_class_basics"_test = [] {
      // Test the most basic character classes first
      auto test1 = glz::re<"[a]">();
      expect(test1.search("a").matched) << "Single char class should work\n";
      
      auto test2 = glz::re<"[abc]">();
      expect(test2.search("b").matched) << "Multi char class should work\n";
      
      auto test3 = glz::re<"[a-c]">();
      expect(test3.search("b").matched) << "Simple range should work\n";
      
      auto test4 = glz::re<"[a-zA-Z]">();
      expect(test4.search("b").matched) << "Double range should work\n";
      expect(test4.search("B").matched) << "Double range should work for uppercase\n";
      
      auto test5 = glz::re<"[a-zA-Z0-9]">();
      expect(test5.search("b").matched) << "Triple range should work for letter\n";
      expect(test5.search("5").matched) << "Triple range should work for digit\n";
   };
   
   "debug_literal_characters_in_class"_test = [] {
      // Test problematic literal characters
      auto test1 = glz::re<"[.]">();
      expect(test1.search(".").matched) << "Literal dot in class should work\n";
      
      auto test2 = glz::re<"[_]">();
      expect(test2.search("_").matched) << "Underscore in class should work\n";
      
      auto test3 = glz::re<"[%]">();
      expect(test3.search("%").matched) << "Percent in class should work\n";
      
      auto test4 = glz::re<"[+]">();
      expect(test4.search("+").matched) << "Plus in class should work\n";
      
      auto test5 = glz::re<"[-]">();
      expect(test5.search("-").matched) << "Dash alone in class should work\n";
      
      auto test6 = glz::re<"[a-]">();
      expect(test6.search("-").matched) << "Dash at end should work\n";
      expect(test6.search("a").matched) << "Letter with dash at end should work\n";
   };
   
   "debug_complex_character_classes"_test = [] {
      // Build up to the problematic character class step by step
      auto test1 = glz::re<"[a-zA-Z.]">();
      expect(test1.search("a").matched) << "Letters + dot should work\n";
      expect(test1.search(".").matched) << "Dot should match in letters + dot\n";
      
      auto test2 = glz::re<"[a-zA-Z0-9.]">();
      expect(test2.search("5").matched) << "Letters + digits + dot should work\n";
      
      auto test3 = glz::re<"[a-zA-Z0-9._%+-]">();
      expect(test3.search("v").matched) << "Full first character class should match 'v'\n";
      expect(test3.search("a").matched) << "Full first character class should match 'a'\n";
      expect(test3.search("l").matched) << "Full first character class should match 'l'\n";
      expect(test3.search("i").matched) << "Full first character class should match 'i'\n";
      expect(test3.search("d").matched) << "Full first character class should match 'd'\n";
      
      auto test4 = glz::re<"[a-zA-Z0-9.-]">();
      expect(test4.search("e").matched) << "Second character class should match 'e'\n";
      expect(test4.search("x").matched) << "Second character class should match 'x'\n";
   };
   
   "debug_quantifiers_separately"_test = [] {
      // Test quantifiers with simple patterns
      auto test1 = glz::re<"a+">();
      expect(test1.search("aaa").matched) << "Simple + quantifier should work\n";
      
      auto test2 = glz::re<"[a]+">();
      expect(test2.search("aaa").matched) << "Character class + quantifier should work\n";
      
      auto test3 = glz::re<"[a-z]+">();
      expect(test3.search("valid").matched) << "Range + quantifier should work\n";
      
      auto test4 = glz::re<"[a-zA-Z]+">();
      expect(test4.search("valid").matched) << "Double range + quantifier should work\n";
      
      auto test5 = glz::re<"[a-zA-Z]{2,}">();
      expect(test5.search("valid").matched) << "{2,} quantifier should work\n";
      expect(test5.search("a").matched == false) << "{2,} should reject single char\n";
   };
   
   "debug_email_pattern_piece_by_piece"_test = [] {
      // Test the email pattern by building it up piece by piece
      std::string email = "valid@example.com";
      
      // Test just the first part
      auto part1 = glz::re<R"([a-zA-Z0-9._%+-]+)">();
      auto result1 = part1.search(email);
      expect(result1.matched) << "First part should match something in email\n";
      if (result1.matched) {
         expect(result1.view() == "valid") << "First part should match 'valid', got: '" << result1.view() << "'\n";
      }
      
      // Test first part + @
      auto part2 = glz::re<R"([a-zA-Z0-9._%+-]+@)">();
      auto result2 = part2.search(email);
      expect(result2.matched) << "First part + @ should match\n";
      if (result2.matched) {
         expect(result2.view() == "valid@") << "Should match 'valid@', got: '" << result2.view() << "'\n";
      }
      
      // Test first part + @ + second part
      auto part3 = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+)">();
      auto result3 = part3.search(email);
      expect(result3.matched) << "First two parts should match\n";
      if (result3.matched) {
         expect(result3.view() == "valid@example") << "Should match 'valid@example', got: '" << result3.view() << "'\n";
      }
      
      // Test everything except the final quantifier
      auto part4 = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z])">();
      auto result4 = part4.search(email);
      expect(result4.matched) << "Everything except quantifier should match\n";
      
      // Test the full pattern
      auto full = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
      auto result_full = full.search(email);
      expect(result_full.matched) << "Full pattern should match 'valid@example.com'\n";
      if (result_full.matched) {
         expect(result_full.view() == "valid@example.com") << "Should match entire email, got: '" << result_full.view() << "'\n";
      }
   };
   
   "debug_pattern_string_construction"_test = [] {
      // Test that the pattern string is being constructed correctly
      auto email_regex = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
      
      std::string pattern_str = std::string(email_regex.pattern());
      std::string expected = R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})";
      
      expect(pattern_str == expected) << "Pattern string should be constructed correctly\n"
      << "Expected: '" << expected << "'\n"
      << "Got: '" << pattern_str << "'\n";
      
      // Also test pattern length
      expect(pattern_str.length() == expected.length()) << "Pattern length should match\n";
   };
};

int main() {}
