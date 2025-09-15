// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/util/key_transformers.hpp"

#include <iostream>
#include <string>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Test renaming functions directly
suite key_transformer_functions = [] {
   "camel_case"_test = [] {
      expect(glz::to_camel_case("hello_world") == "helloWorld");
      expect(glz::to_camel_case("is_active") == "isActive");
      expect(glz::to_camel_case("pi_value") == "piValue");
      expect(glz::to_camel_case("url_endpoint") == "urlEndpoint");
      expect(glz::to_camel_case("http_status") == "httpStatus");
      expect(glz::to_camel_case("api_key") == "apiKey");
      expect(glz::to_camel_case("use_ssl") == "useSsl");
      expect(glz::to_camel_case("single") == "single");
      expect(glz::to_camel_case("") == "");
      expect(glz::to_camel_case("_leading_underscore") == "LeadingUnderscore");
      expect(glz::to_camel_case("trailing_underscore_") == "trailingUnderscore");
      expect(glz::to_camel_case("multiple___underscores") == "multipleUnderscores");
   };

   "pascal_case"_test = [] {
      expect(glz::to_pascal_case("hello_world") == "HelloWorld");
      expect(glz::to_pascal_case("is_active") == "IsActive");
      expect(glz::to_pascal_case("pi_value") == "PiValue");
      expect(glz::to_pascal_case("url_endpoint") == "UrlEndpoint");
      expect(glz::to_pascal_case("http_status") == "HttpStatus");
      expect(glz::to_pascal_case("api_key") == "ApiKey");
      expect(glz::to_pascal_case("use_ssl") == "UseSsl");
      expect(glz::to_pascal_case("single") == "Single");
      expect(glz::to_pascal_case("") == "");
   };

   "snake_case"_test = [] {
      expect(glz::to_snake_case("helloWorld") == "hello_world");
      expect(glz::to_snake_case("HelloWorld") == "hello_world");
      expect(glz::to_snake_case("myVariableName") == "my_variable_name");
      expect(glz::to_snake_case("XMLParser") == "xml_parser");
      expect(glz::to_snake_case("IOSpeed") == "io_speed");
      expect(glz::to_snake_case("HTTPSConnection") == "https_connection");
      expect(glz::to_snake_case("getHTTPResponseCode") == "get_http_response_code");
      expect(glz::to_snake_case("single") == "single");
      expect(glz::to_snake_case("UPPERCASE") == "uppercase");
      expect(glz::to_snake_case("") == "");
   };

   "screaming_snake_case"_test = [] {
      expect(glz::to_screaming_snake_case("helloWorld") == "HELLO_WORLD");
      expect(glz::to_screaming_snake_case("hello_world") == "HELLO_WORLD");
      expect(glz::to_screaming_snake_case("HelloWorld") == "HELLO_WORLD");
      expect(glz::to_screaming_snake_case("myVariableName") == "MY_VARIABLE_NAME");
      expect(glz::to_screaming_snake_case("XMLParser") == "XML_PARSER");
      expect(glz::to_screaming_snake_case("IOSpeed") == "IO_SPEED");
      expect(glz::to_screaming_snake_case("HTTPSConnection") == "HTTPS_CONNECTION");
      expect(glz::to_screaming_snake_case("single") == "SINGLE");
      expect(glz::to_screaming_snake_case("") == "");
   };

   "kebab_case"_test = [] {
      expect(glz::to_kebab_case("helloWorld") == "hello-world");
      expect(glz::to_kebab_case("hello_world") == "hello-world");
      expect(glz::to_kebab_case("HelloWorld") == "hello-world");
      expect(glz::to_kebab_case("myVariableName") == "my-variable-name");
      expect(glz::to_kebab_case("XMLParser") == "xml-parser");
      expect(glz::to_kebab_case("IOSpeed") == "io-speed");
      expect(glz::to_kebab_case("HTTPSConnection") == "https-connection");
      expect(glz::to_kebab_case("getHTTPResponseCode") == "get-http-response-code");
      expect(glz::to_kebab_case("single") == "single");
      expect(glz::to_kebab_case("") == "");
   };

   "screaming_kebab_case"_test = [] {
      expect(glz::to_screaming_kebab_case("helloWorld") == "HELLO-WORLD");
      expect(glz::to_screaming_kebab_case("hello_world") == "HELLO-WORLD");
      expect(glz::to_screaming_kebab_case("HelloWorld") == "HELLO-WORLD");
      expect(glz::to_screaming_kebab_case("myVariableName") == "MY-VARIABLE-NAME");
      expect(glz::to_screaming_kebab_case("XMLParser") == "XML-PARSER");
      expect(glz::to_screaming_kebab_case("IOSpeed") == "IO-SPEED");
      expect(glz::to_screaming_kebab_case("HTTPSConnection") == "HTTPS-CONNECTION");
      expect(glz::to_screaming_kebab_case("single") == "SINGLE");
      expect(glz::to_screaming_kebab_case("") == "");
   };

   "lower_case"_test = [] {
      expect(glz::to_lower_case("HelloWorld") == "helloworld");
      expect(glz::to_lower_case("UPPERCASE") == "uppercase");
      expect(glz::to_lower_case("mixedCase") == "mixedcase");
      expect(glz::to_lower_case("lowercase") == "lowercase");
      expect(glz::to_lower_case("123ABC") == "123abc");
      expect(glz::to_lower_case("") == "");
   };

   "upper_case"_test = [] {
      expect(glz::to_upper_case("HelloWorld") == "HELLOWORLD");
      expect(glz::to_upper_case("UPPERCASE") == "UPPERCASE");
      expect(glz::to_upper_case("mixedCase") == "MIXEDCASE");
      expect(glz::to_upper_case("lowercase") == "LOWERCASE");
      expect(glz::to_upper_case("123abc") == "123ABC");
      expect(glz::to_upper_case("") == "");
   };
};

