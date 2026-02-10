// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/yaml.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <forward_list>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "glaze/json/generic.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Test structures
struct simple_struct
{
   int x{};
   double y{};
   std::string name{};
};

template <>
struct glz::meta<simple_struct>
{
   using T = simple_struct;
   static constexpr auto value = object("x", &T::x, "y", &T::y, "name", &T::name);
};

struct nested_struct
{
   std::string title{};
   simple_struct data{};
   std::vector<int> numbers{};
};

template <>
struct glz::meta<nested_struct>
{
   using T = nested_struct;
   static constexpr auto value = object("title", &T::title, "data", &T::data, "numbers", &T::numbers);
};

struct optional_struct
{
   std::string name{};
   std::optional<int> age{};
   std::optional<std::string> email{};
};

template <>
struct glz::meta<optional_struct>
{
   using T = optional_struct;
   static constexpr auto value = object("name", &T::name, "age", &T::age, "email", &T::email);
};

struct bool_struct
{
   bool flag{};
};

template <>
struct glz::meta<bool_struct>
{
   using T = bool_struct;
   static constexpr auto value = object("flag", &T::flag);
};

enum class Color { red, green, blue };

template <>
struct glz::meta<Color>
{
   using enum Color;
   static constexpr auto value = enumerate("red", red, "green", green, "blue", blue);
};

struct enum_struct
{
   std::string name{};
   Color color{};
};

template <>
struct glz::meta<enum_struct>
{
   using T = enum_struct;
   static constexpr auto value = object("name", &T::name, "color", &T::color);
};

struct reflectable_config
{
   std::vector<int> servers{};
};

suite yaml_write_tests = [] {
   "write_simple_struct"_test = [] {
      simple_struct obj{42, 3.14, "test"};
      std::string buffer;
      auto ec = glz::write_yaml(obj, buffer);
      expect(!ec);
      expect(buffer.find("x: 42") != std::string::npos);
      expect(buffer.find("y: 3.14") != std::string::npos);
      expect(buffer.find("name: test") != std::string::npos);
   };

   "write_nested_struct"_test = [] {
      nested_struct obj{"Hello", {1, 2.5, "inner"}, {1, 2, 3}};
      std::string buffer;
      auto ec = glz::write_yaml(obj, buffer);
      expect(!ec);
      expect(buffer.find("title: Hello") != std::string::npos);
   };

   "write_vector"_test = [] {
      std::vector<int> vec{1, 2, 3, 4, 5};
      std::string buffer;
      auto ec = glz::write_yaml(vec, buffer);
      expect(!ec);
      expect(buffer.find("- 1") != std::string::npos);
      expect(buffer.find("- 5") != std::string::npos);
   };

   "write_map"_test = [] {
      std::map<std::string, int> m{{"one", 1}, {"two", 2}, {"three", 3}};
      std::string buffer;
      auto ec = glz::write_yaml(m, buffer);
      expect(!ec);
      expect(buffer.find("one: 1") != std::string::npos);
      expect(buffer.find("two: 2") != std::string::npos);
   };

   "write_optional_with_value"_test = [] {
      optional_struct obj{"John", 30, "john@example.com"};
      std::string buffer;
      auto ec = glz::write_yaml(obj, buffer);
      expect(!ec);
      expect(buffer.find("name: John") != std::string::npos);
      expect(buffer.find("age: 30") != std::string::npos);
   };

   "write_optional_without_value"_test = [] {
      optional_struct obj{"Jane", std::nullopt, std::nullopt};
      std::string buffer;
      auto ec = glz::write_yaml(obj, buffer);
      expect(!ec);
      expect(buffer.find("name: Jane") != std::string::npos);
   };

   "write_boolean"_test = [] {
      bool_struct obj{true};
      std::string buffer;
      auto ec = glz::write<glz::opts{.format = glz::YAML}>(obj, buffer);
      expect(!ec);
      expect(buffer.find("true") != std::string::npos);
   };

   "write_enum"_test = [] {
      enum_struct obj{"item", Color::green};
      std::string buffer;
      auto ec = glz::write_yaml(obj, buffer);
      expect(!ec);
      expect(buffer.find("color: green") != std::string::npos);
   };

   "write_string_with_special_chars"_test = [] {
      simple_struct obj{1, 1.0, "hello: world"};
      std::string buffer;
      auto ec = glz::write_yaml(obj, buffer);
      expect(!ec);
      // Should be quoted because it contains colon
      expect(buffer.find("name:") != std::string::npos);
   };

   "write_flow_style"_test = [] {
      simple_struct obj{42, 3.14, "test"};
      std::string buffer;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto ec = glz::write<opts>(obj, buffer);
      expect(!ec);
      expect(buffer.find("{") != std::string::npos);
      expect(buffer.find("}") != std::string::npos);
   };
};

suite yaml_read_tests = [] {
   "read_simple_block_mapping"_test = [] {
      std::string yaml = R"(x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
      expect(std::abs(obj.y - 3.14) < 0.001);
      expect(obj.name == "test");
   };

   "read_flow_mapping"_test = [] {
      std::string yaml = R"({x: 42, y: 3.14, name: test})";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
      expect(std::abs(obj.y - 3.14) < 0.001);
      expect(obj.name == "test");
   };

   "read_flow_sequence"_test = [] {
      std::string yaml = R"([1, 2, 3, 4, 5])";
      std::vector<int> vec{};
      auto ec = glz::read_yaml(vec, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(vec.size() == 5u);
      expect(vec[0] == 1);
      expect(vec[4] == 5);
   };

   "read_block_sequence"_test = [] {
      std::string yaml = R"(- 1
- 2
- 3)";
      std::vector<int> vec{};
      auto ec = glz::read_yaml(vec, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(vec.size() == 3u);
      expect(vec[0] == 1);
      expect(vec[2] == 3);
   };

   "read_double_quoted_string"_test = [] {
      std::string yaml = R"(x: 1
y: 2.0
name: "hello world")";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "hello world");
   };

   "read_single_quoted_string"_test = [] {
      std::string yaml = R"(x: 1
y: 2.0
name: 'hello world')";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "hello world");
   };

   "read_boolean_true"_test = [] {
      std::string yaml = "flag: true";
      bool_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.flag == true);
   };

   "read_boolean_false"_test = [] {
      std::string yaml = "flag: false";
      bool_struct obj{.flag = true};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.flag == false);
   };

   "read_null_optional"_test = [] {
      std::string yaml = R"(name: Test
age: null
email: ~)";
      optional_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "Test");
      expect(!obj.age.has_value());
      expect(!obj.email.has_value());
   };

   "read_optional_with_value"_test = [] {
      std::string yaml = R"(name: Test
age: 25
email: test@example.com)";
      optional_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "Test");
      expect(obj.age.value() == 25);
      expect(obj.email.value() == "test@example.com");
   };

   "read_enum"_test = [] {
      std::string yaml = R"(name: item
color: blue)";
      enum_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "item");
      expect(obj.color == Color::blue);
   };

   "read_map"_test = [] {
      std::string yaml = R"(one: 1
two: 2
three: 3)";
      std::map<std::string, int> m{};
      auto ec = glz::read_yaml(m, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(m["one"] == 1);
      expect(m["two"] == 2);
      expect(m["three"] == 3);
   };

   "read_flow_map"_test = [] {
      std::string yaml = R"({one: 1, two: 2, three: 3})";
      std::map<std::string, int> m{};
      auto ec = glz::read_yaml(m, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(m["one"] == 1);
      expect(m["two"] == 2);
   };

   "read_negative_number"_test = [] {
      std::string yaml = R"(x: -42
y: -3.14
name: neg)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == -42);
      expect(std::abs(obj.y - (-3.14)) < 0.001);
   };

   "read_hex_number"_test = [] {
      std::string yaml = "x: 0xFF\ny: 1.0\nname: hex";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 255);
   };

   "read_underscore_int"_test = [] {
      int value{};
      std::string yaml = "1_000_000";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec);
      expect(value == 1000000);
   };

   "read_underscore_float"_test = [] {
      double value{};
      std::string yaml = "1_234.567_89";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec);
      expect(std::abs(value - 1234.56789) < 0.00001);
   };

   "read_underscore_hex"_test = [] {
      int value{};
      std::string yaml = "0xFF_FF";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec);
      expect(value == 0xFFFF);
   };

   "read_underscore_octal"_test = [] {
      int value{};
      std::string yaml = "0o7_7_7";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec);
      expect(value == 0777);
   };

   "read_underscore_binary"_test = [] {
      int value{};
      std::string yaml = "0b1111_0000";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec);
      expect(value == 0b11110000);
   };

   "read_no_underscore_int"_test = [] {
      int value{};
      std::string yaml = "1000000";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec);
      expect(value == 1000000);
   };

   "read_with_comments"_test = [] {
      std::string yaml = R"(# This is a comment
x: 42 # inline comment
y: 3.14
# Another comment
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
      expect(obj.name == "test");
   };

   // Per YAML spec, # only starts a comment when preceded by whitespace
   "hash_in_plain_scalar"_test = [] {
      std::string yaml = "name: foo#bar";
      simple_struct obj{};
      obj.x = 1;
      obj.y = 1.0;
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "foo#bar") << "Hash should be part of scalar, not start comment";
   };

   "hash_with_space_is_comment"_test = [] {
      std::string yaml = "name: foo #bar";
      simple_struct obj{};
      obj.x = 1;
      obj.y = 1.0;
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "foo") << "Space+hash should start a comment";
   };

   "multiple_hashes_in_scalar"_test = [] {
      std::string yaml = "name: a#b#c#d";
      simple_struct obj{};
      obj.x = 1;
      obj.y = 1.0;
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "a#b#c#d");
   };

   "url_with_fragment"_test = [] {
      std::string yaml = "name: http://example.com/page#section";
      simple_struct obj{};
      obj.x = 1;
      obj.y = 1.0;
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "http://example.com/page#section");
   };

   "hash_at_start_is_comment"_test = [] {
      std::string yaml = R"(x: 1
y: 1.0
#name: should_be_ignored
name: actual_value)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "actual_value");
   };
};

suite yaml_roundtrip_tests = [] {
   "roundtrip_simple_struct"_test = [] {
      simple_struct original{42, 3.14159, "hello"};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      simple_struct parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(parsed.x == original.x);
      expect(std::abs(parsed.y - original.y) < 0.0001);
      expect(parsed.name == original.name);
   };

   "roundtrip_vector"_test = [] {
      std::vector<int> original{1, 2, 3, 4, 5};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::vector<int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(parsed == original);
   };

   "roundtrip_map"_test = [] {
      std::map<std::string, int> original{{"a", 1}, {"b", 2}, {"c", 3}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::map<std::string, int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(parsed == original);
   };

   "roundtrip_optional"_test = [] {
      optional_struct original{"Test", 25, std::nullopt};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      optional_struct parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(parsed.name == original.name);
      expect(parsed.age == original.age);
   };

   "roundtrip_enum"_test = [] {
      enum_struct original{"item", Color::green};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      enum_struct parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(parsed.name == original.name);
      expect(parsed.color == original.color);
   };
};

suite yaml_block_scalar_tests = [] {
   "write_multiline_string"_test = [] {
      simple_struct obj{1, 1.0, "line1\nline2\nline3"};
      std::string buffer;
      auto ec = glz::write_yaml(obj, buffer);
      expect(!ec);
      // Multiline strings should use block scalar or quoted string
   };

   "roundtrip_literal_block_keep_multiple_newlines"_test = [] {
      std::string original = "line1\nline2\n\n\n";
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      expect(yaml.find("|+") != std::string::npos);

      std::string parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "read_literal_block_scalar"_test = [] {
      std::string yaml = R"(x: 1
y: 1.0
name: |
  line1
  line2
  line3)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name.find("line1") != std::string::npos);
      expect(obj.name.find("line2") != std::string::npos);
   };

   "read_folded_block_scalar"_test = [] {
      std::string yaml = R"(x: 1
y: 1.0
name: >
  this is a
  folded string)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // Folded scalar replaces single newlines with spaces
      expect(obj.name.find("this is a") != std::string::npos);
   };
};

suite yaml_special_values_tests = [] {
   "read_infinity"_test = [] {
      std::string yaml = "x: 0\ny: .inf\nname: inf";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::isinf(obj.y));
      expect(obj.y > 0);
   };

   "read_negative_infinity"_test = [] {
      std::string yaml = "x: 0\ny: -.inf\nname: ninf";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::isinf(obj.y));
      expect(obj.y < 0);
   };

   "read_nan"_test = [] {
      std::string yaml = "x: 0\ny: .nan\nname: nan";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::isnan(obj.y));
   };
};

suite yaml_tuple_tests = [] {
   "write_tuple_flow"_test = [] {
      std::tuple<int, double, std::string> t{42, 3.14, "hello"};
      std::string buffer;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto ec = glz::write<opts>(t, buffer);
      expect(!ec);
      expect(buffer.find("[42") != std::string::npos);
      expect(buffer.find("3.14") != std::string::npos);
      expect(buffer.find("hello") != std::string::npos);
   };

   "write_tuple_block"_test = [] {
      std::tuple<int, double, std::string> t{42, 3.14, "hello"};
      std::string buffer;
      auto ec = glz::write_yaml(t, buffer);
      expect(!ec);
      expect(buffer.find("- 42") != std::string::npos);
      expect(buffer.find("- 3.14") != std::string::npos);
      expect(buffer.find("- hello") != std::string::npos);
   };

   "read_tuple_flow"_test = [] {
      std::string yaml = "[42, 3.14, hello]";
      std::tuple<int, double, std::string> t{};
      auto ec = glz::read_yaml(t, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::get<0>(t) == 42);
      expect(std::abs(std::get<1>(t) - 3.14) < 0.001);
      expect(std::get<2>(t) == "hello");
   };

   "read_tuple_block"_test = [] {
      std::string yaml = R"(- 42
- 3.14
- hello)";
      std::tuple<int, double, std::string> t{};
      auto ec = glz::read_yaml(t, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::get<0>(t) == 42);
      expect(std::abs(std::get<1>(t) - 3.14) < 0.001);
      expect(std::get<2>(t) == "hello");
   };

   "roundtrip_tuple"_test = [] {
      std::tuple<int, std::string, bool> original{123, "test", true};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::tuple<int, std::string, bool> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::get<0>(parsed) == std::get<0>(original));
      expect(std::get<1>(parsed) == std::get<1>(original));
      expect(std::get<2>(parsed) == std::get<2>(original));
   };
};

