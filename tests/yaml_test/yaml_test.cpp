// Glaze Library
// For the license information refer to glaze.hpp

#include <array>
#include <cmath>
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

#include "glaze/yaml.hpp"
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
   static constexpr auto value =
      object("name", &T::name, "employees", &T::employees, "departments", &T::departments);
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

int main() { return 0; }
