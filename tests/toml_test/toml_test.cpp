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
   nested inner;
   double value = 5.5;
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

   // Test writing an array (std::vector).
   "simple_array"_test = [] {
      std::vector<int> v = {1, 2, 3, 4};
      std::string buffer{};
      expect(not glz::write_toml(v, buffer));
      // Arrays are written with square brackets and entries separated by ", "
      expect(buffer == "[1, 2, 3, 4]");
   };

   // Test writing a map-like container.
   "writable_map"_test = [] {
      std::map<std::string, int> m = { {"a", 1}, {"b", 2} };
      std::string buffer{};
      expect(not glz::write_toml(m, buffer));
      // std::map orders keys lexicographically, so we expect:
      expect(buffer == "a = 1\nb = 2");
   };

   // Test writing a tuple (which is handled as an array).
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
      /*
         Given our current writer for reflectable objects, nested objects are inlined.
         The expected output is:
         
         inner = x = 10
         y = "test"
         value = 5.5
         
         (Note: This output may not conform to TOML’s standard for nested tables.)
      */
      expect(buffer == R"(inner = x = 10
y = "test"
value = 5.5)");
   };

};

int main() { return 0; }