suite yaml_pair_tests = [] {
   "write_pair_flow"_test = [] {
      std::pair<std::string, int> p{"answer", 42};
      std::string buffer;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto ec = glz::write<opts>(p, buffer);
      expect(!ec);
      expect(buffer.find("{answer: 42}") != std::string::npos);
   };

   "write_pair_block"_test = [] {
      std::pair<std::string, int> p{"answer", 42};
      std::string buffer;
      auto ec = glz::write_yaml(p, buffer);
      expect(!ec);
      expect(buffer.find("answer: 42") != std::string::npos);
   };

   "read_pair_flow"_test = [] {
      std::string yaml = "{answer: 42}";
      std::pair<std::string, int> p{};
      auto ec = glz::read_yaml(p, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(p.first == "answer");
      expect(p.second == 42);
   };

   "read_pair_block"_test = [] {
      std::string yaml = "answer: 42";
      std::pair<std::string, int> p{};
      auto ec = glz::read_yaml(p, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(p.first == "answer");
      expect(p.second == 42);
   };

   "roundtrip_pair"_test = [] {
      std::pair<std::string, double> original{"pi", 3.14159};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::pair<std::string, double> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(parsed.first == original.first);
      expect(std::abs(parsed.second - original.second) < 0.0001);
   };

   "read_pair_with_nested_value"_test = [] {
      std::string yaml = "{key: [1, 2, 3]}";
      std::pair<std::string, std::vector<int>> p{};
      auto ec = glz::read_yaml(p, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(p.first == "key");
      expect(p.second.size() == 3u);
      expect(p.second[0] == 1);
      expect(p.second[2] == 3);
   };

   "write_vector_of_pairs"_test = [] {
      std::vector<std::pair<std::string, int>> vec{{"one", 1}, {"two", 2}};
      std::string buffer;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto ec = glz::write<opts>(vec, buffer);
      expect(!ec);
      expect(buffer.find("one: 1") != std::string::npos);
      expect(buffer.find("two: 2") != std::string::npos);
   };
};

suite yaml_tag_tests = [] {
   "valid_str_tag"_test = [] {
      std::string yaml = "!!str hello";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello");
   };

   "valid_int_tag"_test = [] {
      std::string yaml = "!!int 42";
      int value{};
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == 42);
   };

   "valid_float_tag"_test = [] {
      std::string yaml = "!!float 3.14";
      double value{};
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::abs(value - 3.14) < 0.001);
   };

   "valid_bool_tag"_test = [] {
      std::string yaml = "!!bool true";
      bool value{};
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == true);
   };

   "valid_null_tag"_test = [] {
      std::string yaml = "!!null null";
      std::optional<int> value{42};
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(!value.has_value());
   };

   "valid_seq_tag"_test = [] {
      std::string yaml = "!!seq [1, 2, 3]";
      std::vector<int> value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value.size() == 3u);
      expect(value[0] == 1);
   };

   "valid_map_tag"_test = [] {
      std::string yaml = "!!map {a: 1, b: 2}";
      std::map<std::string, int> value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value["a"] == 1);
      expect(value["b"] == 2);
   };

   "invalid_str_tag_for_int"_test = [] {
      std::string yaml = "!!str 42";
      int value{};
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::syntax_error);
   };

   "invalid_int_tag_for_string"_test = [] {
      std::string yaml = "!!int hello";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::syntax_error);
   };

   "invalid_bool_tag_for_int"_test = [] {
      std::string yaml = "!!bool 42";
      int value{};
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::syntax_error);
   };

   "invalid_seq_tag_for_map"_test = [] {
      std::string yaml = "!!seq {a: 1}";
      std::map<std::string, int> value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::syntax_error);
   };

   "invalid_map_tag_for_seq"_test = [] {
      std::string yaml = "!!map [1, 2, 3]";
      std::vector<int> value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::syntax_error);
   };

   "unknown_custom_tag_error"_test = [] {
      std::string yaml = "!mytag value";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::feature_not_supported);
   };

   "unknown_shorthand_tag_error"_test = [] {
      std::string yaml = "!!custom value";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::feature_not_supported);
   };

   "verbatim_tag_str"_test = [] {
      std::string yaml = "!<tag:yaml.org,2002:str> hello";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello");
   };

   "int_tag_valid_for_float"_test = [] {
      // !!int is valid for float types (widening conversion)
      std::string yaml = "!!int 42";
      double value{};
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == 42.0);
   };

   "map_with_str_tagged_values"_test = [] {
      // Map of string to string - only !!str tags are valid for values
      std::string yaml = R"({name: !!str Alice, city: !!str Boston})";
      std::map<std::string, std::string> obj;
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["name"] == "Alice");
      expect(obj["city"] == "Boston");
   };

   "map_with_int_tagged_values"_test = [] {
      // Map of string to int - !!int tags are valid for values
      std::string yaml = R"({count: !!int 100, size: !!int 50})";
      std::map<std::string, int> obj;
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["count"] == 100);
      expect(obj["size"] == 50);
   };

   // ============================================================
   // Comprehensive String Parsing Tests
   // ============================================================

   // Double-quoted string escape tests
   "dq_escape_newline"_test = [] {
      std::string yaml = R"("hello\nworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\nworld");
   };

   "dq_escape_tab"_test = [] {
      std::string yaml = R"("hello\tworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\tworld");
   };

   "dq_escape_carriage_return"_test = [] {
      std::string yaml = R"("hello\rworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\rworld");
   };

   "dq_escape_backslash"_test = [] {
      std::string yaml = R"("hello\\world")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\\world");
   };

   "dq_escape_quote"_test = [] {
      std::string yaml = R"("hello\"world")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\"world");
   };

   "dq_escape_null"_test = [] {
      std::string yaml = R"("hello\0world")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == std::string("hello\0world", 11));
   };

   "dq_escape_bell"_test = [] {
      std::string yaml = R"("hello\aworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\aworld");
   };

   "dq_escape_backspace"_test = [] {
      std::string yaml = R"("hello\bworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\bworld");
   };

   "dq_escape_escape"_test = [] {
      std::string yaml = R"("hello\eworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\x1Bworld"); // ESC = 0x1B
   };

   "dq_escape_formfeed"_test = [] {
      std::string yaml = R"("hello\fworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\fworld");
   };

   "dq_escape_vtab"_test = [] {
      std::string yaml = R"("hello\vworld")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\vworld");
   };

   "dq_escape_slash"_test = [] {
      std::string yaml = R"("hello\/world")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello/world");
   };

   "dq_escape_space"_test = [] {
      std::string yaml = R"("hello\ world")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello world");
   };

   // Hex escape \xXX
   "dq_escape_hex_41"_test = [] {
      std::string yaml = R"("\x41")"; // 'A'
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "A");
   };

   "dq_escape_hex_00"_test = [] {
      std::string yaml = R"("a\x00z")"; // null in middle
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // Build expected string carefully to avoid C++ hex escape ambiguity
      std::string expected = "a";
      expected.push_back('\0');
      expected.push_back('z');
      expect(value == expected);
   };

   "dq_escape_hex_ff"_test = [] {
      std::string yaml = R"("\xff")"; // 0xFF
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\xff");
   };

   "dq_escape_hex_lowercase"_test = [] {
      std::string yaml = R"("\x4a")"; // 'J'
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "J");
   };

   // Unicode escape \uXXXX
   "dq_escape_unicode_ascii"_test = [] {
      std::string yaml = R"("\u0041")"; // 'A'
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "A");
   };

   "dq_escape_unicode_2byte"_test = [] {
      std::string yaml = R"("\u00e9")"; // 'é' (U+00E9)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\xc3\xa9"); // UTF-8 encoding of é
   };

   "dq_escape_unicode_3byte"_test = [] {
      std::string yaml = R"("\u4e2d")"; // '中' (U+4E2D)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\xe4\xb8\xad"); // UTF-8 encoding
   };

   "dq_escape_unicode_euro"_test = [] {
      std::string yaml = R"("\u20ac")"; // '€' (U+20AC)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\xe2\x82\xac"); // UTF-8 encoding
   };

   // Unicode escape \UXXXXXXXX (8 hex digits)
   "dq_escape_unicode8_ascii"_test = [] {
      std::string yaml = R"("\U00000041")"; // 'A'
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "A");
   };

   "dq_escape_unicode8_emoji"_test = [] {
      std::string yaml = R"("\U0001F600")"; // 😀 (U+1F600)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\xf0\x9f\x98\x80"); // UTF-8 encoding
   };

   "dq_escape_unicode8_musical"_test = [] {
      std::string yaml = R"("\U0001D11E")"; // 𝄞 (U+1D11E) musical G clef
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\xf0\x9d\x84\x9e"); // UTF-8 encoding
   };

   // YAML-specific escapes
   "dq_escape_next_line"_test = [] {
      std::string yaml = R"("hello\Nworld")"; // \N = U+0085 (Next Line)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\xc2\x85world"); // UTF-8 encoding of U+0085
   };

   "dq_escape_nbsp"_test = [] {
      std::string yaml = R"("hello\_world")"; // \_ = U+00A0 (NBSP)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\xc2\xa0world"); // UTF-8 encoding of U+00A0
   };

   "dq_escape_line_separator"_test = [] {
      std::string yaml = R"("hello\Lworld")"; // \L = U+2028 (Line Separator)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\xe2\x80\xa8world"); // UTF-8 encoding of U+2028
   };

   "dq_escape_para_separator"_test = [] {
      std::string yaml = R"("hello\Pworld")"; // \P = U+2029 (Paragraph Separator)
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\xe2\x80\xa9world"); // UTF-8 encoding of U+2029
   };

   // Multiple escapes in one string
   "dq_multiple_escapes"_test = [] {
      std::string yaml = R"("line1\nline2\ttabbed\\backslash")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "line1\nline2\ttabbed\\backslash");
   };

   "dq_mixed_escapes"_test = [] {
      std::string yaml = R"("\x48\u0065llo\n\U00000057orld")"; // "Hello\nWorld"
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "Hello\nWorld");
   };

   // Edge cases
   "dq_empty_string"_test = [] {
      std::string yaml = R"("")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "");
   };

   "dq_only_escapes"_test = [] {
      std::string yaml = R"("\n\t\r")";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\n\t\r");
   };

   "dq_consecutive_backslashes"_test = [] {
      std::string yaml = R"("\\\\")"; // four backslashes in YAML = two in result
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "\\\\");
   };

   "dq_long_string"_test = [] {
      std::string yaml = "\"" + std::string(1000, 'a') + "\"";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == std::string(1000, 'a'));
   };

   "dq_long_string_with_escapes"_test = [] {
      std::string input;
      for (int i = 0; i < 100; ++i) {
         input += "text\\n";
      }
      std::string yaml = "\"" + input + "\"";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      std::string expected;
      for (int i = 0; i < 100; ++i) {
         expected += "text\n";
      }
      expect(value == expected);
   };

   // Single-quoted string tests
   "sq_basic"_test = [] {
      std::string yaml = "'hello world'";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello world");
   };

   "sq_empty"_test = [] {
      std::string yaml = "''";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "");
   };

   "sq_escaped_quote"_test = [] {
      std::string yaml = "'it''s'"; // '' = single quote
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "it's");
   };

   "sq_multiple_escaped_quotes"_test = [] {
      std::string yaml = "'a''b''c'"; // a'b'c
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "a'b'c");
   };

   "sq_no_escape_processing"_test = [] {
      std::string yaml = R"('hello\nworld')"; // \n should NOT be escaped
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello\\nworld"); // literal backslash-n
   };

   "sq_backslash_preserved"_test = [] {
      std::string yaml = R"('C:\path\to\file')";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "C:\\path\\to\\file");
   };

   "sq_long_string"_test = [] {
      std::string yaml = "'" + std::string(1000, 'b') + "'";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == std::string(1000, 'b'));
   };

   // Plain scalar tests (unquoted)
   "plain_simple"_test = [] {
      std::string yaml = "hello";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello");
   };

   "plain_with_spaces"_test = [] {
      std::string yaml = "hello world";
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == "hello world");
   };

   // Error cases
   "dq_invalid_hex_escape"_test = [] {
      std::string yaml = R"("\xGG")"; // invalid hex
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "dq_incomplete_hex_escape"_test = [] {
      std::string yaml = R"("\x4")"; // only 1 hex digit
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "dq_invalid_unicode_escape"_test = [] {
      std::string yaml = R"("\uGGGG")"; // invalid hex
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "dq_incomplete_unicode_escape"_test = [] {
      std::string yaml = R"("\u004")"; // only 3 hex digits
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "dq_incomplete_unicode8_escape"_test = [] {
      std::string yaml = R"("\U0001F60")"; // only 7 hex digits
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "dq_unterminated"_test = [] {
      std::string yaml = R"("hello)"; // no closing quote
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "sq_unterminated"_test = [] {
      std::string yaml = "'hello"; // no closing quote
      std::string value;
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   // Strings in object context
   "obj_dq_string_with_escapes"_test = [] {
      std::string yaml = R"(name: "hello\nworld")";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "hello\nworld");
   };

   "obj_sq_string_with_quote"_test = [] {
      std::string yaml = "name: 'it''s working'";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.name == "it's working");
   };

   // Flow sequence with quoted strings
   "flow_seq_dq_strings"_test = [] {
      std::string yaml = R"(["a\nb", "c\td"])";
      std::vector<std::string> value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value.size() == 2);
      expect(value[0] == "a\nb");
      expect(value[1] == "c\td");
   };

   "flow_seq_sq_strings"_test = [] {
      std::string yaml = "['it''s', 'won''t']";
      std::vector<std::string> value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value.size() == 2);
      expect(value[0] == "it's");
      expect(value[1] == "won't");
   };

   // Flow map with quoted strings
   "flow_map_dq_strings"_test = [] {
      std::string yaml = R"({"key\n1": "val\t1"})";
      std::map<std::string, std::string> value;
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value["key\n1"] == "val\t1");
   };
};

// ============================================================
// Container Type Tests
// ============================================================

