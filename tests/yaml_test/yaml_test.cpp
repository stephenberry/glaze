// Glaze Library
// For the license information refer to glaze.hpp

#include <cmath>
#include <map>
#include <optional>
#include <string>
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
      auto ec = glz::read<glz::opts{.format = glz::YAML}>(obj, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(obj.flag == true);
   };

   "read_boolean_false"_test = [] {
      std::string yaml = "flag: false";
      bool_struct obj{.flag = true};
      auto ec = glz::read<glz::opts{.format = glz::YAML}>(obj, yaml);
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

int main() { return 0; }
