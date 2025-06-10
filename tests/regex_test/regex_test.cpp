#include "glaze/regex/regex.hpp"
#include <iostream>
#include <vector>

int main() {
    // using namespace glz::literals; // Removed
    
    // 1. Basic pattern matching
    std::cout << "1. Basic Pattern Matching:\n";
    auto hello_regex = glz::re<"hello">();
    std::cout << "  Pattern 'hello' matches 'hello': " 
              << std::boolalpha << hello_regex.match("hello").matched << "\n";
    std::cout << "  Pattern 'hello' matches 'world': " 
              << std::boolalpha << hello_regex.match("world").matched << "\n\n";
    
    // 2. Character classes
    std::cout << "2. Character Classes:\n";
    auto digit_regex = glz::re<R"(\d+)">();
    auto word_regex = glz::re<R"(\w+)">();
    
    std::string text = "Hello123 World";
    auto digit_match = digit_regex.search(text);
    auto word_match = word_regex.search(text);
    
    if (digit_match) {
        std::cout << "  Found digits: '" << digit_match.view() << "'\n";
    }
    if (word_match) {
        std::cout << "  Found word: '" << word_match.view() << "'\n";
    }
    std::cout << "\n";
    
    // 3. Email validation
    std::cout << "3. Email Validation:\n";
    auto email_regex = glz::re<R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})">();
    
    std::vector<std::string> emails = {
        "valid@example.com",
        "test.email@domain.org",
        "invalid.email",
        "user@test.co.uk"
    };
    
    for (const auto& email : emails) {
        bool is_valid = email_regex.match(email).matched;
        std::cout << "  " << email << ": " << (is_valid ? "Valid" : "Invalid") << "\n";
    }
    std::cout << "\n";
    
    // 4. Text extraction
    std::cout << "4. Text Extraction:\n";
    auto phone_regex = glz::re<R"(\d{3}-\d{3}-\d{4})">();
    auto url_regex = glz::re<R"(https?://[^\s]+)">();
    
    std::string contact_info = "Call us at 555-123-4567 or visit https://example.com";
    
    auto phone_match = phone_regex.search(contact_info);
    auto url_match = url_regex.search(contact_info);
    
    if (phone_match) {
        std::cout << "  Phone: " << phone_match.view() << "\n";
    }
    if (url_match) {
        std::cout << "  URL: " << url_match.view() << "\n";
    }
    std::cout << "\n";
    
    // 5. Pattern validation at compile time
    std::cout << "5. Compile-time Pattern Validation:\n";
    std::cout << "  All patterns above were validated at compile time!\n";
    std::cout << "  Try uncommenting the line below to see a compile error:\n";
    std::cout << "  // auto bad_regex = glz::re<\"unclosed[bracket\">();  // Compilation error!\n\n";
    
    // Uncomment to test compile-time error:
    // auto bad_regex = glz::re<"unclosed[bracket">();
    
    std::cout << "Benefits of this approach:\n";
    std::cout << "✓ Pattern validation at compile time\n";
    std::cout << "✓ Fast runtime execution (no pattern compilation overhead)\n";
    std::cout << "✓ Simple, readable code\n";
    std::cout << "✓ Clear error messages\n";
    std::cout << "✓ Easy to debug and maintain\n";
    std::cout << "✓ Type-safe regex objects\n";
    
    return 0;
}