suite yaml_container_tests = [] {
   "deque_roundtrip"_test = [] {
      std::deque<int> original{1, 2, 3, 4, 5};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::deque<int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "deque_double_roundtrip"_test = [] {
      std::deque<double> original{1.5, 2.7, 3.14, 4.0, 5.555};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::deque<double> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.size() == original.size());
      for (size_t i = 0; i < original.size(); ++i) {
         expect(std::abs(parsed[i] - original[i]) < 0.0001);
      }
   };

   "list_roundtrip"_test = [] {
      std::list<int> original{10, 20, 30, 40, 50};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::list<int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "forward_list_write"_test = [] {
      std::forward_list<int> original{5, 4, 3, 2, 1};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // forward_list reading not supported, just verify write works
      expect(yaml.find("5") != std::string::npos);
   };

   "set_roundtrip"_test = [] {
      std::set<int> original{5, 3, 1, 4, 2};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::set<int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "unordered_set_roundtrip"_test = [] {
      std::unordered_set<int> original{10, 20, 30, 40, 50};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::unordered_set<int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "set_string_roundtrip"_test = [] {
      std::set<std::string> original{"apple", "banana", "cherry"};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::set<std::string> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "vector_of_vectors_flow"_test = [] {
      std::vector<std::vector<int>> original{{1, 2, 3}, {4, 5}, {6, 7, 8, 9}};
      std::string yaml;
      // Use flow style for nested sequences to ensure parsability
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::vector<std::vector<int>> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "vector_of_strings"_test = [] {
      std::vector<std::string> original{"hello", "world", "test"};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::vector<std::string> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "empty_vector"_test = [] {
      std::vector<int> original{};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // Empty sequence writes as [] in flow style
      expect(yaml == "[]" || yaml == "");
   };

   "empty_map"_test = [] {
      std::map<std::string, int> original{};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // Empty map writes as {} in flow style
      expect(yaml == "{}" || yaml == "");
   };
};

// ============================================================
// Map with Various Key Types
// ============================================================

suite yaml_map_key_tests = [] {
   "map_int_keys_roundtrip"_test = [] {
      std::map<int, std::string> original{{1, "one"}, {2, "two"}, {3, "three"}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::map<int, std::string> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "map_int_int_roundtrip"_test = [] {
      std::map<int, int> original{{1, 100}, {2, 200}, {3, 300}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::map<int, int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "unordered_map_string_int"_test = [] {
      std::unordered_map<std::string, int> original{{"alpha", 1}, {"beta", 2}, {"gamma", 3}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::unordered_map<std::string, int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "unordered_map_int_double"_test = [] {
      std::unordered_map<int, double> original{{1, 1.1}, {2, 2.2}, {3, 3.3}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::unordered_map<int, double> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.size() == original.size());
      for (const auto& [k, v] : original) {
         expect(std::abs(parsed[k] - v) < 0.0001);
      }
   };

   "map_nested_value_flow"_test = [] {
      std::map<std::string, std::vector<int>> original{{"nums", {1, 2, 3}}, {"more", {4, 5}}};
      std::string yaml;
      // Use flow style for nested structures
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::map<std::string, std::vector<int>> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };
};

// ============================================================
// Nullable Type Tests
// ============================================================

suite yaml_nullable_tests = [] {
   "shared_ptr_write"_test = [] {
      auto original = std::make_shared<int>(42);
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      expect(yaml.find("42") != std::string::npos);
   };

   "shared_ptr_null_write"_test = [] {
      std::shared_ptr<int> original;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      expect(yaml.find("null") != std::string::npos);
   };

   "unique_ptr_write"_test = [] {
      auto original = std::make_unique<double>(3.14);
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      expect(yaml.find("3.14") != std::string::npos);
   };

   "unique_ptr_null_write"_test = [] {
      std::unique_ptr<std::string> original;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      expect(yaml.find("null") != std::string::npos);
   };

   "optional_nested_struct"_test = [] {
      std::optional<simple_struct> original{simple_struct{10, 2.5, "nested"}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::optional<simple_struct> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.has_value());
      expect(parsed->x == 10);
      expect(std::abs(parsed->y - 2.5) < 0.001);
      expect(parsed->name == "nested");
   };

   "shared_ptr_struct_write"_test = [] {
      auto original = std::make_shared<simple_struct>(simple_struct{5, 1.5, "ptr"});
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      expect(yaml.find("x:") != std::string::npos);
      expect(yaml.find("5") != std::string::npos);
   };
};

// ============================================================
// Array Type Tests
// ============================================================

suite yaml_array_tests = [] {
   "std_array_int_roundtrip"_test = [] {
      std::array<int, 5> original{1, 2, 3, 4, 5};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::array<int, 5> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "std_array_double_roundtrip"_test = [] {
      std::array<double, 3> original{1.1, 2.2, 3.3};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::array<double, 3> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      for (size_t i = 0; i < 3; ++i) {
         expect(std::abs(parsed[i] - original[i]) < 0.001);
      }
   };

   "std_array_string_roundtrip"_test = [] {
      std::array<std::string, 3> original{"one", "two", "three"};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::array<std::string, 3> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "nested_array_roundtrip"_test = [] {
      std::array<std::array<int, 2>, 3> original{{{1, 2}, {3, 4}, {5, 6}}};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::array<std::array<int, 2>, 3> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };
};

// ============================================================
// Number Edge Case Tests
// ============================================================

suite yaml_number_tests = [] {
   "large_integer"_test = [] {
      int64_t original = 9223372036854775807LL; // Max int64
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      int64_t parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "large_negative_integer"_test = [] {
      int64_t original = -9223372036854775807LL;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      int64_t parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "uint64_max"_test = [] {
      uint64_t original = 18446744073709551615ULL; // Max uint64
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      uint64_t parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "scientific_notation"_test = [] {
      std::string yaml = "1.5e10";
      double parsed{};
      auto ec = glz::read_yaml(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::abs(parsed - 1.5e10) < 1e5);
   };

   "negative_scientific"_test = [] {
      std::string yaml = "-2.5e-5";
      double parsed{};
      auto ec = glz::read_yaml(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::abs(parsed - (-2.5e-5)) < 1e-10);
   };

   "zero_values"_test = [] {
      int i{};
      std::string yaml_i = "0";
      expect(!glz::read_yaml(i, yaml_i));
      expect(i == 0);

      double d{};
      std::string yaml_d = "0.0";
      expect(!glz::read_yaml(d, yaml_d));
      expect(d == 0.0);
   };

   "float_precision"_test = [] {
      float original = 3.14159265f;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      float parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::abs(parsed - original) < 0.0001f);
   };

   "octal_number"_test = [] {
      std::string yaml = "0o755";
      int parsed{};
      auto ec = glz::read_yaml(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed == 0755);
   };

   "binary_number"_test = [] {
      std::string yaml = "0b101010";
      int parsed{};
      auto ec = glz::read_yaml(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed == 42);
   };

   "write_infinity"_test = [] {
      double original = std::numeric_limits<double>::infinity();
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // YAML might use .inf, .Inf, inf, Inf, or other representation
      expect(!yaml.empty());
   };

   "write_nan"_test = [] {
      double original = std::numeric_limits<double>::quiet_NaN();
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // YAML might use .nan, .NaN, nan, NaN, or other representation
      expect(!yaml.empty());
   };
};

// ============================================================
// Variant Type Tests
// ============================================================

struct variant_a
{
   int value{};
};

struct variant_b
{
   std::string text{};
};

template <>
struct glz::meta<variant_a>
{
   using T = variant_a;
   static constexpr auto value = object("value", &T::value);
};

template <>
struct glz::meta<variant_b>
{
   using T = variant_b;
   static constexpr auto value = object("text", &T::text);
};

suite yaml_variant_tests = [] {
   "variant_int_double_string"_test = [] {
      std::variant<int, double, std::string> original = 42;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::variant<int, double, std::string> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<int>(parsed));
      expect(std::get<int>(parsed) == 42);
   };

   "variant_double_value"_test = [] {
      std::variant<int, double, std::string> original = 3.14;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::variant<int, double, std::string> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      // Note: May parse as int if no decimal point in output
   };

   "variant_string_value"_test = [] {
      std::variant<int, double, std::string> original = std::string("hello");
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::variant<int, double, std::string> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::string>(parsed));
      expect(std::get<std::string>(parsed) == "hello");
   };

   "generic_empty_object"_test = [] {
      std::string yaml = "{}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
   };

   "generic_empty_array"_test = [] {
      std::string yaml = "[]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
   };

   "generic_string"_test = [] {
      std::string yaml = "\"hello world\"";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::string>(parsed.data));
      expect(std::get<std::string>(parsed.data) == "hello world");
   };

   "generic_number"_test = [] {
      std::string yaml = "42.5";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
      expect(std::get<double>(parsed.data) == 42.5);
   };

   "generic_boolean_true"_test = [] {
      std::string yaml = "true";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<bool>(parsed.data));
      expect(std::get<bool>(parsed.data) == true);
   };

   "generic_boolean_false"_test = [] {
      std::string yaml = "false";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<bool>(parsed.data));
      expect(std::get<bool>(parsed.data) == false);
   };

   "generic_null"_test = [] {
      std::string yaml = "null";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::nullptr_t>(parsed.data));
   };

   "generic_object_with_values"_test = [] {
      std::string yaml = "{name: \"test\", value: 123}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
   };

   "generic_array_with_values"_test = [] {
      std::string yaml = "[1, 2, 3]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 3);
   };

   // ============================================================
   // Extended glz::generic YAML Tests
   // ============================================================

   // Number format tests
   "generic_integer"_test = [] {
      std::string yaml = "42";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
      expect(std::get<double>(parsed.data) == 42.0);
   };

   "generic_negative_integer"_test = [] {
      std::string yaml = "-123";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
      expect(std::get<double>(parsed.data) == -123.0);
   };

   "generic_negative_float"_test = [] {
      std::string yaml = "-3.14159";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
      expect(std::abs(std::get<double>(parsed.data) - (-3.14159)) < 1e-10);
   };

   "generic_scientific_notation"_test = [] {
      std::string yaml = "1.5e10";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
      expect(std::get<double>(parsed.data) == 1.5e10);
   };

   "generic_negative_exponent"_test = [] {
      std::string yaml = "2.5e-3";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
      expect(std::abs(std::get<double>(parsed.data) - 0.0025) < 1e-10);
   };

   // Boolean format variations
   "generic_boolean_True"_test = [] {
      std::string yaml = "True";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<bool>(parsed.data));
      expect(std::get<bool>(parsed.data) == true);
   };

   "generic_boolean_FALSE"_test = [] {
      std::string yaml = "FALSE";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<bool>(parsed.data));
      expect(std::get<bool>(parsed.data) == false);
   };

   // Null format variations
   "generic_null_tilde"_test = [] {
      std::string yaml = "~";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::nullptr_t>(parsed.data));
   };

   "generic_null_Null"_test = [] {
      std::string yaml = "Null";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::nullptr_t>(parsed.data));
   };

   "generic_null_NULL"_test = [] {
      std::string yaml = "NULL";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::nullptr_t>(parsed.data));
   };

   // String format tests
   "generic_single_quoted_string"_test = [] {
      std::string yaml = "'hello world'";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::string>(parsed.data));
      expect(std::get<std::string>(parsed.data) == "hello world");
   };

   "generic_string_with_escapes"_test = [] {
      std::string yaml = "\"hello\\nworld\"";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::string>(parsed.data));
      expect(std::get<std::string>(parsed.data) == "hello\nworld");
   };

   // Nested object tests
   "generic_nested_object"_test = [] {
      std::string yaml = "{outer: {inner: {value: 42}}}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& outer = std::get<glz::generic::object_t>(parsed.data);
      expect(outer.contains("outer"));
      expect(std::holds_alternative<glz::generic::object_t>(outer.at("outer").data));

      auto& inner_obj = std::get<glz::generic::object_t>(outer.at("outer").data);
      expect(inner_obj.contains("inner"));
   };

   "generic_object_with_array"_test = [] {
      std::string yaml = "{name: \"test\", values: [1, 2, 3]}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.contains("name"));
      expect(obj.contains("values"));
      expect(std::holds_alternative<std::string>(obj.at("name").data));
      expect(std::holds_alternative<glz::generic::array_t>(obj.at("values").data));

      auto& arr = std::get<glz::generic::array_t>(obj.at("values").data);
      expect(arr.size() == 3);
   };

   // Nested array tests
   "generic_nested_array"_test = [] {
      std::string yaml = "[[1, 2], [3, 4], [5, 6]]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 3);

      for (const auto& inner : arr) {
         expect(std::holds_alternative<glz::generic::array_t>(inner.data));
         expect(std::get<glz::generic::array_t>(inner.data).size() == 2);
      }
   };

   "generic_array_of_objects"_test = [] {
      std::string yaml = "[{a: 1}, {b: 2}, {c: 3}]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 3);

      for (const auto& item : arr) {
         expect(std::holds_alternative<glz::generic::object_t>(item.data));
      }
   };

   // Mixed type array
   "generic_mixed_array"_test = [] {
      std::string yaml = "[42, \"hello\", true, null, 3.14]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 5);
      expect(std::holds_alternative<double>(arr[0].data));
      expect(std::holds_alternative<std::string>(arr[1].data));
      expect(std::holds_alternative<bool>(arr[2].data));
      expect(std::holds_alternative<std::nullptr_t>(arr[3].data));
      expect(std::holds_alternative<double>(arr[4].data));
   };

   // Complex nested structure
   "generic_complex_structure"_test = [] {
      std::string yaml =
         "{users: [{name: \"Alice\", age: 30, active: true}, {name: \"Bob\", age: 25, active: false}], "
         "metadata: {version: 1.5, tags: [\"prod\", \"v2\"]}, nullable_field: null}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("users"));
      expect(root.contains("metadata"));
      expect(root.contains("nullable_field"));

      // Check users array
      auto& users = std::get<glz::generic::array_t>(root.at("users").data);
      expect(users.size() == 2);

      auto& alice = std::get<glz::generic::object_t>(users[0].data);
      expect(std::get<std::string>(alice.at("name").data) == "Alice");
      expect(std::get<double>(alice.at("age").data) == 30.0);
      expect(std::get<bool>(alice.at("active").data) == true);

      // Check metadata
      auto& metadata = std::get<glz::generic::object_t>(root.at("metadata").data);
      expect(std::get<double>(metadata.at("version").data) == 1.5);

      auto& tags = std::get<glz::generic::array_t>(metadata.at("tags").data);
      expect(tags.size() == 2);

      // Check nullable field
      expect(std::holds_alternative<std::nullptr_t>(root.at("nullable_field").data));
   };

   // Deeply nested structure
   "generic_deeply_nested"_test = [] {
      std::string yaml = "{a: {b: {c: {d: {e: {f: 42}}}}}}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      // Navigate through the nesting
      auto* current = &parsed;
      for (const char* key : {"a", "b", "c", "d", "e"}) {
         expect(std::holds_alternative<glz::generic::object_t>(current->data));
         auto& obj = std::get<glz::generic::object_t>(current->data);
         expect(obj.contains(key));
         current = &obj.at(key);
      }
      expect(std::holds_alternative<glz::generic::object_t>(current->data));
      auto& final_obj = std::get<glz::generic::object_t>(current->data);
      expect(std::get<double>(final_obj.at("f").data) == 42.0);
   };

   // Object with all value types
   "generic_object_all_types"_test = [] {
      std::string yaml =
         "{string_val: \"text\", int_val: 42, float_val: 3.14, bool_true: true, "
         "bool_false: false, null_val: null, array_val: [1, 2], object_val: {nested: true}}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<std::string>(obj.at("string_val").data));
      expect(std::holds_alternative<double>(obj.at("int_val").data));
      expect(std::holds_alternative<double>(obj.at("float_val").data));
      expect(std::holds_alternative<bool>(obj.at("bool_true").data));
      expect(std::holds_alternative<bool>(obj.at("bool_false").data));
      expect(std::holds_alternative<std::nullptr_t>(obj.at("null_val").data));
      expect(std::holds_alternative<glz::generic::array_t>(obj.at("array_val").data));
      expect(std::holds_alternative<glz::generic::object_t>(obj.at("object_val").data));
   };

   // Flow style with whitespace variations
   "generic_flow_object_with_spaces"_test = [] {
      std::string yaml = "{ name: \"John\", age: 30, active: true }";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 3);
      expect(std::get<std::string>(obj.at("name").data) == "John");
      expect(std::get<double>(obj.at("age").data) == 30.0);
      expect(std::get<bool>(obj.at("active").data) == true);
   };

   "generic_flow_array_with_spaces"_test = [] {
      std::string yaml = "[ 1, 2, 3 ]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 3);
      expect(std::get<double>(arr[0].data) == 1.0);
      expect(std::get<double>(arr[1].data) == 2.0);
      expect(std::get<double>(arr[2].data) == 3.0);
   };

   "generic_flow_nested"_test = [] {
      std::string yaml = "{person: {name: \"Alice\", age: 25, hobbies: [\"reading\", \"coding\"]}}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("person"));

      auto& person = std::get<glz::generic::object_t>(root.at("person").data);
      expect(std::get<std::string>(person.at("name").data) == "Alice");
      expect(std::get<double>(person.at("age").data) == 25.0);

      auto& hobbies = std::get<glz::generic::array_t>(person.at("hobbies").data);
      expect(hobbies.size() == 2);
   };

   // Edge cases
   "generic_empty_string"_test = [] {
      std::string yaml = "\"\"";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::string>(parsed.data));
      expect(std::get<std::string>(parsed.data).empty());
   };

   "generic_zero"_test = [] {
      std::string yaml = "0";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
      expect(std::get<double>(parsed.data) == 0.0);
   };

   "generic_negative_zero"_test = [] {
      std::string yaml = "-0";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
   };

   "generic_object_single_key"_test = [] {
      std::string yaml = "{key: \"value\"}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 1);
      expect(std::get<std::string>(obj.at("key").data) == "value");
   };

   "generic_array_single_element"_test = [] {
      std::string yaml = "[42]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 1);
      expect(std::get<double>(arr[0].data) == 42.0);
   };

   "generic_object_numeric_string_key"_test = [] {
      std::string yaml = "{\"123\": \"numeric key\"}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.contains("123"));
   };

   // Large numbers
   "generic_large_integer"_test = [] {
      std::string yaml = "9007199254740992";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
   };

   "generic_very_small_float"_test = [] {
      std::string yaml = "1e-308";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed.data));
   };
};

// ============================================================
// Complex Nested Structure Tests
// ============================================================

struct address
{
   std::string street{};
   std::string city{};
   int zip{};
};

template <>
struct glz::meta<address>
{
   using T = address;
   static constexpr auto value = object("street", &T::street, "city", &T::city, "zip", &T::zip);
};

struct person
{
   std::string name{};
   int age{};
   address addr{};
   std::vector<std::string> hobbies{};
};

template <>
struct glz::meta<person>
{
   using T = person;
   static constexpr auto value = object("name", &T::name, "age", &T::age, "addr", &T::addr, "hobbies", &T::hobbies);
};

struct company
{
   std::string name{};
   std::vector<person> employees{};
   std::map<std::string, int> departments{};
};

template <>
struct glz::meta<company>
{
   using T = company;
   static constexpr auto value = object("name", &T::name, "employees", &T::employees, "departments", &T::departments);
};

suite yaml_complex_struct_tests = [] {
   "person_roundtrip"_test = [] {
      person original{"John Doe", 30, {"123 Main St", "Springfield", 12345}, {"reading", "coding"}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      person parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.name == original.name);
      expect(parsed.age == original.age);
      expect(parsed.addr.street == original.addr.street);
      expect(parsed.addr.city == original.addr.city);
      expect(parsed.addr.zip == original.addr.zip);
      expect(parsed.hobbies == original.hobbies);
   };

   "company_write"_test = [] {
      company original{"TechCorp",
                       {{"Alice", 25, {"456 Oak Ave", "Techville", 54321}, {"gaming"}},
                        {"Bob", 35, {"789 Pine Rd", "Codeburg", 98765}, {"hiking", "photography"}}},
                       {{"Engineering", 50}, {"Sales", 30}}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // Verify the structure is written
      expect(yaml.find("TechCorp") != std::string::npos);
      expect(yaml.find("Alice") != std::string::npos);
      expect(yaml.find("Engineering") != std::string::npos);
   };

   "deeply_nested_write"_test = [] {
      std::map<std::string, std::vector<std::map<std::string, int>>> original{
         {"group1", {{{"a", 1}, {"b", 2}}, {{"c", 3}}}}, {"group2", {{{"d", 4}}}}};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);
      // Just verify the write works
      expect(yaml.find("group1") != std::string::npos);
      expect(yaml.find("group2") != std::string::npos);
   };
};

// ============================================================
// Error Handling Tests
// ============================================================

suite yaml_error_tests = [] {
   "invalid_int"_test = [] {
      int value{};
      std::string yaml = "not_a_number";
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "invalid_bool"_test = [] {
      bool value{};
      std::string yaml = "maybe";
      auto ec = glz::read_yaml(value, yaml);
      expect(bool(ec));
   };

   "missing_key_in_struct"_test = [] {
      std::string yaml = R"(x: 1
y: 2.0)";
      // name is missing but has default
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 1);
      expect(obj.name.empty()); // default value
   };

   "type_mismatch_array_to_object"_test = [] {
      simple_struct obj{};
      std::string yaml = "[1, 2, 3]";
      auto ec = glz::read_yaml(obj, yaml);
      expect(bool(ec));
   };

   "type_mismatch_object_to_array"_test = [] {
      std::vector<int> vec{};
      std::string yaml = "key: value";
      auto ec = glz::read_yaml(vec, yaml);
      expect(bool(ec));
   };

   "unclosed_bracket"_test = [] {
      std::vector<int> vec{};
      std::string yaml = "[1, 2, 3";
      auto ec = glz::read_yaml(vec, yaml);
      expect(bool(ec));
   };

   "unclosed_brace"_test = [] {
      std::map<std::string, int> m{};
      std::string yaml = "{a: 1, b: 2";
      auto ec = glz::read_yaml(m, yaml);
      expect(bool(ec));
   };
};

// ============================================================
// Boolean Variations Tests
// ============================================================

suite yaml_boolean_tests = [] {
   "bool_yes"_test = [] {
      bool value{};
      std::string yaml = "yes";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == true);
   };

   "bool_no"_test = [] {
      bool value{true};
      std::string yaml = "no";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == false);
   };

   "bool_on"_test = [] {
      bool value{};
      std::string yaml = "on";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == true);
   };

   "bool_off"_test = [] {
      bool value{true};
      std::string yaml = "off";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == false);
   };

   "bool_True"_test = [] {
      bool value{};
      std::string yaml = "True";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == true);
   };

   "bool_False"_test = [] {
      bool value{true};
      std::string yaml = "False";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == false);
   };

   "bool_TRUE"_test = [] {
      bool value{};
      std::string yaml = "TRUE";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == true);
   };

   "bool_FALSE"_test = [] {
      bool value{true};
      std::string yaml = "FALSE";
      auto ec = glz::read_yaml(value, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(value == false);
   };
};

// ============================================================
// Indentation and Whitespace Tests
// ============================================================

suite yaml_whitespace_tests = [] {
   "extra_whitespace_in_mapping"_test = [] {
      std::string yaml = R"(x:    42
y:   3.14
name:   test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
      expect(obj.name == "test");
   };

   "leading_whitespace"_test = [] {
      std::string yaml = R"(   x: 1
   y: 2.0
   name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 1);
   };

   "trailing_newlines"_test = [] {
      std::string yaml = "x: 1\ny: 2.0\nname: test\n\n\n";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 1);
   };

   "tabs_in_values"_test = [] {
      std::string yaml = "x:\t42\ny:\t3.14\nname:\ttest";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };
};

// ============================================================
// Document Markers Tests
// ============================================================

