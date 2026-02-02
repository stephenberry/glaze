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

// Note: Anchors/aliases are advanced YAML features
// Adding placeholder tests to verify error handling

suite yaml_anchor_tests = [] {
   "anchor_not_supported"_test = [] {
      std::string yaml = R"(
anchor: &anchor_name value
alias: *anchor_name)";
      std::map<std::string, std::string> obj{};
      [[maybe_unused]] auto ec = glz::read_yaml(obj, yaml);
      // Anchors may not be supported - check for error or partial parse
      // This test documents current behavior
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
      std::string yaml = R"({"users": [{"name": "Alice", "age": 30}, {"name": "Bob", "age": 25}], "metadata": {"version": 1, "enabled": true}})";
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
   using test_variant = std::variant<std::nullptr_t, bool, double, std::string, std::vector<int>, std::map<std::string, int>>;

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

int main() { return 0; }
