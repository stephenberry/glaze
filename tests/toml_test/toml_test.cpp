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

struct simple_container
{
   nested inner{};
   double value = 5.5;
};

struct advanced_container
{
   nested inner{};
   nested inner_two{};
   double value = 5.5;
};

struct level_one
{
   int value{0};
};

struct level_two
{
   level_one l1{};
};

struct dotted_access_struct
{
   level_two l2{};
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

   "read_wrong_overflow_integer"_test = [] {
      // Max uint64 value plus one.
      std::string toml_input = "18446744073709551616";
      uint64_t value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_nearly_overfow_integer"_test = [] {
      // Max uint64 value.
      std::string toml_input = "18446744073709551615";
      uint64_t value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == 18446744073709551615ull);
   };

   "read_wrong_underflow_integer"_test = [] {
      // Min int64 value minus one.
      std::string toml_input = "-9223372036854775809";
      int64_t value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_nearly_underflow_integer"_test = [] {
      // Min int64 value.
      std::string toml_input = "-9223372036854775808";
      int64_t value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == -9223372036854775807ll - 1);
   };

   "read_negative_integer"_test = [] {
      std::string toml_input = "-123";
      int value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == -123);
   };

   "read_wrong_negative_integer"_test = [] {
      std::string toml_input = "-123";
      // Negative values should not succeed for unsigned types.
      unsigned int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_positive_integer"_test = [] {
      std::string toml_input = "+123";
      int value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == 123);
   };

   "read_hex_integer"_test = [] {
      std::string toml_input = "0x012abCD";
      int value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == 0x012abCD);
   };

   "read_wrong_hex_integer"_test = [] {
      std::string toml_input = "0xG";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_hex_negative_integer"_test = [] {
      std::string toml_input = "-0x12abCD";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_hex_positive_integer"_test = [] {
      std::string toml_input = "+0x12abCD";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_binary_integer"_test = [] {
      std::string toml_input = "0b010";
      int value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == 0b010);
   };

   "read_wrong_binary_integer"_test = [] {
      std::string toml_input = "0b3";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_binary_negative_integer"_test = [] {
      std::string toml_input = "-0b10";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_binary_positive_integer"_test = [] {
      std::string toml_input = "+0b10";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_octal_integer"_test = [] {
      std::string toml_input = "0o01267";
      int value{};
      expect(not glz::read_toml(value, toml_input));
      // Leading '0' to specify octal in C++.
      expect(value == 001267);
   };

   "read_wrong_octal_integer"_test = [] {
      std::string toml_input = "0o8";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_octal_negative_integer"_test = [] {
      std::string toml_input = "-0o1267";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_octal_positive_integer"_test = [] {
      std::string toml_input = "+0o1267";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_underscore_integer"_test = [] {
      std::string toml_input = "1_2_3";
      int value{};
      expect(not glz::read_toml(value, toml_input));
      expect(value == 123);
   };

   "read_wrong_underscore_integer"_test = [] {
      std::string toml_input = "1__2_3";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_underscore_integer"_test = [] {
      std::string toml_input = "_123";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_underscore_negative_integer"_test = [] {
      std::string toml_input = "-_123";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_underscore_positive_integer"_test = [] {
      std::string toml_input = "+_123";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_underscore_hex_integer"_test = [] {
      std::string toml_input = "0x_12abCD";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_underscore_binary_integer"_test = [] {
      std::string toml_input = "0b_10";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_underscore_octal_integer"_test = [] {
      std::string toml_input = "0o_1267";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_trailing_underscore_integer"_test = [] {
      std::string toml_input = "123_";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_zero_integer"_test = [] {
      std::string toml_input = "0123";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_zero_negative_integer"_test = [] {
      std::string toml_input = "-0123";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   "read_wrong_leading_zero_positive_integer"_test = [] {
      std::string toml_input = "+0123";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
   };

   // In TOML, integers are not able to have an exponent component.
   "read_wrong_exponent_integer"_test = [] {
      std::string toml_input = "1e2";
      int value{};
      auto error = glz::read_toml(value, toml_input);
      expect(error);
      expect(error == glz::error_code::parse_number_failure);
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

   "write_explicit_string_view"_test = [] {
      struct explicit_string_view_type
      {
         std::string storage{};

         explicit explicit_string_view_type(std::string_view s) : storage(s) {}

         explicit operator std::string_view() const noexcept { return storage; }
      };

      explicit_string_view_type value{std::string_view{"explicit"}};

      std::string buffer{};
      expect(not glz::write_toml(value, buffer));
      expect(buffer == R"("explicit")");

      buffer.clear();
      expect(not glz::write<glz::opts{.format = glz::TOML, .raw_string = true}>(value, buffer));
      expect(buffer == R"("explicit")");
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
   "write_nested_struct"_test = [] {
      simple_container c{};
      std::string buffer{};
      expect(not glz::write_toml(c, buffer));
      expect(buffer == R"([inner]
x = 10
y = "test"

value = 5.5)"); // TODO: This is not the right format, we need to refactor the output to match TOML syntax.
                // For now, I'll leave it as is, but it should be fixed in the future.
   };

   "read_wrong_format_nested"_test = [] {
      advanced_container sc{};
      std::string buffer{R"([inner]
x = 10
y = "test"

value = 5.5)"};
      auto error = glz::read_toml(sc, buffer);
      expect(error); // Expect an error because the format is not correct for TOML. root value should be before nested
                     // table.
      expect(error == glz::error_code::syntax_error);
   };

   "read_nested_struct"_test = [] {
      simple_container sc{};
      std::string buffer{R"(value = 5.6

[inner]
x = 11
y = "test1"
)"};
      expect(not glz::read_toml(sc, buffer));
      expect(sc.inner.x == 11);
      expect(sc.inner.y == "test1");
      expect(sc.value == 5.6);
   };

   "read_advanced_nested_struct"_test = [] {
      advanced_container ac{};
      std::string buffer{R"(value = 5.6

[inner]
x = 11
y = "test1"

[inner_two]
x = 12
y = "test2"
)"};
      expect(not glz::read_toml(ac, buffer));
      expect(ac.inner.x == 11);
      expect(ac.inner.y == "test1");
      expect(ac.inner_two.x == 12);
      expect(ac.inner_two.y == "test2");
      expect(ac.value == 5.6);
   };

   "read_advanced_nested_struct"_test = [] {
      dotted_access_struct dac{};
      std::string buffer{R"(l2.l1.value = 1)"};
      expect(not glz::read_toml(dac, buffer));
      expect(dac.l2.l1.value == 1);
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