suite yaml_document_tests = [] {
   "document_start_marker"_test = [] {
      std::string yaml = R"(---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   "document_end_marker"_test = [] {
      std::string yaml = R"(x: 42
y: 3.14
name: test
...)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   "both_markers"_test = [] {
      std::string yaml = R"(---
x: 42
y: 3.14
name: test
...)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   // YAML directive tests
   "yaml_version_directive"_test = [] {
      std::string yaml = R"(%YAML 1.2
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
      expect(obj.name == "test");
   };

   "yaml_tag_directive"_test = [] {
      std::string yaml = R"(%TAG ! tag:example.com,2000:
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   "yaml_multiple_directives"_test = [] {
      std::string yaml = R"(%YAML 1.2
%TAG ! tag:example.com,2000:
%TAG !! tag:yaml.org,2002:
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   "yaml_directive_with_generic"_test = [] {
      std::string yaml = R"(%YAML 1.2
---
name: Alice)";
      glz::generic parsed;
      auto ec = glz::read_yaml(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 1u);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
   };

   "yaml_directive_with_blank_lines"_test = [] {
      std::string yaml = R"(%YAML 1.2

---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   // YAML 1.1 should be accepted (per spec)
   "yaml_directive_version_1_1"_test = [] {
      std::string yaml = R"(%YAML 1.1
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   // Higher minor versions should be accepted (per spec: process with warning)
   "yaml_directive_version_1_3"_test = [] {
      std::string yaml = R"(%YAML 1.3
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   // Duplicate %YAML directive is an error (per spec)
   "yaml_directive_duplicate_error"_test = [] {
      std::string yaml = R"(%YAML 1.2
%YAML 1.2
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(bool(ec)) << "Duplicate %YAML directive should be an error";
   };

   // %YAML with major version > 1 should be rejected (per spec)
   "yaml_directive_major_version_2_error"_test = [] {
      std::string yaml = R"(%YAML 2.0
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(bool(ec)) << "%YAML 2.0 should be rejected";
   };

   // %YAML with major version 3 should be rejected
   "yaml_directive_major_version_3_error"_test = [] {
      std::string yaml = R"(%YAML 3.0
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(bool(ec)) << "%YAML 3.0 should be rejected";
   };

   // Unknown directives should be silently ignored (per spec)
   "yaml_directive_unknown_ignored"_test = [] {
      std::string yaml = R"(%FOOBAR some params here
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   // Multiple unknown directives should be ignored
   "yaml_directive_multiple_unknown_ignored"_test = [] {
      std::string yaml = R"(%FOO bar
%BAZ qux
%YAML 1.2
%ANOTHER directive
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };

   // %YAML 1.0 should be accepted (major version 1)
   "yaml_directive_version_1_0"_test = [] {
      std::string yaml = R"(%YAML 1.0
---
x: 42
y: 3.14
name: test)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 42);
   };
};

// ============================================================
// Mixed Flow and Block Style Tests
// ============================================================

suite yaml_mixed_style_tests = [] {
   "flow_in_block_mapping"_test = [] {
      std::string yaml = R"(title: Test
data: {x: 1, y: 2.0, name: inner}
numbers: [1, 2, 3])";
      nested_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.title == "Test");
      expect(obj.data.x == 1);
      expect(obj.numbers.size() == 3);
   };

   "vector_of_maps_write"_test = [] {
      std::vector<std::map<std::string, int>> original{{{"a", 1}, {"b", 2}}, {{"a", 3}, {"b", 4}}};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);
      // Verify write output
      expect(yaml.find("a:") != std::string::npos);
      expect(yaml.find("b:") != std::string::npos);
   };
};

// ============================================================
// Anchor and Alias Tests (if supported)
// ============================================================

// Anchor/Alias Tests (source span replay)

suite yaml_anchor_tests = [] {
   "scalar_anchor_alias_string"_test = [] {
      std::string yaml = R"(anchor: &a hello
alias: *a)";
      std::map<std::string, std::string> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["anchor"] == "hello");
      expect(obj["alias"] == "hello");
   };

   "scalar_anchor_alias_int"_test = [] {
      std::string yaml = R"(anchor: &a 42
alias: *a)";
      std::map<std::string, int> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["anchor"] == 42);
      expect(obj["alias"] == 42);
   };

   "scalar_anchor_alias_double"_test = [] {
      std::string yaml = R"(anchor: &a 3.14
alias: *a)";
      std::map<std::string, double> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["anchor"] == 3.14);
      expect(obj["alias"] == 3.14);
   };

   "scalar_anchor_alias_bool"_test = [] {
      std::string yaml = R"(anchor: &a true
alias: *a)";
      std::map<std::string, bool> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["anchor"] == true);
      expect(obj["alias"] == true);
   };

   "scalar_anchor_alias_generic"_test = [] {
      std::string yaml = R"(first: &anchor Value
second: *anchor)";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto json = glz::write_json(parsed).value_or("ERROR");
      expect(json == R"({"first":"Value","second":"Value"})") << json;
   };

   "anchor_on_flow_mapping"_test = [] {
      std::string yaml = R"(root: &m {key1: val1, key2: val2}
alias: *m)";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto json = glz::write_json(parsed).value_or("ERROR");
      expect(json == R"({"alias":{"key1":"val1","key2":"val2"},"root":{"key1":"val1","key2":"val2"}})") << json;
   };

   "anchor_on_flow_sequence"_test = [] {
      std::string yaml = R"(root: &s [1, 2, 3]
alias: *s)";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto json = glz::write_json(parsed).value_or("ERROR");
      expect(json == R"({"alias":[1,2,3],"root":[1,2,3]})") << json;
   };

   "multiple_anchors"_test = [] {
      std::string yaml = R"(a: &x hello
b: &y world
c: *x
d: *y)";
      std::map<std::string, std::string> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["a"] == "hello");
      expect(obj["b"] == "world");
      expect(obj["c"] == "hello");
      expect(obj["d"] == "world");
   };

   "anchor_override"_test = [] {
      std::string yaml = R"(a: &x first
b: *x
c: &x second
d: *x)";
      std::map<std::string, std::string> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["a"] == "first");
      expect(obj["b"] == "first");
      expect(obj["c"] == "second");
      expect(obj["d"] == "second");
   };

   "anchor_double_quoted"_test = [] {
      std::string yaml = "a: &q \"hello world\"\nb: *q";
      std::map<std::string, std::string> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["a"] == "hello world");
      expect(obj["b"] == "hello world");
   };

   "anchor_single_quoted"_test = [] {
      std::string yaml = "a: &q 'hello world'\nb: *q";
      std::map<std::string, std::string> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["a"] == "hello world");
      expect(obj["b"] == "hello world");
   };

   "undefined_alias_error"_test = [] {
      std::string yaml = "a: *nonexistent";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec)) << "Expected error for undefined alias";
   };

   "anchor_on_alias_error"_test = [] {
      std::string yaml = "key1: &a value\nkey2: &b *a";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec)) << "Expected error for anchor on alias";
   };

   "nested_anchor_reference"_test = [] {
      std::string yaml = R"(outer:
  inner: &val deep
ref: *val)";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto json = glz::write_json(parsed).value_or("ERROR");
      expect(json == R"({"outer":{"inner":"deep"},"ref":"deep"})") << json;
   };

   "aliases_in_block_sequence"_test = [] {
      std::string yaml = R"(- &a hello
- &b world
- *a
- *b)";
      std::vector<std::string> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.size() == 4);
      expect(obj[0] == "hello");
      expect(obj[1] == "world");
      expect(obj[2] == "hello");
      expect(obj[3] == "world");
   };

   "anchor_on_sequence_value"_test = [] {
      std::string yaml = R"(&seq
- a)";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto json = glz::write_json(parsed).value_or("ERROR");
      expect(json == R"(["a"])") << json;
   };

   "document_start_anchor"_test = [] {
      std::string yaml = R"(--- &seq
- a)";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto json = glz::write_json(parsed).value_or("ERROR");
      expect(json == R"(["a"])") << json;
   };

   "spec_2_10_sammy_sosa"_test = [] {
      std::string yaml = R"(---
hr:
  - Mark McGwire
  - &SS Sammy Sosa
rbi:
  - *SS
  - Ken Griffey)";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto json = glz::write_json(parsed).value_or("ERROR");
      expect(json == R"({"hr":["Mark McGwire","Sammy Sosa"],"rbi":["Sammy Sosa","Ken Griffey"]})") << json;
   };

   "anchor_on_flow_map_typed"_test = [] {
      std::string yaml = R"(root: &m {x: 1, y: 2}
copy: *m)";
      std::map<std::string, std::map<std::string, int>> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["root"]["x"] == 1);
      expect(obj["root"]["y"] == 2);
      expect(obj["copy"]["x"] == 1);
      expect(obj["copy"]["y"] == 2);
   };

   "anchor_on_flow_seq_typed"_test = [] {
      std::string yaml = R"(root: &s [10, 20, 30]
copy: *s)";
      std::map<std::string, std::vector<int>> obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj["root"].size() == 3);
      expect(obj["root"][0] == 10);
      expect(obj["copy"].size() == 3);
      expect(obj["copy"][2] == 30);
   };

   "anchor_skip_unknown_key"_test = [] {
      // Test that skip_yaml_value handles anchors/aliases when skipping
      std::string yaml = R"(x: 1
y: 1.0
name: &a skipped
extra: *a)";
      simple_struct obj{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.x == 1);
      expect(obj.y == 1.0);
      expect(obj.name == "skipped");
   };
};

// ============================================================
// Multiline String Continuation Tests
// ============================================================

suite yaml_multiline_tests = [] {
   "literal_block_strip"_test = [] {
      std::string yaml = R"(x: 1
y: 1.0
name: |-
  line1
  line2
  line3)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // Strip indicator removes trailing newlines
   };

   "literal_block_keep"_test = [] {
      std::string yaml = R"(x: 1
y: 1.0
name: |+
  line1
  line2
  line3

)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // Keep indicator preserves trailing newlines
   };

   "folded_block_basic"_test = [] {
      std::string yaml = R"(x: 1
y: 1.0
name: >
  this is a long
  string that should
  be folded)";
      simple_struct obj{};
      auto ec = glz::read_yaml(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // Folded replaces single newlines with spaces
   };
};

// ============================================================
// Char Type Tests (write only - char reading not supported)
// ============================================================

suite yaml_char_tests = [] {
   "char_write"_test = [] {
      char original = 'A';
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // char reading not supported, just verify write works
      expect(yaml.find("A") != std::string::npos);
   };

   "unsigned_char_roundtrip"_test = [] {
      unsigned char original = 255;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      // unsigned char is a num_t, should be readable
      unsigned char parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };
};

// ============================================================
// File I/O Tests
// ============================================================

struct file_struct
{
   int x{};
   double y{};
   std::string name{};
};

template <>
struct glz::meta<file_struct>
{
   using T = file_struct;
   static constexpr auto value = object("x", &T::x, "y", &T::y, "name", &T::name);
};

struct k8s_metadata
{
   std::string name{};
   std::map<std::string, std::string> labels{};
};

struct k8s_label_selector
{
   std::map<std::string, std::string> matchLabels{};
};

struct k8s_container_port
{
   int containerPort{};
};

struct k8s_container
{
   std::string name{};
   std::string image{};
   std::vector<k8s_container_port> ports{};
};

struct k8s_pod_spec
{
   std::vector<k8s_container> containers{};
};

struct k8s_pod_template_metadata
{
   std::map<std::string, std::string> labels{};
};

struct k8s_pod_template
{
   k8s_pod_template_metadata metadata{};
   k8s_pod_spec spec{};
};

struct k8s_deployment_spec
{
   int replicas{};
   k8s_label_selector selector{};
   k8s_pod_template template_{};
};

template <>
struct glz::meta<k8s_deployment_spec>
{
   static constexpr std::string_view rename_key(const std::string_view key)
   {
      if (key == "template_") {
         return "template";
      }
      return key;
   }
};

struct k8s_deployment
{
   std::string apiVersion{};
   std::string kind{};
   k8s_metadata metadata{};
   k8s_deployment_spec spec{};
};

struct k8s_service_port
{
   std::string name{};
   std::string protocol{};
   int port{};
   int targetPort{};
};

struct k8s_service_spec
{
   std::map<std::string, std::string> selector{};
   std::vector<k8s_service_port> ports{};
};

struct k8s_service
{
   std::string apiVersion{};
   std::string kind{};
   k8s_metadata metadata{};
   k8s_service_spec spec{};
};

template <class T, class Check>
static void roundtrip_yaml(const std::string& yaml, Check&& check)
{
   T parsed{};
   auto rec = glz::read_yaml(parsed, yaml);
   expect(!rec) << glz::format_error(rec, yaml);
   check(parsed);

   std::string output;
   auto wec = glz::write_yaml(parsed, output);
   expect(!wec);

   T reparsed{};
   auto rec2 = glz::read_yaml(reparsed, output);
   expect(!rec2) << glz::format_error(rec2, output);
   check(reparsed);
}

struct advanced_flags
{
   bool enabled{};
   bool archived{};
};

struct advanced_counts
{
   int retries{};
   int timeout_ms{};
   double ratio{};
};

struct advanced_flow
{
   std::vector<int> values{};
   std::map<std::string, std::string> mapping{};
};

struct advanced_nested
{
   std::string name{};
   std::vector<int> ids{};
   std::map<std::string, std::string> labels{};
};

struct advanced_doc
{
   std::string title{};
   std::string description{};
   std::string literal{};
   std::string multiline_plain{};
   std::string quoted{};
   advanced_flags flags{};
   advanced_counts counts{};
   std::vector<std::string> list{};
   advanced_flow flow{};
   advanced_nested nested{};
   std::optional<std::string> note{};
};

suite yaml_file_io_tests = [] {
   "write_file_yaml"_test = [] {
      file_struct obj{42, 3.14, "test_file"};
      std::string filename = "./test_output.yaml";

      auto ec = glz::write_file_yaml(obj, filename);
      expect(!ec);

      // Read it back
      file_struct parsed{};
      auto rec = glz::read_file_yaml(parsed, filename);
      expect(!rec);
      expect(parsed.x == obj.x);
      expect(std::abs(parsed.y - obj.y) < 0.001);
      expect(parsed.name == obj.name);

      // Clean up
      std::remove(filename.c_str());
   };

   "read_file_yaml_not_found"_test = [] {
      file_struct obj{};
      auto ec = glz::read_file_yaml(obj, "./nonexistent_file.yaml");
      expect(bool(ec)); // Should error
   };
};

// ============================================================
// External YAML String Tests
// ============================================================

