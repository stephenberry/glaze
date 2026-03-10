// Glaze Library
// For the license information refer to glaze.hpp
// P2996 Reflection Test - Verifies C++26 P2996 reflection works with Glaze

#include <cstdint>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct TestStruct
{
   std::string name;
   int value;
   double data;
};

// C-style array in struct - requires P2996 reflection for automatic serialization (issue #1839)
struct SingleArrayStruct
{
   int data[10];
};

struct MixedArrayStruct
{
   uint16_t ints[2];
   float floats[3];
   std::string name;
};

struct NestedWithArray
{
   std::string label;
   double values[4];
   int count;
};

// Test enum WITHOUT any glz::meta - used for reflect_enums option test
enum class Direction { North, South, East, West };

// Custom opts with reflect_enums enabled
struct reflect_enums_opts : glz::opts
{
   bool reflect_enums = true;
};

suite p2996_reflection = [] {
   "member_names"_test = [] {
      constexpr auto names = glz::member_names<TestStruct>;
      expect(names.size() == 3);
      expect(names[0] == "name");
      expect(names[1] == "value");
      expect(names[2] == "data");
   };

   "json round-trip"_test = [] {
      TestStruct obj{"test", 42, 3.14};
      auto json = glz::write_json(obj).value_or("error");

      TestStruct obj2{};
      expect(!glz::read_json(obj2, json));
      expect(obj2.name == "test");
      expect(obj2.value == 42);
   };
};

suite p2996_enums = [] {
   "enum_to_string"_test = [] {
      expect(glz::enum_to_string(Direction::North) == "North");
      expect(glz::enum_to_string(Direction::South) == "South");
      expect(glz::enum_to_string(Direction::East) == "East");
      expect(glz::enum_to_string(Direction::West) == "West");
   };

   "string_to_enum"_test = [] {
      expect(glz::string_to_enum<Direction>("North") == Direction::North);
      expect(glz::string_to_enum<Direction>("South") == Direction::South);
      expect(glz::string_to_enum<Direction>("East") == Direction::East);
      expect(glz::string_to_enum<Direction>("West") == Direction::West);
      expect(glz::string_to_enum<Direction>("Invalid") == std::nullopt);
   };

   "reflect_enums option"_test = [] {
      Direction d = Direction::East;

      std::string dir_json;
      expect(not glz::write<reflect_enums_opts{}>(d, dir_json));
      expect(dir_json == "\"East\"");

      Direction d2{};
      expect(not glz::read<reflect_enums_opts{}>(d2, dir_json));
      expect(d2 == Direction::East);
   };
};

// C-style array in struct tests (issue #1839)
// With P2996 reflection, structs containing C-style arrays can be
// automatically reflected without requiring glz::meta specialization.
suite c_style_array_reflection = [] {
   "single array member reflection"_test = [] {
      constexpr auto names = glz::member_names<SingleArrayStruct>;
      expect(names.size() == 1);
      expect(names[0] == "data");
   };

   "single array member json round-trip"_test = [] {
      SingleArrayStruct obj{};
      for (int i = 0; i < 10; ++i) {
         obj.data[i] = i * 10;
      }

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"data":[0,10,20,30,40,50,60,70,80,90]})") << s;

      SingleArrayStruct obj2{};
      expect(not glz::read_json(obj2, s));
      for (int i = 0; i < 10; ++i) {
         expect(obj2.data[i] == i * 10);
      }
   };

   "mixed arrays and regular members"_test = [] {
      MixedArrayStruct obj{};
      obj.ints[0] = 1;
      obj.ints[1] = 2;
      obj.floats[0] = 1.5f;
      obj.floats[1] = 2.5f;
      obj.floats[2] = 3.5f;
      obj.name = "test";

      std::string s{};
      expect(not glz::write_json(obj, s));

      MixedArrayStruct obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.ints[0] == 1);
      expect(obj2.ints[1] == 2);
      expect(obj2.floats[0] == 1.5f);
      expect(obj2.floats[1] == 2.5f);
      expect(obj2.floats[2] == 3.5f);
      expect(obj2.name == "test");
   };

   "array member among scalar members"_test = [] {
      NestedWithArray obj{};
      obj.label = "sensor";
      obj.values[0] = 1.1;
      obj.values[1] = 2.2;
      obj.values[2] = 3.3;
      obj.values[3] = 4.4;
      obj.count = 42;

      std::string s{};
      expect(not glz::write_json(obj, s));

      NestedWithArray obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.label == "sensor");
      expect(obj2.count == 42);
      expect(obj2.values[0] == 1.1);
      expect(obj2.values[1] == 2.2);
      expect(obj2.values[2] == 3.3);
      expect(obj2.values[3] == 4.4);
   };

   "single array member beve round-trip"_test = [] {
      SingleArrayStruct obj{};
      for (int i = 0; i < 10; ++i) {
         obj.data[i] = i + 100;
      }

      std::string s{};
      expect(not glz::write_beve(obj, s));

      SingleArrayStruct obj2{};
      expect(not glz::read_beve(obj2, s));
      for (int i = 0; i < 10; ++i) {
         expect(obj2.data[i] == i + 100);
      }
   };
};

int main() {}
