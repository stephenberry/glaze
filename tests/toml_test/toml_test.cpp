#include "glaze/toml.hpp"

#include <map>
#include <string_view> // Added for std::string_view

#include "ut/ut.hpp"

using namespace ut;

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

struct nested
{
   int x = 10;
   std::string y = "test";
};

struct container
{
   nested inner{};
   double value = 5.5;
};

struct optional_struct
{
   std::optional<int> maybe = 99;
};

struct inline_table_member
{
   std::string key1{};
   int key2{};

   bool operator==(const inline_table_member&) const = default;
};

template <>
struct glz::meta<inline_table_member>
{
   using T = inline_table_member;
   static constexpr auto value = object("key1", &T::key1, "key2", &T::key2);
};

struct struct_with_inline_table
{
   std::string name{};
   inline_table_member inline_data{};

   bool operator==(const struct_with_inline_table&) const = default;
};

template <>
struct glz::meta<struct_with_inline_table>
{
   using T = struct_with_inline_table;
   static constexpr auto value = object("name", &T::name, "inline_data", &T::inline_data);
};

struct complex_strings_struct
{
   std::string basic_multiline{};
   std::string literal_multiline{};
   std::string literal_multiline_with_quotes{};

   bool operator==(const complex_strings_struct&) const = default;
};

template <>
struct glz::meta<complex_strings_struct>
{
   using T = complex_strings_struct;
   static constexpr auto value =
      object("basic_multiline", &T::basic_multiline, "literal_multiline", &T::literal_multiline,
             "literal_multiline_with_quotes", &T::literal_multiline_with_quotes);
};

struct comment_test_struct
{
   int a{};
   std::string b{};

   bool operator==(const comment_test_struct&) const = default;
};

template <>
struct glz::meta<comment_test_struct>
{
   using T = comment_test_struct;
   static constexpr auto value = object("a", &T::a, "b", &T::b);
};

struct non_null_term_struct
{
   int value{};
   bool operator==(const non_null_term_struct&) const = default;
};

template <>
struct glz::meta<non_null_term_struct>
{
   using T = non_null_term_struct;
   static constexpr auto value = object("value", &T::value);
};

suite starter = [] {
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

   "read_basic_struct"_test = [] {
      std::string toml_input = R"(i = 42
d = 2.71
hello = "Test String"
arr = [4, 5, 6])";

      my_struct s{};
      expect(not glz::read_toml(s, toml_input));
      expect(s.i == 42);
      expect(s.d == 2.71);
      expect(s.hello == "Test String");
      expect(s.arr[0] == 4);
      expect(s.arr[1] == 5);
      expect(s.arr[2] == 6);
   };

   "read_integer"_test = [] {
      std::string toml_input = "123";
      int value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == 123);
   };

   "read_float"_test = [] {
      std::string toml_input = "3.14159";
      double value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == 3.14159);
   };

   "read_string"_test = [] {
      std::string toml_input = R"("Hello TOML")";
      std::string value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == "Hello TOML");
   };

   "read_boolean_true"_test = [] {
      std::string toml_input = "true";
      bool value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == true);
   };

   "read_boolean_false"_test = [] {
      std::string toml_input = "false";
      bool value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == false);
   };

   "read_array"_test = [] {
      std::string toml_input = "[1, 2, 3, 4]";
      std::vector<int> value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value.size() == 4);
      expect(value[0] == 1);
      expect(value[1] == 2);
      expect(value[2] == 3);
      expect(value[3] == 4);
   };

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
      std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
      std::string buffer{};
      expect(not glz::write_toml(m, buffer));
      // std::map orders keys lexicographically, so we expect:
      expect(buffer == R"(a = 1
b = 2)");
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
      std::vector<bool> vb = {true, false, true};
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

   "read_inline_table"_test = [] {
      std::string toml_input = R"(name = "Test Person"
inline_data = { key1 = "value1", key2 = 100 })";
      struct_with_inline_table s{};
      auto error = glz::read_toml(s, toml_input);
      expect(!error) << glz::format_error(error, toml_input);
      expect(s.name == "Test Person");
      expect(s.inline_data.key1 == "value1");
      expect(s.inline_data.key2 == 100);
   };

   "read_complex_strings"_test = [] {
      std::string toml_input = R"(
basic_multiline = """
Roses are red
Violets are blue"""
literal_multiline = '''
The first line.
  The second line.
    The third line.'''
literal_multiline_with_quotes = '''He said "She said 'It is so.''"'''
)";
      complex_strings_struct s{};
      auto error = glz::read_toml(s, toml_input);
      expect(!error) << glz::format_error(error, toml_input);
      expect(s.basic_multiline == "Roses are red\nViolets are blue");
      expect(s.literal_multiline == "The first line.\n  The second line.\n    The third line.");
      expect(s.literal_multiline_with_quotes == "He said \"She said 'It is so.''\"");
   };

   "read_with_comments"_test = [] {
      std::string toml_input = R"(
# This is a full line comment
a = 123 # This is an end-of-line comment
# Another comment
b = "test string" # another eol comment
)";
      comment_test_struct s{};
      auto error = glz::read_toml(s, toml_input);
      expect(!error) << glz::format_error(error, toml_input);
      expect(s.a == 123);
      expect(s.b == "test string");
   };

   "read_non_null_terminated"_test = [] {
      std::string buffer_with_extra = "value = 123GARBAGE_DATA";
      // Create a string_view that does not include "GARBAGE_DATA" and is not null-terminated
      // within the view itself.
      std::string_view toml_data(buffer_with_extra.data(), 11); // "value = 123"

      non_null_term_struct s_val{};
      auto error = glz::read_toml(s_val, toml_data);
      expect(!error) << glz::format_error(error, toml_data);
      expect(s_val.value == 123);

      std::string buffer_just_value = "value = 456"; // Null terminated by string constructor
      non_null_term_struct s_val2{};
      error = glz::read_toml(s_val2, buffer_just_value);
      expect(!error) << glz::format_error(error, buffer_just_value);
      expect(s_val2.value == 456);
   };
};

int main() { return 0; }
