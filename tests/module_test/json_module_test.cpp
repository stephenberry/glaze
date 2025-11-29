// Glaze Library
// For the license information refer to glaze.hpp

// Test that the glaze.json module works correctly

// IMPORTANT: Standard library headers and other textual headers must come
// BEFORE module imports to avoid ODR conflicts with headers included in
// the module's global module fragment.
#include <string>

#include "ut/ut.hpp"

import glaze.json;

using namespace ut;

struct Person
{
   std::string name{};
   int age{};
};

suite module_json_tests = [] {
   "write_json"_test = [] {
      Person person{"John", 30};
      auto result = glz::write_json(person);
      expect(result.has_value());
      expect(result.value() == R"({"name":"John","age":30})") << result.value();
   };

   "read_json"_test = [] {
      std::string json = R"({"name":"Jane","age":25})";
      auto result = glz::read_json<Person>(json);
      expect(result.has_value());
      expect(result->name == "Jane");
      expect(result->age == 25);
   };

   "roundtrip"_test = [] {
      Person original{"Alice", 42};

      auto json_result = glz::write_json(original);
      expect(json_result.has_value());

      auto parsed_result = glz::read_json<Person>(json_result.value());
      expect(parsed_result.has_value());
      expect(parsed_result->name == original.name);
      expect(parsed_result->age == original.age);
   };

   "prettify_json"_test = [] {
      std::string compact = R"({"name":"Bob","age":35})";
      auto result = glz::prettify_json(compact);
      // Prettified JSON should contain newlines
      expect(result.find('\n') != std::string::npos);
   };

   "minify_json"_test = [] {
      std::string pretty = R"({
         "name": "Carol",
         "age": 28
      })";
      auto result = glz::minify_json(pretty);
      // Minified JSON should not contain newlines
      expect(result.find('\n') == std::string::npos);
   };
};

int main() { return 0; }
