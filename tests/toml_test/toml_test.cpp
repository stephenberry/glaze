#include "glaze/toml.hpp"
#include "ut/ut.hpp"
#include <map>

using namespace ut;

struct my_struct {
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

struct nested {
   int x = 10;
   std::string y = "test";
};

struct container {
   nested inner{};
   double value = 5.5;
};

struct optional_struct {
   std::optional<int> maybe = 99;
};

suite starter = [] {
   
   // Original test for a reflectable object.
   "example"_test = [] {
      my_struct s{};
      std::string buffer{};
      expect(not glz::write_toml(s, buffer));
      expect(buffer ==
R"(i = 287
d = 3.14
hello = "Hello World"
arr = [1, 2, 3])");
   };
   
   // Test writing a simple scalar value.
   "scalar_int"_test = [] {
      int i = 42;
      std::string buffer{};
      expect(not glz::write_toml(i, buffer));
      expect(buffer == "42");
   };
   
   "simple_array"_test = [] {
      std::vector<int> v = {1, 2, 3, 4};
      std::string buffer{};
      expect(not glz::write_toml(v, buffer));
      expect(buffer == "[1, 2, 3, 4]");
   };
   
   "writable_map"_test = [] {
      std::map<std::string, int> m = { {"a", 1}, {"b", 2} };
      std::string buffer{};
      expect(not glz::write_toml(m, buffer));
      // std::map orders keys lexicographically, so we expect:
      expect(buffer == "a = 1\nb = 2");
   };
   
   "tuple_test"_test = [] {
      std::tuple<int, std::string> t = {100, "hello"};
      std::string buffer{};
      expect(not glz::write_toml(t, buffer));
      expect(buffer == R"([100, "hello"])");
   };
   
   // Test writing a string that contains quotes and backslashes.
   "escape_string"_test = [] {
      std::string s = "Line \"quoted\" and \\ backslash";
      std::string buffer{};
      expect(not glz::write_toml(s, buffer));
      // The expected output escapes the quote and backslash, and encloses the result in quotes.
      expect(buffer == R"("Line \"quoted\" and \\ backslash")");
   };
   
   // Test writing a nested structure.
   "nested_struct"_test = [] {
      container c{};
      std::string buffer{};
      expect(not glz::write_toml(c, buffer));
      expect(buffer == R"([inner]
x = 10
y = "test"

value = 5.5)");
   };
   
   // Test writing a boolean value.
   "boolean_value"_test = [] {
      bool b = true;
      std::string buffer{};
      expect(not glz::write_toml(b, buffer));
      expect(buffer == "true");
   };
   
   // Test writing an empty array.
   "empty_array"_test = [] {
      std::vector<int> empty{};
      std::string buffer{};
      expect(not glz::write_toml(empty, buffer));
      expect(buffer == "[]");
   };
   
   // Test writing an empty map.
   "empty_map"_test = [] {
      std::map<std::string, int> empty{};
      std::string buffer{};
      expect(not glz::write_toml(empty, buffer));
      expect(buffer == "");
   };
   
   // Test writing a vector of booleans.
   "vector_of_bool"_test = [] {
      std::vector<bool> vb = { true, false, true };
      std::string buffer{};
      expect(not glz::write_toml(vb, buffer));
      expect(buffer == "[true, false, true]");
   };
   
   // Test writing an optional that contains a value.
   "optional_present"_test = [] {
      std::optional<int> opt = 42;
      std::string buffer{};
      expect(not glz::write_toml(opt, buffer));
      expect(buffer == "42");
   };
   
   // Test writing an optional that is null.
   "optional_null"_test = [] {
      std::optional<int> opt = std::nullopt;
      std::string buffer{};
      expect(not glz::write_toml(opt, buffer));
      // Assuming that a null optional is skipped and produces an empty output.
      expect(buffer == "");
   };
   
   // Test writing a structure with an optional member (present).
   "optional_struct_present"_test = [] {
      optional_struct os{};
      std::string buffer{};
      expect(not glz::write_toml(os, buffer));
      expect(buffer == "maybe = 99");
   };
   
   // Test writing a structure with an optional member (null).
   "optional_struct_null"_test = [] {
      optional_struct os{};
      os.maybe = std::nullopt;
      std::string buffer{};
      expect(not glz::write_toml(os, buffer));
      // If all members are null (or skipped) then the output is empty.
      expect(buffer == "");
   };
   
};

int main() { return 0; }