// Test structures for JSON serialization
struct test_struct_camel
{
   int i_value = 287;
   std::string hello_world = "Hello World";
   bool is_active = true;
};

template <>
struct glz::meta<test_struct_camel> : glz::camel_case
{};

struct test_struct_pascal
{
   int user_id = 123;
   std::string first_name = "John";
   bool is_admin = false;
};

template <>
struct glz::meta<test_struct_pascal> : glz::pascal_case
{};

// NOTE: kebab_case with multiple underscore fields triggers a Clang constexpr issue
// struct test_struct_kebab {
//    int user_id = 456;
//    std::string email_address = "user@example.com";
// };

// template <>
// struct glz::meta<test_struct_kebab> : glz::kebab_case {};

struct test_struct_screaming
{
   std::string api_key = "SECRET123";
   int max_retries = 3;
};

template <>
struct glz::meta<test_struct_screaming> : glz::screaming_snake_case
{};

// Test JSON serialization with meta specializations
suite json_serialization_tests = [] {
   "camel_case_serialization"_test = [] {
      test_struct_camel obj{};
      auto json = glz::write_json(obj);
      expect(json.has_value());

      // Check that the JSON contains camel-cased keys
      auto& result = json.value();
      expect(result.find("iValue") != std::string::npos);
      expect(result.find("helloWorld") != std::string::npos);
      expect(result.find("isActive") != std::string::npos);

      // Test round-trip
      test_struct_camel parsed{};
      parsed.i_value = 0;
      parsed.hello_world = "";
      parsed.is_active = false;

      auto err = glz::read_json(parsed, result);
      expect(!err);
      expect(parsed.i_value == obj.i_value);
      expect(parsed.hello_world == obj.hello_world);
      expect(parsed.is_active == obj.is_active);
   };

   "pascal_case_serialization"_test = [] {
      test_struct_pascal obj{};
      auto json = glz::write_json(obj);
      expect(json.has_value());

      // Check that the JSON contains Pascal-cased keys
      auto& result = json.value();
      expect(result.find("UserId") != std::string::npos);
      expect(result.find("FirstName") != std::string::npos);
      expect(result.find("IsAdmin") != std::string::npos);

      // Test round-trip
      test_struct_pascal parsed{};
      parsed.user_id = 0;
      parsed.first_name = "";
      parsed.is_admin = true;

      auto err = glz::read_json(parsed, result);
      expect(!err);
      expect(parsed.user_id == obj.user_id);
      expect(parsed.first_name == obj.first_name);
      expect(parsed.is_admin == obj.is_admin);
   };

   // "kebab_case_serialization"_test = [] {
   //    test_struct_kebab obj{};
   //    auto json = glz::write_json(obj);
   //    expect(json.has_value());
   //
   //    // Check that the JSON contains kebab-cased keys
   //    auto& result = json.value();
   //    expect(result.find("user-id") != std::string::npos);
   //    expect(result.find("email-address") != std::string::npos);
   //
   //    // Test round-trip
   //    test_struct_kebab parsed{};
   //    parsed.user_id = 0;
   //    parsed.email_address = "";
   //
   //    auto err = glz::read_json(parsed, result);
   //    expect(!err);
   //    expect(parsed.user_id == obj.user_id);
   //    expect(parsed.email_address == obj.email_address);
   // };

   "screaming_snake_case_serialization"_test = [] {
      test_struct_screaming obj{};
      auto json = glz::write_json(obj);
      expect(json.has_value());

      // Check that the JSON contains SCREAMING_SNAKE_CASE keys
      auto& result = json.value();
      expect(result.find("API_KEY") != std::string::npos);
      expect(result.find("MAX_RETRIES") != std::string::npos);

      // Test round-trip
      test_struct_screaming parsed{};
      parsed.api_key = "";
      parsed.max_retries = 0;

      auto err = glz::read_json(parsed, result);
      expect(!err);
      expect(parsed.api_key == obj.api_key);
      expect(parsed.max_retries == obj.max_retries);
   };
};

// Test edge cases
suite edge_cases = [] {
   "empty_strings"_test = [] {
      expect(glz::to_camel_case("") == "");
      expect(glz::to_pascal_case("") == "");
      expect(glz::to_snake_case("") == "");
      expect(glz::to_kebab_case("") == "");
   };

   "single_character"_test = [] {
      expect(glz::to_camel_case("a") == "a");
      expect(glz::to_pascal_case("a") == "A");
      expect(glz::to_snake_case("A") == "a");
      expect(glz::to_kebab_case("A") == "a");
   };

   "numbers_in_names"_test = [] {
      expect(glz::to_camel_case("variable_1") == "variable1");
      expect(glz::to_camel_case("test_2_value") == "test2Value");
      expect(glz::to_snake_case("variable1") == "variable1");
      expect(glz::to_snake_case("test2Value") == "test2_value");
   };

   "consecutive_capitals"_test = [] {
      expect(glz::to_snake_case("XMLHTTPRequest") == "xmlhttp_request");
      expect(glz::to_snake_case("IOController") == "io_controller");
      expect(glz::to_kebab_case("XMLHTTPRequest") == "xmlhttp-request");
      expect(glz::to_kebab_case("IOController") == "io-controller");
   };
};

int main()
{
   std::cout << "Running key transformer tests...\n";
   return 0;
}