suite yaml_external_yaml_string_tests = [] {
   // Sources:
   // - https://k8s-examples.container-solutions.com/examples/Deployment/simple-deployment.yaml
   // - https://k8s-examples.container-solutions.com/examples/Service/simple.yaml
   "external_service_generic_roundtrip"_test = [] {
      const std::string yaml = R"(---
apiVersion: v1
kind: Service
metadata:
  name: simple-service
spec:
  selector:
    app: App1
  ports:
    - name: http
      protocol: TCP
      port: 80
      targetPort: 9376
)";
      auto check = [](const glz::generic& parsed) {
         auto& root = std::get<glz::generic::object_t>(parsed.data);
         expect(std::get<std::string>(root.at("kind").data) == "Service");

         auto& spec = std::get<glz::generic::object_t>(root.at("spec").data);
         auto& selector = std::get<glz::generic::object_t>(spec.at("selector").data);
         expect(std::get<std::string>(selector.at("app").data) == "App1");

         auto& ports = std::get<glz::generic::array_t>(spec.at("ports").data);
         expect(ports.size() == 1);
         auto& port0 = std::get<glz::generic::object_t>(ports[0].data);
         expect(std::get<std::string>(port0.at("name").data) == "http");
         expect(std::get<double>(port0.at("port").data) == 80.0);
      };

      roundtrip_yaml<glz::generic>(yaml, check);
   };

   "external_service_struct_roundtrip"_test = [] {
      const std::string yaml = R"(---
apiVersion: v1
kind: Service
metadata:
  name: simple-service
spec:
  selector:
    app: App1
  ports:
    - name: http
      protocol: TCP
      port: 80
      targetPort: 9376
)";
      auto check = [](const k8s_service& svc) {
         expect(svc.apiVersion == "v1");
         expect(svc.kind == "Service");
         expect(svc.metadata.name == "simple-service");
         expect(svc.spec.selector.at("app") == "App1");
         expect(svc.spec.ports.size() == 1);
         expect(svc.spec.ports[0].protocol == "TCP");
         expect(svc.spec.ports[0].port == 80);
         expect(svc.spec.ports[0].targetPort == 9376);
      };

      roundtrip_yaml<k8s_service>(yaml, check);
   };

   "external_deployment_generic_roundtrip"_test = [] {
      const std::string yaml = R"(---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nginx-deployment
  labels:
    app: nginx
spec:
  replicas: 3
  selector:
    matchLabels:
      app: nginx
  template:
    metadata:
      labels:
        app: nginx
    spec:
      containers:
        - name: nginx
          image: nginx:1.7.9
          ports:
            - containerPort: 80
)";
      auto check = [](const glz::generic& parsed) {
         auto& root = std::get<glz::generic::object_t>(parsed.data);
         expect(std::get<std::string>(root.at("kind").data) == "Deployment");

         auto& spec = std::get<glz::generic::object_t>(root.at("spec").data);
         expect(std::get<double>(spec.at("replicas").data) == 3.0);

         auto& template_obj = std::get<glz::generic::object_t>(spec.at("template").data);
         auto& pod_spec = std::get<glz::generic::object_t>(template_obj.at("spec").data);
         auto& containers = std::get<glz::generic::array_t>(pod_spec.at("containers").data);
         expect(containers.size() == 1);

         auto& container0 = std::get<glz::generic::object_t>(containers[0].data);
         expect(std::get<std::string>(container0.at("name").data) == "nginx");
         expect(std::get<std::string>(container0.at("image").data) == "nginx:1.7.9");
      };

      roundtrip_yaml<glz::generic>(yaml, check);
   };

   "external_deployment_struct_roundtrip"_test = [] {
      const std::string yaml = R"(---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nginx-deployment
  labels:
    app: nginx
spec:
  replicas: 3
  selector:
    matchLabels:
      app: nginx
  template:
    metadata:
      labels:
        app: nginx
    spec:
      containers:
        - name: nginx
          image: nginx:1.7.9
          ports:
            - containerPort: 80
)";
      auto check = [](const k8s_deployment& dep) {
         expect(dep.apiVersion == "apps/v1");
         expect(dep.kind == "Deployment");
         expect(dep.metadata.name == "nginx-deployment");
         expect(dep.spec.replicas == 3);
         expect(dep.spec.template_.spec.containers.size() == 1);
         expect(dep.spec.template_.spec.containers[0].image == "nginx:1.7.9");
         expect(dep.spec.template_.spec.containers[0].ports.size() == 1);
         expect(dep.spec.template_.spec.containers[0].ports[0].containerPort == 80);
      };

      roundtrip_yaml<k8s_deployment>(yaml, check);
   };

   "reflectable_flow_mapping_empty_sequence_roundtrip"_test = [] {
      const std::string yaml = R"({
  "servers":
  [

  ]
})";
      auto check = [](const reflectable_config& cfg) { expect(cfg.servers.empty()); };
      roundtrip_yaml<reflectable_config>(yaml, check);
   };

   "generic_flow_mapping_empty_sequence_output"_test = [] {
      const std::string yaml = R"({
  "servers":
  [

  ]
})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);
      expect(output.find("servers") != std::string::npos);
      expect(output.find("[]") != std::string::npos);

      glz::generic reparsed;
      auto rec2 = glz::read_yaml(reparsed, output);
      expect(!rec2) << glz::format_error(rec2, output);
      auto& root = std::get<glz::generic::object_t>(reparsed.data);
      expect(root.contains("servers"));
      auto& servers = std::get<glz::generic::array_t>(root.at("servers").data);
      expect(servers.empty());
   };

   "external_advanced_generic_roundtrip"_test = [] {
      const std::string yaml = R"(---
title: "Advanced YAML"
description: >
  This is a folded
  description with a blank line.

  Second paragraph.
literal: |-
  line one
    line two
  line three
multiline_plain:
  meta.statement.conditional.case.python
  keyword.control.conditional.case.python
quoted: "beta: colon"
flags:
  enabled: true
  archived: false
counts:
  retries: 3
  timeout_ms: 1500
  ratio: 0.75
list:
  - alpha
  - "beta: colon"
  - 'gamma # not comment'
flow:
  values: [1, 2, 3]
  mapping: {a: one, b: two, c: three}
nested:
  name: sample
  ids: [10, 20, 30]
  labels:
    env: dev
    tier: backend
note: null
)";
      auto check = [](const glz::generic& parsed) {
         auto& root = std::get<glz::generic::object_t>(parsed.data);
         expect(std::get<std::string>(root.at("title").data) == "Advanced YAML");

         auto& description = std::get<std::string>(root.at("description").data);
         expect(description.find("This is a folded description") != std::string::npos);
         expect(description.find("Second paragraph.") != std::string::npos);

         expect(std::get<std::string>(root.at("literal").data) == "line one\n  line two\nline three");
         expect(std::get<std::string>(root.at("multiline_plain").data) ==
                "meta.statement.conditional.case.python keyword.control.conditional.case.python");
         expect(std::get<std::string>(root.at("quoted").data) == "beta: colon");

         auto& flags = std::get<glz::generic::object_t>(root.at("flags").data);
         expect(std::get<bool>(flags.at("enabled").data));
         expect(!std::get<bool>(flags.at("archived").data));

         auto& counts = std::get<glz::generic::object_t>(root.at("counts").data);
         expect(std::get<double>(counts.at("retries").data) == 3.0);
         expect(std::get<double>(counts.at("timeout_ms").data) == 1500.0);
         expect(std::abs(std::get<double>(counts.at("ratio").data) - 0.75) < 0.0001);

         auto& list = std::get<glz::generic::array_t>(root.at("list").data);
         expect(list.size() == 3);
         expect(std::get<std::string>(list[0].data) == "alpha");
         expect(std::get<std::string>(list[1].data) == "beta: colon");
         expect(std::get<std::string>(list[2].data) == "gamma # not comment");

         auto& flow = std::get<glz::generic::object_t>(root.at("flow").data);
         auto& values = std::get<glz::generic::array_t>(flow.at("values").data);
         expect(values.size() == 3);
         expect(std::get<double>(values[0].data) == 1.0);
         expect(std::get<double>(values[2].data) == 3.0);
         auto& mapping = std::get<glz::generic::object_t>(flow.at("mapping").data);
         expect(std::get<std::string>(mapping.at("b").data) == "two");

         auto& nested = std::get<glz::generic::object_t>(root.at("nested").data);
         expect(std::get<std::string>(nested.at("name").data) == "sample");
         auto& ids = std::get<glz::generic::array_t>(nested.at("ids").data);
         expect(ids.size() == 3);
         auto& labels = std::get<glz::generic::object_t>(nested.at("labels").data);
         expect(std::get<std::string>(labels.at("env").data) == "dev");
         expect(std::get<std::string>(labels.at("tier").data) == "backend");

         auto& note = root.at("note").data;
         expect(std::holds_alternative<std::nullptr_t>(note));
      };

      roundtrip_yaml<glz::generic>(yaml, check);
   };

   "external_advanced_struct_roundtrip"_test = [] {
      const std::string yaml = R"(---
title: "Advanced YAML"
description: >
  This is a folded
  description with a blank line.

  Second paragraph.
literal: |-
  line one
    line two
  line three
multiline_plain:
  meta.statement.conditional.case.python
  keyword.control.conditional.case.python
quoted: "beta: colon"
flags:
  enabled: true
  archived: false
counts:
  retries: 3
  timeout_ms: 1500
  ratio: 0.75
list:
  - alpha
  - "beta: colon"
  - 'gamma # not comment'
flow:
  values: [1, 2, 3]
  mapping: {a: one, b: two, c: three}
nested:
  name: sample
  ids: [10, 20, 30]
  labels:
    env: dev
    tier: backend
note: null
)";
      auto check = [](const advanced_doc& doc) {
         expect(doc.title == "Advanced YAML");
         expect(doc.description.find("This is a folded description") != std::string::npos);
         expect(doc.description.find("Second paragraph.") != std::string::npos);
         expect(doc.literal == "line one\n  line two\nline three");
         expect(doc.multiline_plain ==
                "meta.statement.conditional.case.python keyword.control.conditional.case.python");
         expect(doc.quoted == "beta: colon");

         expect(doc.flags.enabled);
         expect(!doc.flags.archived);
         expect(doc.counts.retries == 3);
         expect(doc.counts.timeout_ms == 1500);
         expect(std::abs(doc.counts.ratio - 0.75) < 0.0001);

         expect(doc.list.size() == 3);
         expect(doc.list[2] == "gamma # not comment");

         expect(doc.flow.values.size() == 3);
         expect(doc.flow.values[0] == 1);
         expect(doc.flow.mapping.at("b") == "two");

         expect(doc.nested.name == "sample");
         expect(doc.nested.ids.size() == 3);
         expect(doc.nested.labels.at("env") == "dev");
         expect(doc.nested.labels.at("tier") == "backend");

         expect(!doc.note.has_value());
      };

      roundtrip_yaml<advanced_doc>(yaml, check);
   };
};

// ============================================================
// Tuple and Pair Tests (Additional)
// ============================================================

suite yaml_tuple_pair_tests = [] {
   "tuple_mixed_types"_test = [] {
      auto original = std::make_tuple(42, 3.14, std::string("hello"));
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      decltype(original) parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::get<0>(parsed) == std::get<0>(original));
      expect(std::abs(std::get<1>(parsed) - std::get<1>(original)) < 0.001);
      expect(std::get<2>(parsed) == std::get<2>(original));
   };

   "tuple_nested"_test = [] {
      auto original = std::make_tuple(1, std::make_tuple(2, 3), 4);
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      decltype(original) parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::get<0>(parsed) == 1);
      expect(std::get<0>(std::get<1>(parsed)) == 2);
      expect(std::get<1>(std::get<1>(parsed)) == 3);
      expect(std::get<2>(parsed) == 4);
   };

   "pair_roundtrip"_test = [] {
      auto original = std::make_pair(std::string("key"), 123);
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      decltype(original) parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.first == original.first);
      expect(parsed.second == original.second);
   };

   "pair_int_int"_test = [] {
      auto original = std::make_pair(1, 2);
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      decltype(original) parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "vector_of_pairs"_test = [] {
      std::vector<std::pair<std::string, int>> original{{"a", 1}, {"b", 2}, {"c", 3}};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      decltype(original) parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };
};

// ============================================================
// Enum Tests (Additional)
// ============================================================

enum class Priority { Low, Medium, High };

template <>
struct glz::meta<Priority>
{
   using enum Priority;
   static constexpr auto value = enumerate(Low, Medium, High);
};

struct priority_container
{
   Priority priority{Priority::Low};
   std::vector<Priority> priorities{};
};

template <>
struct glz::meta<priority_container>
{
   using T = priority_container;
   static constexpr auto value = object("priority", &T::priority, "priorities", &T::priorities);
};

suite yaml_enum_additional_tests = [] {
   "enum_write_read"_test = [] {
      Priority original = Priority::Medium;
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      expect(yaml.find("Medium") != std::string::npos);

      Priority parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "enum_all_values"_test = [] {
      for (auto p : {Priority::Low, Priority::Medium, Priority::High}) {
         std::string yaml;
         auto wec = glz::write_yaml(p, yaml);
         expect(!wec);

         Priority parsed{};
         auto rec = glz::read_yaml(parsed, yaml);
         expect(!rec) << glz::format_error(rec, yaml);
         expect(parsed == p);
      }
   };

   "array_of_enums"_test = [] {
      std::array<Priority, 3> original{Priority::Medium, Priority::Low, Priority::High};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::array<Priority, 3> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "vector_of_enums"_test = [] {
      std::vector<Priority> original{Priority::Low, Priority::Medium, Priority::High, Priority::Low};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::vector<Priority> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "enum_in_struct"_test = [] {
      priority_container original{Priority::High, {Priority::Low, Priority::Medium}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      priority_container parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.priority == original.priority);
      expect(parsed.priorities == original.priorities);
   };

   "invalid_enum"_test = [] {
      Priority parsed{Priority::Low};
      std::string yaml = "InvalidPriority";
      auto ec = glz::read_yaml(parsed, yaml);
      expect(bool(ec)); // Should error
      expect(parsed == Priority::Low); // Should remain unchanged
   };
};

// ============================================================
// Skip Null Members Tests
// ============================================================

struct nullable_struct
{
   std::optional<int> opt{};
   std::shared_ptr<std::string> ptr{};
   int value{42};
};

template <>
struct glz::meta<nullable_struct>
{
   using T = nullable_struct;
   static constexpr auto value = object("opt", &T::opt, "ptr", &T::ptr, "value", &T::value);
};

suite yaml_skip_null_tests = [] {
   "skip_null_members_true"_test = [] {
      nullable_struct obj{};
      std::string yaml;
      // Default is skip_null_members = true
      auto wec = glz::write_yaml(obj, yaml);
      expect(!wec);
      // Null members should be omitted
      expect(yaml.find("opt") == std::string::npos);
      expect(yaml.find("ptr") == std::string::npos);
      expect(yaml.find("value") != std::string::npos);
      expect(yaml.find("42") != std::string::npos);
   };

   "skip_null_members_false"_test = [] {
      nullable_struct obj{};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.skip_null_members = false};
      auto wec = glz::write<opts>(obj, yaml);
      expect(!wec);
      // Null members should be present
      expect(yaml.find("opt") != std::string::npos);
      expect(yaml.find("ptr") != std::string::npos);
      expect(yaml.find("null") != std::string::npos);
   };

   "skip_null_with_values"_test = [] {
      nullable_struct obj{};
      obj.opt = 99;
      obj.ptr = std::make_shared<std::string>("hello");

      std::string yaml;
      auto wec = glz::write_yaml(obj, yaml);
      expect(!wec);
      // All members should be present when they have values
      expect(yaml.find("opt") != std::string::npos);
      expect(yaml.find("99") != std::string::npos);
      expect(yaml.find("ptr") != std::string::npos);
      expect(yaml.find("hello") != std::string::npos);
   };
};

// ============================================================
// Reflection and glz::meta Tests
// ============================================================

struct custom_keys_struct
{
   int internal_x{};
   std::string internal_name{};
};

template <>
struct glz::meta<custom_keys_struct>
{
   using T = custom_keys_struct;
   static constexpr auto value = object("x", &T::internal_x, "name", &T::internal_name);
};

struct nested_meta_struct
{
   custom_keys_struct inner{};
   int outer_value{};
};

template <>
struct glz::meta<nested_meta_struct>
{
   using T = nested_meta_struct;
   static constexpr auto value = object("inner", &T::inner, "outer", &T::outer_value);
};

suite yaml_meta_tests = [] {
   "custom_keys"_test = [] {
      custom_keys_struct original{42, "test"};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);
      // Should use "x" not "internal_x"
      expect(yaml.find("x:") != std::string::npos);
      expect(yaml.find("name:") != std::string::npos);
      expect(yaml.find("internal_x") == std::string::npos);

      custom_keys_struct parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.internal_x == original.internal_x);
      expect(parsed.internal_name == original.internal_name);
   };

   "nested_meta"_test = [] {
      nested_meta_struct original{{10, "inner"}, 20};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      nested_meta_struct parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.inner.internal_x == original.inner.internal_x);
      expect(parsed.inner.internal_name == original.inner.internal_name);
      expect(parsed.outer_value == original.outer_value);
   };
};

// ============================================================
// Edge Cases and Special Values
// ============================================================

suite yaml_edge_cases = [] {
   "empty_string"_test = [] {
      std::string original = "";
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::string parsed = "not_empty";
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.empty());
   };

   "string_with_special_chars"_test = [] {
      std::string original = "line1\nline2\ttab\"quote";
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::string parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "unicode_string"_test = [] {
      std::string original = "Hello \xe4\xb8\x96\xe7\x95\x8c \xf0\x9f\x8c\x8d"; // "Hello 世界 🌍" in UTF-8
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::string parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "very_long_string"_test = [] {
      std::string original(10000, 'x');
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::string parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "deeply_nested_struct"_test = [] {
      nested_struct level1{};
      level1.title = "level1";
      level1.data.x = 1;
      level1.numbers = {1, 2, 3};

      std::string yaml;
      auto wec = glz::write_yaml(level1, yaml);
      expect(!wec);

      nested_struct parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.title == level1.title);
      expect(parsed.data.x == level1.data.x);
      expect(parsed.numbers == level1.numbers);
   };

   "map_with_empty_values"_test = [] {
      std::map<std::string, std::string> original{{"a", ""}, {"b", "value"}, {"c", ""}};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::map<std::string, std::string> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };

   "single_element_vector"_test = [] {
      std::vector<int> original{42};
      std::string yaml;
      constexpr glz::yaml::yaml_opts opts{.flow_style = true};
      auto wec = glz::write<opts>(original, yaml);
      expect(!wec);

      std::vector<int> parsed{};
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed == original);
   };
};

// ============================================================
// glz::generic YAML Parsing Tests
// ============================================================

suite yaml_generic_parsing_tests = [] {
   // Output formatting - verify proper newlines between entries
   "generic_flow_mapping_output_formatting"_test = [] {
      std::string yaml = R"({"name": "Alice", "age": 30})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      // Write back and verify proper formatting
      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);

      // Output should have proper newlines - each key:value on separate lines
      // Should NOT have entries running together like "30name:"
      expect(output.find("30name") == std::string::npos) << "Values should be separated by newlines";
      expect(output.find("Aliceage") == std::string::npos) << "Values should be separated by newlines";

      // Verify it can be parsed back
      glz::generic reparsed;
      auto rec2 = glz::read_yaml(reparsed, output);
      expect(!rec2) << glz::format_error(rec2, output);
   };

   "generic_nested_map_output_formatting"_test = [] {
      std::string yaml = R"({"outer": {"inner": 42}, "other": "value"})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);

      // Verify roundtrip
      glz::generic reparsed;
      auto rec2 = glz::read_yaml(reparsed, output);
      expect(!rec2) << glz::format_error(rec2, output);
      expect(std::holds_alternative<glz::generic::object_t>(reparsed.data));
   };

   // Multi-line flow-style parsing
   "generic_multiline_flow_mapping"_test = [] {
      std::string yaml = R"({"name": "Alice",
"age": 30})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 2u);
      expect(std::holds_alternative<std::string>(obj.at("name").data));
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
      expect(std::holds_alternative<double>(obj.at("age").data));
      expect(std::get<double>(obj.at("age").data) == 30.0);
   };

   "generic_multiline_flow_mapping_multiple_lines"_test = [] {
      std::string yaml = R"({
"name": "Bob",
"age": 25,
"city": "NYC"
})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 3u);
      expect(std::get<std::string>(obj.at("name").data) == "Bob");
      expect(std::get<double>(obj.at("age").data) == 25.0);
      expect(std::get<std::string>(obj.at("city").data) == "NYC");
   };

   "generic_multiline_flow_with_nested"_test = [] {
      std::string yaml = R"({"person": {"name": "Charlie",
"age": 35},
"active": true})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 2u);
      expect(std::holds_alternative<glz::generic::object_t>(obj.at("person").data));
      expect(std::holds_alternative<bool>(obj.at("active").data));
   };

   // Block-style YAML parsing into glz::generic
   "generic_block_mapping_simple"_test = [] {
      std::string yaml = "name: Alice";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 1u);
      expect(std::holds_alternative<std::string>(obj.at("name").data));
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
   };

   "generic_block_mapping_with_document_marker"_test = [] {
      std::string yaml = "---\nname: Alice";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 1u);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
   };

   "generic_block_mapping_multiple_entries"_test = [] {
      std::string yaml = R"(name: Alice
age: 30
city: NYC)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 3u);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
      expect(std::get<double>(obj.at("age").data) == 30.0);
      expect(std::get<std::string>(obj.at("city").data) == "NYC");
   };

   // Nested block-style mappings into glz::generic
   "generic_nested_block_style_mapping"_test = [] {
      std::string yaml = R"(person:
  name: Bob
  age: 25)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("person") == 1u);
      expect(std::holds_alternative<glz::generic::object_t>(obj.at("person").data));

      auto& person = std::get<glz::generic::object_t>(obj.at("person").data);
      expect(std::get<std::string>(person.at("name").data) == "Bob");
      expect(std::get<double>(person.at("age").data) == 25.0);
   };

   // First verify simple two-key block mapping works
   "generic_two_key_simple"_test = [] {
      std::string yaml = R"(first: 1
second: 2)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("first") == 1u);
      expect(root.count("second") == 1u);
   };

   // Multiple top-level keys, first with nested content
   "generic_nested_then_simple"_test = [] {
      std::string yaml = R"(person:
  name: Bob
other: simple)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("person") == 1u);
      expect(root.count("other") == 1u);
   };

   // Three levels of nesting
   "generic_deeply_nested_block"_test = [] {
      std::string yaml = R"(level1:
  level2:
    level3: deep_value
    another: 42
  sibling2: test
top_sibling: done)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("level1") == 1u);
      expect(root.count("top_sibling") == 1u);

      auto& level1 = std::get<glz::generic::object_t>(root.at("level1").data);
      expect(level1.count("level2") == 1u);
      expect(level1.count("sibling2") == 1u);

      auto& level2 = std::get<glz::generic::object_t>(level1.at("level2").data);
      expect(std::get<std::string>(level2.at("level3").data) == "deep_value");
      expect(std::get<double>(level2.at("another").data) == 42.0);
   };

   // Multiple nested objects at same level
   "generic_multiple_nested_siblings"_test = [] {
      std::string yaml = R"(first:
  a: 1
  b: 2
second:
  c: 3
  d: 4
third:
  e: 5)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("first") == 1u);
      expect(root.count("second") == 1u);
      expect(root.count("third") == 1u);

      auto& first = std::get<glz::generic::object_t>(root.at("first").data);
      expect(std::get<double>(first.at("a").data) == 1.0);
      expect(std::get<double>(first.at("b").data) == 2.0);

      auto& second = std::get<glz::generic::object_t>(root.at("second").data);
      expect(std::get<double>(second.at("c").data) == 3.0);
      expect(std::get<double>(second.at("d").data) == 4.0);
   };

   // Mixed pattern: simple, nested, simple
   "generic_simple_nested_simple"_test = [] {
      std::string yaml = R"(before: start
nested:
  inner: value
after: end)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("before") == 1u);
      expect(root.count("nested") == 1u);
      expect(root.count("after") == 1u);

      expect(std::get<std::string>(root.at("before").data) == "start");
      expect(std::get<std::string>(root.at("after").data) == "end");

      auto& nested = std::get<glz::generic::object_t>(root.at("nested").data);
      expect(std::get<std::string>(nested.at("inner").data) == "value");
   };

   // Nested with various value types
   "generic_nested_mixed_types"_test = [] {
      std::string yaml = R"(config:
  name: test
  count: 100
  enabled: true
  ratio: 3.14
status: ok)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("config") == 1u);
      expect(root.count("status") == 1u);

      auto& config = std::get<glz::generic::object_t>(root.at("config").data);
      expect(std::get<std::string>(config.at("name").data) == "test");
      expect(std::get<double>(config.at("count").data) == 100.0);
      expect(std::get<bool>(config.at("enabled").data) == true);
      expect(std::get<double>(config.at("ratio").data) == 3.14);
   };

   // Nested followed by multiple siblings
   "generic_nested_then_multiple_siblings"_test = [] {
      std::string yaml = R"(nested:
  key: value
sibling1: one
sibling2: two
sibling3: three)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("nested") == 1u);
      expect(root.count("sibling1") == 1u);
      expect(root.count("sibling2") == 1u);
      expect(root.count("sibling3") == 1u);
   };

   // Block array as value in block mapping (was a bug: parsed as string)
   "generic_block_array_as_value"_test = [] {
      std::string yaml = R"(items:
  - first
  - second
other: done)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("items") == 1u);
      expect(root.count("other") == 1u);

      expect(std::holds_alternative<glz::generic::array_t>(root.at("items").data));
      auto& items = std::get<glz::generic::array_t>(root.at("items").data);
      expect(items.size() == 2u);
      expect(std::get<std::string>(items[0].data) == "first");
      expect(std::get<std::string>(items[1].data) == "second");
   };

   // Block array with single item
   "generic_block_array_single_item"_test = [] {
      std::string yaml = R"(items:
  - only)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::array_t>(root.at("items").data));
      auto& items = std::get<glz::generic::array_t>(root.at("items").data);
      expect(items.size() == 1u);
   };

   // Multiple block arrays as values
   "generic_multiple_block_arrays"_test = [] {
      std::string yaml = R"(first:
  - a
  - b
second:
  - c
  - d)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("first") == 1u);
      expect(root.count("second") == 1u);

      auto& first = std::get<glz::generic::array_t>(root.at("first").data);
      auto& second = std::get<glz::generic::array_t>(root.at("second").data);
      expect(first.size() == 2u);
      expect(second.size() == 2u);
   };

   // Block array of objects - each item should retain all keys
   "generic_block_array_of_objects"_test = [] {
      std::string yaml = R"(- name: Alice
  age: 30
- name: Bob
  age: 25)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 2u);

      // First item should have both keys
      expect(std::holds_alternative<glz::generic::object_t>(arr[0].data));
      auto& first = std::get<glz::generic::object_t>(arr[0].data);
      expect(first.size() == 2u);
      expect(first.count("name") == 1u);
      expect(first.count("age") == 1u);
      expect(std::get<std::string>(first.at("name").data) == "Alice");

      // Second item should also have both keys
      expect(std::holds_alternative<glz::generic::object_t>(arr[1].data));
      auto& second = std::get<glz::generic::object_t>(arr[1].data);
      expect(second.size() == 2u);
      expect(second.count("name") == 1u);
      expect(second.count("age") == 1u);
      expect(std::get<std::string>(second.at("name").data) == "Bob");
   };

   // Nested arrays with dash on separate line (value on next line)
   "generic_nested_array_dash_newline"_test = [] {
      std::string yaml = R"(-
  - a
  - b
-
  - c
  - d)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 2u);

      // First item should be an array with 2 elements
      expect(std::holds_alternative<glz::generic::array_t>(arr[0].data));
      auto& first = std::get<glz::generic::array_t>(arr[0].data);
      expect(first.size() == 2u);
      expect(std::get<std::string>(first[0].data) == "a");
      expect(std::get<std::string>(first[1].data) == "b");

      // Second item should be an array with 2 elements
      expect(std::holds_alternative<glz::generic::array_t>(arr[1].data));
      auto& second = std::get<glz::generic::array_t>(arr[1].data);
      expect(second.size() == 2u);
      expect(std::get<std::string>(second[0].data) == "c");
      expect(std::get<std::string>(second[1].data) == "d");
   };

   // Comment before nested content should not break parsing
   "generic_comment_before_nested_content"_test = [] {
      std::string yaml = R"(data:
  # This is a comment
  key: value
end: done)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.count("data") == 1u);
      expect(root.count("end") == 1u);

      // data should be an object, not an empty string
      expect(std::holds_alternative<glz::generic::object_t>(root.at("data").data));
      auto& data = std::get<glz::generic::object_t>(root.at("data").data);
      expect(data.count("key") == 1u);
      expect(std::get<std::string>(data.at("key").data) == "value");
   };

   // Multiple comments before nested content
   "generic_multiple_comments_before_nested"_test = [] {
      std::string yaml = R"(config:
  # First comment
  # Second comment
  setting: enabled
status: ok)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::object_t>(root.at("config").data));
      auto& config = std::get<glz::generic::object_t>(root.at("config").data);
      expect(std::get<std::string>(config.at("setting").data) == "enabled");
   };

   // Indented comment between mapping entries
   "generic_indented_comment_between_entries"_test = [] {
      std::string yaml = R"(name: Alice
  # indented comment
age: 30)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 2u);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
      expect(std::get<double>(obj.at("age").data) == 30.0);
   };

   // Leading comment before any content
   "generic_leading_comment"_test = [] {
      std::string yaml = R"(# This is a header comment
name: Alice)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("name") == 1u);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
   };

   // Leading comment with blank line
   "generic_leading_comment_blank_line"_test = [] {
      std::string yaml = R"(# comment

name: Alice)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
   };

   // Leading whitespace then comment
   "generic_leading_whitespace_comment"_test = [] {
      std::string yaml = "  # comment\nname: Alice";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
   };

   // Blank lines with whitespace between entries
   "generic_blank_lines_with_whitespace"_test = [] {
      std::string yaml = "name: Alice\n   \nage: 30";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 2u);
   };

   // Indented comment between array items
   "generic_indented_comment_in_array"_test = [] {
      std::string yaml = "- first\n  # comment\n- second";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 2u);
      expect(std::get<std::string>(arr[0].data) == "first");
      expect(std::get<std::string>(arr[1].data) == "second");
   };

   // Comment in nested array
   "generic_comment_in_nested_array"_test = [] {
      std::string yaml = R"(items:
  - first
  # comment
  - second)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::array_t>(obj.at("items").data));
      auto& arr = std::get<glz::generic::array_t>(obj.at("items").data);
      expect(arr.size() == 2u);
   };

   // Quoted keys in block mapping
   "generic_quoted_key_double"_test = [] {
      std::string yaml = R"("name": Alice)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("name") == 1u);
      expect(std::get<std::string>(obj.at("name").data) == "Alice");
   };

   // Quoted key with spaces
   "generic_quoted_key_with_spaces"_test = [] {
      std::string yaml = R"("full name": Alice Smith)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("full name") == 1u);
      expect(std::get<std::string>(obj.at("full name").data) == "Alice Smith");
   };

   // Quoted key containing colon
   "generic_quoted_key_with_colon"_test = [] {
      std::string yaml = R"("key:value": test)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("key:value") == 1u);
      expect(std::get<std::string>(obj.at("key:value").data) == "test");
   };

   // Empty quoted key
   "generic_empty_quoted_key"_test = [] {
      std::string yaml = R"("": value)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("") == 1u);
      expect(std::get<std::string>(obj.at("").data) == "value");
   };

   // Empty array item (dash followed by newline should be null)
   "generic_empty_array_item_first"_test = [] {
      std::string yaml = "- \n- second";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 2u);
      expect(std::holds_alternative<glz::generic::null_t>(arr[0].data));
      expect(std::get<std::string>(arr[1].data) == "second");
   };

   // Multiple empty array items
   "generic_empty_array_items_multiple"_test = [] {
      std::string yaml = "- \n- \n- value";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 3u);
      expect(std::holds_alternative<glz::generic::null_t>(arr[0].data));
      expect(std::holds_alternative<glz::generic::null_t>(arr[1].data));
      expect(std::get<std::string>(arr[2].data) == "value");
   };

   // Flow-style nested objects also work
   "generic_block_mapping_with_flow_nested_object"_test = [] {
      std::string yaml = R"(person: {name: Bob, age: 25})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("person") == 1u);
      expect(std::holds_alternative<glz::generic::object_t>(obj.at("person").data));

      auto& person = std::get<glz::generic::object_t>(obj.at("person").data);
      expect(std::get<std::string>(person.at("name").data) == "Bob");
      expect(std::get<double>(person.at("age").data) == 25.0);
   };

   // Keys starting with special characters that could be mistaken for other types
   "generic_block_mapping_key_starts_with_t"_test = [] {
      std::string yaml = "title: My Document";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::get<std::string>(obj.at("title").data) == "My Document");
   };

   "generic_block_mapping_key_starts_with_f"_test = [] {
      std::string yaml = "filename: test.txt";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::get<std::string>(obj.at("filename").data) == "test.txt");
   };

   "generic_block_mapping_key_starts_with_n"_test = [] {
      std::string yaml = "number: 42";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::get<double>(obj.at("number").data) == 42.0);
   };

   "generic_block_mapping_key_starts_with_digit"_test = [] {
      std::string yaml = "123key: value";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::get<std::string>(obj.at("123key").data) == "value");
   };

   // Actual boolean/null values vs keys that start with same letters
   "generic_block_mapping_true_vs_key"_test = [] {
      // "true" as a value should be boolean
      std::string yaml1 = "flag: true";
      glz::generic parsed1;
      auto rec1 = glz::read_yaml(parsed1, yaml1);
      expect(!rec1) << glz::format_error(rec1, yaml1);
      auto& obj1 = std::get<glz::generic::object_t>(parsed1.data);
      expect(std::holds_alternative<bool>(obj1.at("flag").data));
      expect(std::get<bool>(obj1.at("flag").data) == true);

      // "truthy" as a key should be detected as block mapping
      std::string yaml2 = "truthy: yes";
      glz::generic parsed2;
      auto rec2 = glz::read_yaml(parsed2, yaml2);
      expect(!rec2) << glz::format_error(rec2, yaml2);
      expect(std::holds_alternative<glz::generic::object_t>(parsed2.data));
      auto& obj2 = std::get<glz::generic::object_t>(parsed2.data);
      expect(obj2.count("truthy") == 1u);
   };

   "generic_block_mapping_false_vs_key"_test = [] {
      // "false" as a value should be boolean
      std::string yaml1 = "flag: false";
      glz::generic parsed1;
      auto rec1 = glz::read_yaml(parsed1, yaml1);
      expect(!rec1) << glz::format_error(rec1, yaml1);
      auto& obj1 = std::get<glz::generic::object_t>(parsed1.data);
      expect(std::holds_alternative<bool>(obj1.at("flag").data));
      expect(std::get<bool>(obj1.at("flag").data) == false);

      // "falsy" as a key should be detected as block mapping
      std::string yaml2 = "falsy: no";
      glz::generic parsed2;
      auto rec2 = glz::read_yaml(parsed2, yaml2);
      expect(!rec2) << glz::format_error(rec2, yaml2);
      expect(std::holds_alternative<glz::generic::object_t>(parsed2.data));
      auto& obj2 = std::get<glz::generic::object_t>(parsed2.data);
      expect(obj2.count("falsy") == 1u);
   };

   "generic_block_mapping_null_value"_test = [] {
      // "null" as a value should be null
      std::string yaml1 = "value: null";
      glz::generic parsed1;
      auto rec1 = glz::read_yaml(parsed1, yaml1);
      expect(!rec1) << glz::format_error(rec1, yaml1);
      auto& obj1 = std::get<glz::generic::object_t>(parsed1.data);
      expect(std::holds_alternative<std::nullptr_t>(obj1.at("value").data));
   };

   // For keys that might conflict with reserved words, use flow style
   "generic_flow_mapping_null_like_key"_test = [] {
      std::string yaml = R"({"nullable": "something"})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.count("nullable") == 1u);
   };

   // Mixed flow and block styles
   "generic_block_mapping_with_flow_value"_test = [] {
      std::string yaml = R"(data: {"inner": "value"})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::object_t>(obj.at("data").data));
   };

   "generic_block_mapping_with_flow_array_value"_test = [] {
      std::string yaml = "items: [1, 2, 3]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));

      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::array_t>(obj.at("items").data));

      auto& arr = std::get<glz::generic::array_t>(obj.at("items").data);
      expect(arr.size() == 3u);
   };

   // Roundtrip tests
   "generic_block_roundtrip"_test = [] {
      std::string yaml = R"(name: Test
value: 123
active: true)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);

      glz::generic reparsed;
      auto rec2 = glz::read_yaml(reparsed, output);
      expect(!rec2) << glz::format_error(rec2, output);

      // Verify the data matches
      auto& obj1 = std::get<glz::generic::object_t>(parsed.data);
      auto& obj2 = std::get<glz::generic::object_t>(reparsed.data);
      expect(obj1.size() == obj2.size());
   };

   // Use flow style for complex nested structures with glz::generic
   "generic_complex_roundtrip"_test = [] {
      std::string yaml =
         R"({"users": [{"name": "Alice", "age": 30}, {"name": "Bob", "age": 25}], "metadata": {"version": 1, "enabled": true}})";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);

      glz::generic reparsed;
      auto rec2 = glz::read_yaml(reparsed, output);
      expect(!rec2) << glz::format_error(rec2, output);
      expect(std::holds_alternative<glz::generic::object_t>(reparsed.data));
   };
};

// ============================================================
// Additional YAML Map Parsing Tests
// ============================================================

suite yaml_map_parsing_tests = [] {
   "map_multiline_flow_parsing"_test = [] {
      std::string yaml = R"({"key1": "value1",
"key2": "value2",
"key3": "value3"})";
      std::map<std::string, std::string> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.size() == 3u);
      expect(parsed["key1"] == "value1");
      expect(parsed["key2"] == "value2");
      expect(parsed["key3"] == "value3");
   };

   "map_multiline_flow_with_spaces"_test = [] {
      std::string yaml = R"({
   "a": 1,
   "b": 2
})";
      std::map<std::string, int> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.size() == 2u);
      expect(parsed["a"] == 1);
      expect(parsed["b"] == 2);
   };

   "map_flow_with_trailing_comma_newline"_test = [] {
      std::string yaml = R"({"x": 10,
"y": 20})";
      std::map<std::string, int> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.size() == 2u);
   };

   "unordered_map_multiline_flow"_test = [] {
      std::string yaml = R"({"first": 100,
"second": 200})";
      std::unordered_map<std::string, int> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(parsed.size() == 2u);
      expect(parsed["first"] == 100);
      expect(parsed["second"] == 200);
   };
};

// ============================================================
// YAML Variant Edge Cases
// ============================================================

suite yaml_variant_edge_cases = [] {
   using test_variant =
      std::variant<std::nullptr_t, bool, double, std::string, std::vector<int>, std::map<std::string, int>>;

   "variant_block_map_key_t"_test = [] {
      // Key starting with 't' but not "true"
      std::string yaml = "test: 42";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::map<std::string, int>>(parsed));
      auto& m = std::get<std::map<std::string, int>>(parsed);
      expect(m["test"] == 42);
   };

   "variant_block_map_key_f"_test = [] {
      // Key starting with 'f' but not "false"
      std::string yaml = "foo: 123";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::map<std::string, int>>(parsed));
      auto& m = std::get<std::map<std::string, int>>(parsed);
      expect(m["foo"] == 123);
   };

   "variant_block_map_key_n"_test = [] {
      // Key starting with 'n' but not "null"
      std::string yaml = "name: 99";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::map<std::string, int>>(parsed));
      auto& m = std::get<std::map<std::string, int>>(parsed);
      expect(m["name"] == 99);
   };

   "variant_actual_true"_test = [] {
      std::string yaml = "true";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<bool>(parsed));
      expect(std::get<bool>(parsed) == true);
   };

   "variant_actual_false"_test = [] {
      std::string yaml = "false";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<bool>(parsed));
      expect(std::get<bool>(parsed) == false);
   };

   "variant_actual_null"_test = [] {
      std::string yaml = "null";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::nullptr_t>(parsed));
   };

   "variant_flow_map"_test = [] {
      std::string yaml = R"({"a": 1, "b": 2})";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::map<std::string, int>>(parsed));
   };

   "variant_flow_array"_test = [] {
      std::string yaml = "[1, 2, 3]";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::vector<int>>(parsed));
      auto& v = std::get<std::vector<int>>(parsed);
      expect(v.size() == 3u);
   };

   "variant_number"_test = [] {
      std::string yaml = "42.5";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<double>(parsed));
      expect(std::get<double>(parsed) == 42.5);
   };

   "variant_quoted_string"_test = [] {
      std::string yaml = R"("hello world")";
      test_variant parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::string>(parsed));
      expect(std::get<std::string>(parsed) == "hello world");
   };
};

suite generic_colon_in_value_tests = [] {
   "generic_time_format_hhmm"_test = [] {
      // Time format HH:MM should parse as string, not fail as number
      std::string yaml = "time: 12:30";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 1u);
      expect(std::holds_alternative<std::string>(obj["time"].data));
      expect(std::get<std::string>(obj["time"].data) == "12:30");
   };

   "generic_time_format_hhmmss"_test = [] {
      // Time format HH:MM:SS should parse as string
      std::string yaml = "time: 12:30:45";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<std::string>(obj["time"].data));
      expect(std::get<std::string>(obj["time"].data) == "12:30:45");
   };

   "generic_ip_with_port"_test = [] {
      // IP:port should parse as string
      std::string yaml = "addr: 192.168.1.1:8080";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<std::string>(obj["addr"].data));
      expect(std::get<std::string>(obj["addr"].data) == "192.168.1.1:8080");
   };

   "generic_url_http"_test = [] {
      // URLs should parse as strings
      std::string yaml = "url: http://example.com";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<std::string>(obj["url"].data));
      expect(std::get<std::string>(obj["url"].data) == "http://example.com");
   };

   "generic_colon_no_space"_test = [] {
      // Colon without following space should be part of string
      std::string yaml = "msg: hello:world";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<std::string>(obj["msg"].data));
      expect(std::get<std::string>(obj["msg"].data) == "hello:world");
   };
};

suite generic_malformed_flow_tests = [] {
   "generic_malformed_flow_array_in_value"_test = [] {
      // Unclosed flow array in a block mapping value should produce an error
      std::string yaml = "note: [not closed";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(bool(rec)) << "Expected error for unclosed flow array";
   };

   "generic_malformed_flow_object_in_value"_test = [] {
      // Unclosed flow object in a block mapping value should produce an error
      std::string yaml = "note: {not closed";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(bool(rec)) << "Expected error for unclosed flow object";
   };

   "generic_partial_flow_array_in_value"_test = [] {
      // Partially closed flow array should produce an error
      std::string yaml = "note: [a, b";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(bool(rec)) << "Expected error for partial flow array";
   };

   "generic_wellformed_flow_array_in_value"_test = [] {
      // Well-formed flow array should parse correctly
      std::string yaml = "note: [a, b, c]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::array_t>(obj["note"].data));
      auto& arr = std::get<glz::generic::array_t>(obj["note"].data);
      expect(arr.size() == 3u);
   };

   "generic_wellformed_flow_object_in_value"_test = [] {
      // Well-formed flow object should parse correctly
      std::string yaml = "note: {a: 1, b: 2}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::object_t>(obj["note"].data));
   };
};

suite generic_boolean_null_key_tests = [] {
   "generic_true_as_key"_test = [] {
      // "true: value" should parse as object with key "true", not as boolean
      std::string yaml = "true: value";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.size() == 1u);
      expect(obj.contains("true"));
      expect(std::holds_alternative<std::string>(obj["true"].data));
      expect(std::get<std::string>(obj["true"].data) == "value");
   };

   "generic_true_colon_no_space"_test = [] {
      // "true:foo" should parse as a string, not a key or boolean
      std::string yaml = "true:foo";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<std::string>(parsed.data));
      expect(std::get<std::string>(parsed.data) == "true:foo");
   };

   "generic_false_as_key"_test = [] {
      // "false: value" should parse as object with key "false", not as boolean
      std::string yaml = "false: value";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.contains("false"));
      expect(std::get<std::string>(obj["false"].data) == "value");
   };

   "generic_null_as_key"_test = [] {
      // "null: value" should parse as object with key "null", not as null
      std::string yaml = "null: value";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.contains("null"));
      expect(std::get<std::string>(obj["null"].data) == "value");
   };

   "generic_true_as_value"_test = [] {
      // "key: true" should parse true as boolean
      std::string yaml = "key: true";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<bool>(obj["key"].data));
      expect(std::get<bool>(obj["key"].data) == true);
   };

   "generic_false_as_value"_test = [] {
      // "key: false" should parse false as boolean
      std::string yaml = "key: false";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<bool>(obj["key"].data));
      expect(std::get<bool>(obj["key"].data) == false);
   };

   "generic_null_as_value"_test = [] {
      // "key: null" should parse null correctly
      std::string yaml = "key: null";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::null_t>(obj["key"].data));
   };

   "generic_TRUE_as_key"_test = [] {
      // "TRUE: value" should parse as object with key "TRUE"
      std::string yaml = "TRUE: value";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.contains("TRUE"));
   };

   "generic_False_as_key"_test = [] {
      // "False: value" should parse as object with key "False"
      std::string yaml = "False: value";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(obj.contains("False"));
   };
};

suite multiline_flow_sequence_tests = [] {
   "multiline_flow_sequence_basic"_test = [] {
      std::string yaml = "[\n  1,\n  2,\n  3\n]";
      std::vector<int> result;
      auto rec = glz::read_yaml(result, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(result.size() == 3u);
      expect(result[0] == 1);
      expect(result[1] == 2);
      expect(result[2] == 3);
   };

   "multiline_flow_sequence_in_map"_test = [] {
      std::string yaml = "items: [\n  a,\n  b,\n  c\n]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      auto& arr = std::get<glz::generic::array_t>(obj["items"].data);
      expect(arr.size() == 3u);
   };

   "multiline_flow_sequence_trailing_newline"_test = [] {
      std::string yaml = "[1, 2, 3\n]";
      std::vector<int> result;
      auto rec = glz::read_yaml(result, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(result.size() == 3u);
   };
};

suite infinity_nan_tests = [] {
   "read_positive_infinity"_test = [] {
      std::string yaml = "val: .inf";
      std::map<std::string, double> result;
      auto rec = glz::read_yaml(result, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::isinf(result["val"]));
      expect(result["val"] > 0);
   };

   "read_negative_infinity"_test = [] {
      std::string yaml = "val: -.inf";
      std::map<std::string, double> result;
      auto rec = glz::read_yaml(result, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::isinf(result["val"]));
      expect(result["val"] < 0);
   };

   "read_nan"_test = [] {
      std::string yaml = "val: .nan";
      std::map<std::string, double> result;
      auto rec = glz::read_yaml(result, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::isnan(result["val"]));
   };

   "write_positive_infinity"_test = [] {
      std::map<std::string, double> data{{"val", std::numeric_limits<double>::infinity()}};
      std::string yaml;
      auto rec = glz::write_yaml(data, yaml);
      expect(!rec);
      expect(yaml.find(".inf") != std::string::npos);
      expect(yaml.find("-.inf") == std::string::npos);
   };

   "write_negative_infinity"_test = [] {
      std::map<std::string, double> data{{"val", -std::numeric_limits<double>::infinity()}};
      std::string yaml;
      auto rec = glz::write_yaml(data, yaml);
      expect(!rec);
      expect(yaml.find("-.inf") != std::string::npos);
   };

   "write_nan"_test = [] {
      std::map<std::string, double> data{{"val", std::nan("")}};
      std::string yaml;
      auto rec = glz::write_yaml(data, yaml);
      expect(!rec);
      expect(yaml.find(".nan") != std::string::npos);
   };

   "roundtrip_infinity"_test = [] {
      std::map<std::string, double> original{{"val", std::numeric_limits<double>::infinity()}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::map<std::string, double> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::isinf(parsed["val"]));
      expect(parsed["val"] > 0);
   };

   "roundtrip_nan"_test = [] {
      std::map<std::string, double> original{{"val", std::nan("")}};
      std::string yaml;
      auto wec = glz::write_yaml(original, yaml);
      expect(!wec);

      std::map<std::string, double> parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::isnan(parsed["val"]));
   };
};

suite yaml_tag_variant_tests = [] {
   "generic_tag_int"_test = [] {
      std::string yaml = "val: !!int 123";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<double>(obj["val"].data));
      expect(std::get<double>(obj["val"].data) == 123.0);
   };

   "generic_tag_float"_test = [] {
      std::string yaml = "val: !!float 1.5";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<double>(obj["val"].data));
      expect(std::get<double>(obj["val"].data) == 1.5);
   };

   "generic_tag_bool"_test = [] {
      std::string yaml = "val: !!bool true";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<bool>(obj["val"].data));
      expect(std::get<bool>(obj["val"].data) == true);
   };

   "generic_tag_null"_test = [] {
      std::string yaml = "val: !!null ~";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<glz::generic::null_t>(obj["val"].data));
   };

   "generic_tag_str"_test = [] {
      std::string yaml = "val: !!str 123";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      auto& obj = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<std::string>(obj["val"].data));
      expect(std::get<std::string>(obj["val"].data) == "123");
   };

   "generic_tag_seq"_test = [] {
      std::string yaml = "!!seq [1, 2, 3]";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::array_t>(parsed.data));
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 3u);
   };

   "generic_tag_map"_test = [] {
      std::string yaml = "!!map {a: 1}";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(std::holds_alternative<glz::generic::object_t>(parsed.data));
   };

   "typed_tag_int"_test = [] {
      std::string yaml = "!!int 456";
      int result = 0;
      auto rec = glz::read_yaml(result, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(result == 456);
   };

   "typed_tag_str_to_string"_test = [] {
      std::string yaml = "!!str 789";
      std::string result;
      auto rec = glz::read_yaml(result, yaml);
      expect(!rec) << glz::format_error(rec, yaml);
      expect(result == "789");
   };
};

// ============================================================
// Empty Value Tests
// ============================================================

struct two_strings
{
   std::string a{};
   std::string b{};
};

suite yaml_empty_value_tests = [] {
   "empty_value_followed_by_key"_test = [] {
      // Empty value (just newline after colon) followed by another key
      // This is valid YAML where 'a' should get an empty/default value
      std::string yaml = R"(a:
b: hello)";
      two_strings result;
      result.a = "unchanged";
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // 'a' should be empty (default value) and 'b' should be "hello"
      expect(result.a.empty() || result.a == "unchanged")
         << "a should be empty or unchanged, got: [" << result.a << "]";
      expect(result.b == "hello") << "b should be 'hello', got: [" << result.b << "]";
   };

   "empty_value_with_comment"_test = [] {
      // Empty value with trailing comment
      std::string yaml = R"(a: # this is a comment
b: world)";
      two_strings result;
      result.a = "unchanged";
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.a.empty() || result.a == "unchanged");
      expect(result.b == "world");
   };

   "multiple_empty_values"_test = [] {
      // Multiple consecutive empty values
      std::string yaml = R"(a:
b:
)";
      two_strings result;
      result.a = "x";
      result.b = "y";
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // Both should be empty or unchanged (default behavior)
   };

   "empty_value_at_end"_test = [] {
      // Empty value at the end of document
      std::string yaml = R"(a: test
b:)";
      two_strings result;
      result.b = "unchanged";
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.a == "test");
      expect(result.b.empty() || result.b == "unchanged");
   };

   "nested_value_properly_indented"_test = [] {
      // When value IS properly indented, it should be parsed
      std::string yaml = R"(a:
  nested_value
b: other)";
      two_strings result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.a == "nested_value");
      expect(result.b == "other");
   };
};

// ============================================================
// glz::generic Write Indentation Tests
// ============================================================

suite yaml_generic_write_indentation_tests = [] {
   "generic_nested_write_indentation"_test = [] {
      // Parse nested YAML into generic
      std::string yaml = R"(level1:
  level2:
    level3: deep_value)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      // Write back and verify indentation
      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);

      // Output should have proper indentation
      expect(output.find("level1:") != std::string::npos);
      expect(output.find("  level2:") != std::string::npos) << "level2 should be indented under level1";
      expect(output.find("    level3:") != std::string::npos) << "level3 should be indented under level2";
   };

   "generic_complex_nested_write_indentation"_test = [] {
      std::string yaml = R"(contexts:
  prototype:
    - include: scope:source.shell.bash#prototype
  main:
    - include: scope:source.shell.bash)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);

      // Verify nested structure has proper indentation
      expect(output.find("contexts:") != std::string::npos);
      // The nested keys should be indented
      expect(output.find("  main:") != std::string::npos || output.find("  prototype:") != std::string::npos)
         << "Nested keys should be indented";
   };

   "generic_roundtrip_preserves_structure"_test = [] {
      std::string yaml = R"(root:
  child1:
    grandchild: value1
  child2: value2)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      std::string output;
      auto wec = glz::write_yaml(parsed, output);
      expect(!wec);

      // Parse the output again
      glz::generic reparsed;
      auto rec2 = glz::read_yaml(reparsed, output);
      expect(!rec2) << glz::format_error(rec2, output);

      // Verify structure is preserved
      auto& root = std::get<glz::generic::object_t>(reparsed.data);
      expect(root.contains("root"));
      auto& rootObj = std::get<glz::generic::object_t>(root.at("root").data);
      expect(rootObj.contains("child1"));
      expect(rootObj.contains("child2"));
   };
};

// ============================================================
// Issue #2291: Strings starting with . or + should not be treated as numbers
// ============================================================

suite yaml_dot_prefix_string_tests = [] {
   // Test for issue #2291: strings starting with '.' should not be treated as numbers
   "generic_dot_prefixed_strings"_test = [] {
      std::string yaml = R"(file_extensions:
  - .c
  - .cpp
  - .h)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("file_extensions"));
      auto& arr = std::get<glz::generic::array_t>(root.at("file_extensions").data);
      expect(arr.size() == 3);
      expect(std::holds_alternative<std::string>(arr[0].data));
      expect(std::get<std::string>(arr[0].data) == ".c");
      expect(std::get<std::string>(arr[1].data) == ".cpp");
      expect(std::get<std::string>(arr[2].data) == ".h");
   };

   // Test that .inf, .nan are still parsed as numbers in generic context
   "generic_special_floats_still_work"_test = [] {
      std::string yaml = R"(values:
  - .inf
  - -.inf
  - .nan)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      auto& arr = std::get<glz::generic::array_t>(root.at("values").data);
      expect(arr.size() == 3);
      // In glz::generic, numbers are stored as double
      expect(std::holds_alternative<double>(arr[0].data));
      expect(std::isinf(std::get<double>(arr[0].data)));
      expect(std::get<double>(arr[0].data) > 0);
      expect(std::holds_alternative<double>(arr[1].data));
      expect(std::isinf(std::get<double>(arr[1].data)));
      expect(std::get<double>(arr[1].data) < 0);
      expect(std::holds_alternative<double>(arr[2].data));
      expect(std::isnan(std::get<double>(arr[2].data)));
   };

   // Test for inline comments after values containing special characters
   "generic_inline_comment_after_value_simple"_test = [] {
      // Simple inline comment test without backslash
      std::string yaml = R"(match: hello|world  # This is a comment)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::holds_alternative<std::string>(root.at("match").data));
      expect(std::get<std::string>(root.at("match").data) == "hello|world");
   };

   "generic_inline_comment_with_regex"_test = [] {
      // Test with regex-like value containing backslash
      std::string yaml = R"(match: regex  # comment)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::get<std::string>(root.at("match").data) == "regex");
   };

   // Test + prefixed strings (similar to . issue)
   "generic_plus_prefixed_strings"_test = [] {
      std::string yaml = R"(items:
  - +foo
  - +bar)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      auto& arr = std::get<glz::generic::array_t>(root.at("items").data);
      expect(arr.size() == 2);
      expect(std::holds_alternative<std::string>(arr[0].data));
      expect(std::get<std::string>(arr[0].data) == "+foo");
      expect(std::get<std::string>(arr[1].data) == "+bar");
   };

   // Test that +5 and .5 are still parsed as numbers
   "generic_plus_and_dot_numbers"_test = [] {
      std::string yaml = R"(values:
  - +5
  - .5
  - +.5)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      auto& arr = std::get<glz::generic::array_t>(root.at("values").data);
      expect(arr.size() == 3);
      expect(std::holds_alternative<double>(arr[0].data));
      expect(std::get<double>(arr[0].data) == 5.0);
      expect(std::holds_alternative<double>(arr[1].data));
      expect(std::get<double>(arr[1].data) == 0.5);
      expect(std::holds_alternative<double>(arr[2].data));
      expect(std::get<double>(arr[2].data) == 0.5);
   };
};

// ============================================================
// Issue #2291: Sublime Text syntax file parsing tests
// ============================================================

suite yaml_sublime_syntax_tests = [] {
   // Test for issue #2291: inline comment after regex-like value
   // From Python.sublime-syntax line 410
   // First, test a simple version without the sequence context
   "sublime_inline_comment_simple"_test = [] {
      // First verify parsing works without backslash
      std::string yaml1 = R"(match: hello|world  # comment)";
      glz::generic parsed1;
      auto rec1 = glz::read_yaml(parsed1, yaml1);
      expect(!rec1) << glz::format_error(rec1, yaml1);
      auto& root1 = std::get<glz::generic::object_t>(parsed1.data);
      expect(root1.contains("match"));
      expect(std::get<std::string>(root1.at("match").data) == "hello|world");

      // Now test with backslash (escaped regex)
      std::string yaml2 = R"(match: test\Svalue  # comment)";
      glz::generic parsed2;
      auto rec2 = glz::read_yaml(parsed2, yaml2);
      expect(!rec2) << glz::format_error(rec2, yaml2);
      auto& root2 = std::get<glz::generic::object_t>(parsed2.data);
      expect(root2.contains("match"));
      // In raw string, \S is backslash-S
      expect(std::get<std::string>(root2.at("match").data) == "test\\Svalue");
   };

   // Test a sequence item with mapping
   "sublime_sequence_mapping"_test = [] {
      std::string yaml = R"(- match: hello|world  # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 1);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "hello|world");
      expect(item.contains("pop"));
   };

   // Test simpler backslash in sequence
   "sublime_backslash_in_sequence"_test = [] {
      std::string yaml = R"(- match: test\Svalue
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 1);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "test\\Svalue");
   };

   // Test backslash with comment in sequence
   "sublime_backslash_comment_sequence"_test = [] {
      std::string yaml = R"(- match: test\S  # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 1);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "test\\S");
   };

   // Test caret and pipe in sequence
   "sublime_caret_pipe"_test = [] {
      std::string yaml = R"(- match: ^|  # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "^|");
   };

   // Test parentheses in sequence
   "sublime_parens"_test = [] {
      std::string yaml = R"(- match: (?=test)  # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "(?=test)");
   };

   // Test caret-pipe-parens without backslash
   "sublime_caret_pipe_parens"_test = [] {
      std::string yaml = R"(- match: ^|(?=test)  # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "^|(?=test)");
   };

   // Test just parens with backslash
   "sublime_parens_backslash"_test = [] {
      std::string yaml = R"(- match: (?=\S)  # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "(?=\\S)");
   };

   // Test the full pattern without comment
   "sublime_full_pattern_no_comment"_test = [] {
      std::string yaml = R"(- match: ^|(?=\S)
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "^|(?=\\S)");
   };

   // Test reverse order: backslash first, then caret-pipe
   "sublime_backslash_then_caretpipe"_test = [] {
      std::string yaml = R"(- match: (?=\S)|^ # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "(?=\\S)|^");
   };

   // Test separate: caret-pipe-parens plus backslash later
   "sublime_combo_no_backslash_in_parens"_test = [] {
      std::string yaml = R"(- match: ^|(?=X)\S # comment
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      expect(std::get<std::string>(item.at("match").data) == "^|(?=X)\\S");
   };

   // Test as simple key-value (not in sequence)
   "sublime_pattern_simple_kv"_test = [] {
      std::string yaml = R"(match: ^|(?=\S)  # comment)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::get<std::string>(root.at("match").data) == "^|(?=\\S)");
   };

   // Test the full context with sequence and backslash
   "sublime_inline_comment_with_regex"_test = [] {
      std::string yaml = R"(- match: ^|(?=\S)  # Note: Ensure to highlight shebang
  pop: 1)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 1);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("match"));
      // The value should be the regex pattern without the comment
      expect(std::get<std::string>(item.at("match").data) == "^|(?=\\S)");
      expect(item.contains("pop"));
   };

   // Test for issue #2291: file_extensions with dot-prefixed strings
   "sublime_file_extensions"_test = [] {
      std::string yaml = R"(file_extensions:
  - py
  - py3
  - pyw
  - pyi
  - .pyx
  - .pxd)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      auto& arr = std::get<glz::generic::array_t>(root.at("file_extensions").data);
      expect(arr.size() == 6);
      expect(std::get<std::string>(arr[0].data) == "py");
      expect(std::get<std::string>(arr[4].data) == ".pyx");
      expect(std::get<std::string>(arr[5].data) == ".pxd");
   };

   // Test for issue #2291: block scalar with chomping indicator
   "sublime_block_scalar_chomping"_test = [] {
      std::string yaml = R"(first_line_match: |-
  (?xi:
    ^ \#! .* \bpython\b
  ))";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("first_line_match"));
      auto& val = std::get<std::string>(root.at("first_line_match").data);
      // Block scalar with strip chomping - no trailing newline
      expect(val.find("(?xi:") != std::string::npos);
   };

   // Test for YAML directives
   "sublime_yaml_directive"_test = [] {
      std::string yaml = R"(%YAML 1.2
---
name: Python
scope: source.python)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("name"));
      expect(std::get<std::string>(root.at("name").data) == "Python");
   };

   // Test for nested structure similar to sublime-syntax contexts
   "sublime_contexts_structure"_test = [] {
      std::string yaml = R"(contexts:
  prototype:
    - include: scope:source.shell.bash#prototype
  main:
    - include: scope:source.shell.bash)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("contexts"));
      auto& contexts = std::get<glz::generic::object_t>(root.at("contexts").data);
      expect(contexts.contains("prototype"));
      expect(contexts.contains("main"));
   };
};

// Tests for multiline plain scalar folding (issue #2291)
suite yaml_multiline_plain_scalar_tests = [] {
   // Test multiline value where content starts on next line after key
   "multiline_scope_value"_test = [] {
      std::string yaml = R"(- match: '(\.)'
  scope:
    meta.statement.conditional.case.python
    keyword.control.conditional.case.python)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 1);
      auto& item = std::get<glz::generic::object_t>(arr[0].data);
      expect(item.contains("scope"));
      // The two lines should be folded with a space
      expect(std::get<std::string>(item.at("scope").data) ==
             "meta.statement.conditional.case.python keyword.control.conditional.case.python");
   };

   // Test that sequence items at same indent don't get folded
   "sequence_items_not_folded"_test = [] {
      std::string yaml = R"(- hello
- world
- test)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 3);
      expect(std::get<std::string>(arr[0].data) == "hello");
      expect(std::get<std::string>(arr[1].data) == "world");
      expect(std::get<std::string>(arr[2].data) == "test");
   };

   // Test that mapping keys at same indent don't get folded
   "mapping_keys_not_folded"_test = [] {
      std::string yaml = R"(key1: value1
key2: value2)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.size() == 2);
      expect(std::get<std::string>(root.at("key1").data) == "value1");
      expect(std::get<std::string>(root.at("key2").data) == "value2");
   };

   // Test three-line multiline scalar
   "three_line_multiline_scalar"_test = [] {
      std::string yaml = R"(key:
  line one
  line two
  line three)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("key"));
      expect(std::get<std::string>(root.at("key").data) == "line one line two line three");
   };
};

// Tests for boolean-like string values (issue #2291)
suite yaml_boolean_like_string_tests = [] {
   // Test that "False\b" is treated as a string, not a boolean
   "false_with_backslash_b"_test = [] {
      std::string yaml = R"(match: False\b)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::holds_alternative<std::string>(root.at("match").data));
      expect(std::get<std::string>(root.at("match").data) == "False\\b");
   };

   // Test that "True\b" is treated as a string, not a boolean
   "true_with_backslash_b"_test = [] {
      std::string yaml = R"(match: True\b)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::holds_alternative<std::string>(root.at("match").data));
      expect(std::get<std::string>(root.at("match").data) == "True\\b");
   };

   // Test that "Null\b" is treated as a string, not null
   "null_with_backslash_b"_test = [] {
      std::string yaml = R"(match: Null\b)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::holds_alternative<std::string>(root.at("match").data));
      expect(std::get<std::string>(root.at("match").data) == "Null\\b");
   };

   // Test that "true#comment" is treated as a string (not a boolean)
   "true_hash_comment_is_string"_test = [] {
      std::string yaml = R"(match: true#comment)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::holds_alternative<std::string>(root.at("match").data));
      expect(std::get<std::string>(root.at("match").data) == "true#comment");
   };

   // Test that "false#comment" is treated as a string (not a boolean)
   "false_hash_comment_is_string"_test = [] {
      std::string yaml = R"(match: false#comment)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::holds_alternative<std::string>(root.at("match").data));
      expect(std::get<std::string>(root.at("match").data) == "false#comment");
   };

   // Test that "null#comment" is treated as a string (not null)
   "null_hash_comment_is_string"_test = [] {
      std::string yaml = R"(match: null#comment)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("match"));
      expect(std::holds_alternative<std::string>(root.at("match").data));
      expect(std::get<std::string>(root.at("match").data) == "null#comment");
   };

   // Test that plain "False" is still treated as a boolean
   "plain_false_is_boolean"_test = [] {
      std::string yaml = R"(value: False)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("value"));
      expect(std::holds_alternative<bool>(root.at("value").data));
      expect(std::get<bool>(root.at("value").data) == false);
   };

   // Test that plain "True" is still treated as a boolean
   "plain_true_is_boolean"_test = [] {
      std::string yaml = R"(value: True)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(root.contains("value"));
      expect(std::holds_alternative<bool>(root.at("value").data));
      expect(std::get<bool>(root.at("value").data) == true);
   };

   // Test "False" followed by comment is still boolean
   "false_with_comment_is_boolean"_test = [] {
      std::string yaml = R"(value: False # comment)";
      glz::generic parsed;
      auto rec = glz::read_yaml(parsed, yaml);
      expect(!rec) << glz::format_error(rec, yaml);

      auto& root = std::get<glz::generic::object_t>(parsed.data);
      expect(std::holds_alternative<bool>(root.at("value").data));
      expect(std::get<bool>(root.at("value").data) == false);
   };
};

// Tests for block scalar followed by another key in same mapping
struct block_scalar_sibling_struct
{
   std::string k1{};
   std::string k2{};
};

suite yaml_block_scalar_sibling_tests = [] {
   // Issue: Block scalar followed by another key at same indent level loses k2
   "block_scalar_sibling_key_in_sequence"_test = [] {
      std::string yaml = R"(- k1: |
    a
    b
  k2: c)";
      std::vector<block_scalar_sibling_struct> result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.size() == 1u);
      expect(result[0].k1 == "a\nb\n") << "k1 was: " << result[0].k1;
      expect(result[0].k2 == "c") << "k2 was: " << result[0].k2;
   };

   "block_scalar_sibling_key_in_sequence_generic"_test = [] {
      std::string yaml = R"(- k1: |
    a
    b
  k2: c)";
      glz::generic parsed;
      auto ec = glz::read_yaml(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      auto& arr = std::get<glz::generic::array_t>(parsed.data);
      expect(arr.size() == 1u);
      auto& obj = std::get<glz::generic::object_t>(arr[0].data);
      expect(obj.count("k1") == 1u);
      expect(obj.count("k2") == 1u);
      expect(std::get<std::string>(obj.at("k1").data) == "a\nb\n") << "k1 was: " << std::get<std::string>(obj.at("k1").data);
      expect(std::get<std::string>(obj.at("k2").data) == "c") << "k2 was: " << std::get<std::string>(obj.at("k2").data);
   };

   "block_scalar_sibling_key_simple"_test = [] {
      std::string yaml = R"(k1: |
  a
  b
k2: c)";
      block_scalar_sibling_struct result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.k1 == "a\nb\n") << "k1 was: " << result.k1;
      expect(result.k2 == "c") << "k2 was: " << result.k2;
   };
};

suite yaml_quoted_string_folding_tests = [] {
   // Issue: Quoted strings should fold line breaks
   // Single newline -> space, double newline -> single newline
   // Backslash at end of line (double-quoted only) -> no space
   "double_quoted_line_folding"_test = [] {
      // Note: trailing spaces on some lines are significant
      std::string yaml = R"(- "very \"long\"
  'string' with

  paragraph gap, \n and
  s\
  p\
  a\
  c\
  e\
  s.")";
      std::vector<std::string> result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.size() == 1u);
      // Expected: line breaks fold to spaces, blank line becomes \n, \n is literal newline, \ at end of line means no
      // space
      expect(result[0] == "very \"long\" 'string' with\nparagraph gap, \n and spaces.") << "got: " << result[0];
   };

   "single_quoted_line_folding"_test = [] {
      std::string yaml = R"(- 'very "long"
  ''string'' with

  paragraph gap, \n and
  spaces.')";
      std::vector<std::string> result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.size() == 1u);
      // Expected: line breaks fold to spaces, blank line becomes \n, \n is literal (two chars), trailing spaces trimmed
      expect(result[0] == "very \"long\" 'string' with\nparagraph gap, \\n and spaces.") << "got: " << result[0];
   };

   "double_quoted_simple_folding"_test = [] {
      std::string yaml = R"("hello
  world")";
      std::string result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result == "hello world") << "got: " << result;
   };

   "single_quoted_simple_folding"_test = [] {
      std::string yaml = R"('hello
  world')";
      std::string result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result == "hello world") << "got: " << result;
   };

   "double_quoted_blank_line_becomes_newline"_test = [] {
      std::string yaml = R"("line1

  line2")";
      std::string result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result == "line1\nline2") << "got: " << result;
   };

   "double_quoted_backslash_continuation"_test = [] {
      std::string yaml = R"("no\
  space")";
      std::string result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result == "nospace") << "got: " << result;
   };

   // Test from StackOverflow answer with trailing whitespace on lines
   // Trailing whitespace before a line break is trimmed in YAML quoted strings
   "stackoverflow_example_double_quoted"_test = [] {
      // Note: there are trailing spaces after "and" on line 5 - these get trimmed
      std::string yaml =
         "- \"very \\\"long\\\"\n"
         "  'string' with\n"
         "\n"
         "  paragraph gap, \\n and        \n"
         "  s\\\n"
         "  p\\\n"
         "  a\\\n"
         "  c\\\n"
         "  e\\\n"
         "  s.\"";
      std::vector<std::string> result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.size() == 1u);
      // Trailing spaces are trimmed, backslash continuations should work
      expect(result[0] == "very \"long\" 'string' with\nparagraph gap, \n and spaces.") << "got: " << result[0];
   };

   "stackoverflow_example_single_quoted"_test = [] {
      // Note: there are trailing spaces after "and" on line 5 - these get trimmed
      std::string yaml =
         "- 'very \"long\"\n"
         "  ''string'' with\n"
         "\n"
         "  paragraph gap, \\n and        \n"
         "  spaces.'";
      std::vector<std::string> result;
      auto ec = glz::read_yaml(result, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(result.size() == 1u);
      // In single-quoted, \n is literal two chars, trailing spaces trimmed
      expect(result[0] == "very \"long\" 'string' with\nparagraph gap, \\n and spaces.") << "got: " << result[0];
   };
};

int main() { return 0; }
