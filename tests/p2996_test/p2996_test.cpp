// Glaze Library
// For the license information refer to glaze.hpp
// P2996 Reflection Test - Verifies C++26 P2996 reflection works with Glaze

#include <cstdint>
#include <memory>

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

// Array of structs in a struct
struct Point
{
   double x;
   double y;
};

struct Polygon
{
   Point vertices[4];
   std::string name;
};

// Struct containing a struct with an array
struct Inner
{
   int values[3];
};

struct Outer
{
   Inner inner;
   std::string tag;
};

// Multi-dimensional C-style array
struct Matrix
{
   int data[2][3];
};

// Deeply nested: array of structs that contain arrays
struct Segment
{
   double coords[2];
};

struct Path
{
   Segment segments[3];
   int id;
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

      expect(glz::write<reflect_enums_opts{}>(static_cast<Direction>(7), dir_json) == glz::error_code::unexpected_enum);
   };

   // The reflective enum-by-name reader scans the quoted key directly (not via skip_ws), so on a
   // non-null-terminated buffer (no trailing '\0' sentinel) it must respect end. Each input below
   // is read over an exact-size buffer. A key missing its closing quote must error rather than
   // scan past the end: with the unbounded scan, "North" (no closing quote) would read past the
   // buffer and wrongly succeed, so expecting an error here pins the bounded behavior. Under the
   // ASAN CI configuration this additionally catches the over-read itself.
   "reflect_enums non-null-terminated bounds"_test = [] {
      static constexpr auto options = [] {
         reflect_enums_opts o{};
         o.null_terminated = false;
         return o;
      }();

      for (const std::string_view s : {"\"North", "\"Sou", "\""}) {
         std::vector<char> buf{s.begin(), s.end()};
         Direction d{};
         const auto ec = glz::read<options>(d, std::string_view{buf.data(), buf.data() + buf.size()});
         expect(bool(ec)) << "unterminated reflective enum key should error";
      }

      // A complete value still reads correctly over an exact-size buffer.
      const std::string_view complete = "\"West\"";
      std::vector<char> buf{complete.begin(), complete.end()};
      Direction d{};
      const auto ec = glz::read<options>(d, std::string_view{buf.data(), buf.data() + buf.size()});
      expect(not ec);
      expect(d == Direction::West);
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

suite c_style_array_of_structs = [] {
   "array of structs"_test = [] {
      Polygon obj{};
      obj.vertices[0] = {0.0, 0.0};
      obj.vertices[1] = {1.0, 0.0};
      obj.vertices[2] = {1.0, 1.0};
      obj.vertices[3] = {0.0, 1.0};
      obj.name = "square";

      std::string s{};
      expect(not glz::write_json(obj, s));

      Polygon obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.name == "square");
      expect(obj2.vertices[0].x == 0.0);
      expect(obj2.vertices[0].y == 0.0);
      expect(obj2.vertices[1].x == 1.0);
      expect(obj2.vertices[2].y == 1.0);
      expect(obj2.vertices[3].x == 0.0);
      expect(obj2.vertices[3].y == 1.0);
   };

   "nested struct containing array"_test = [] {
      Outer obj{};
      obj.inner.values[0] = 10;
      obj.inner.values[1] = 20;
      obj.inner.values[2] = 30;
      obj.tag = "outer";

      std::string s{};
      expect(not glz::write_json(obj, s));

      Outer obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.tag == "outer");
      expect(obj2.inner.values[0] == 10);
      expect(obj2.inner.values[1] == 20);
      expect(obj2.inner.values[2] == 30);
   };

   "multi-dimensional array"_test = [] {
      Matrix obj{};
      obj.data[0][0] = 1;
      obj.data[0][1] = 2;
      obj.data[0][2] = 3;
      obj.data[1][0] = 4;
      obj.data[1][1] = 5;
      obj.data[1][2] = 6;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"data":[[1,2,3],[4,5,6]]})") << s;

      Matrix obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.data[0][0] == 1);
      expect(obj2.data[0][2] == 3);
      expect(obj2.data[1][0] == 4);
      expect(obj2.data[1][2] == 6);
   };

   "array of structs containing arrays"_test = [] {
      Path obj{};
      obj.segments[0].coords[0] = 0.0;
      obj.segments[0].coords[1] = 0.0;
      obj.segments[1].coords[0] = 1.0;
      obj.segments[1].coords[1] = 2.0;
      obj.segments[2].coords[0] = 3.0;
      obj.segments[2].coords[1] = 4.0;
      obj.id = 99;

      std::string s{};
      expect(not glz::write_json(obj, s));

      Path obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.id == 99);
      expect(obj2.segments[0].coords[0] == 0.0);
      expect(obj2.segments[0].coords[1] == 0.0);
      expect(obj2.segments[1].coords[0] == 1.0);
      expect(obj2.segments[1].coords[1] == 2.0);
      expect(obj2.segments[2].coords[0] == 3.0);
      expect(obj2.segments[2].coords[1] == 4.0);
   };

   "array of structs beve round-trip"_test = [] {
      Path obj{};
      obj.segments[0].coords[0] = 1.5;
      obj.segments[0].coords[1] = 2.5;
      obj.segments[1].coords[0] = 3.5;
      obj.segments[1].coords[1] = 4.5;
      obj.segments[2].coords[0] = 5.5;
      obj.segments[2].coords[1] = 6.5;
      obj.id = 42;

      std::string s{};
      expect(not glz::write_beve(obj, s));

      Path obj2{};
      expect(not glz::read_beve(obj2, s));
      expect(obj2.id == 42);
      expect(obj2.segments[0].coords[0] == 1.5);
      expect(obj2.segments[1].coords[1] == 4.5);
      expect(obj2.segments[2].coords[0] == 5.5);
   };
};

// reflect_array marks a type as a positional array without enumerating its members
struct PositionalPoint
{
   double x{};
   double y{};
   std::string label{};
};

template <>
struct glz::meta<PositionalPoint>
{
   static constexpr auto value = glz::reflect_array{};
};

static_assert(glz::glaze_array_t<PositionalPoint>);

suite p2996_reflect_array = [] {
   "reflect_array json round-trip"_test = [] {
      std::vector<PositionalPoint> values{};
      std::string buffer = R"([[1,2,"a"],[3,4,"b"]])";
      expect(!glz::read_json(values, buffer));
      expect(values.size() == 2);
      expect(values[1].y == 4.0);
      expect(values[1].label == "b");

      auto written = glz::write_json(values).value_or("error");
      expect(written == buffer) << written;
   };

   "reflect_array beve round-trip"_test = [] {
      PositionalPoint obj{1.5, 2.5, "origin"};
      std::string s{};
      expect(not glz::write_beve(obj, s));

      PositionalPoint obj2{};
      expect(not glz::read_beve(obj2, s));
      expect(obj2.x == 1.5);
      expect(obj2.y == 2.5);
      expect(obj2.label == "origin");
   };
};

// Inherited members are reflected ahead of the derived class's own, since
// std::meta::nonstatic_data_members_of lists only the members a class declares itself
namespace inheritance
{
   struct base
   {
      int id = 1;
      std::string name = "base";
   };

   struct derived : base
   {
      double extra = 2.5;
   };

   struct grandchild : derived
   {
      bool flag = true;
   };

   struct position
   {
      int x = 3;
   };

   struct multiple : base, position
   {
      int own = 4;
   };

   // A stateless mixin contributes no members
   struct mixin
   {
      void touch() {}
   };

   struct with_mixin : mixin, base
   {};

   struct shared
   {
      int value = 5;
   };
   struct left : virtual shared
   {
      int l = 6;
   };
   struct right : virtual shared
   {
      int r = 7;
   };
   // The virtual base is one subobject, so its members appear once
   struct diamond : left, right
   {
      int d = 8;
   };

   // A private base is an implementation detail and is not reflected
   class hidden_base
   {
     public:
      hidden_base() = default;
      int secret() const { return secret_; }

     private:
      int secret_ = 9;
   };

   class constructed : private hidden_base
   {
     public:
      constructed() = default;
      constructed(int v, std::string s) : value(v), label(std::move(s)) {}
      int value = 0;
      std::string label{};
      int base_secret() const { return secret(); }
   };

   // A public base contributes its public members only
   struct public_base_with_private_member
   {
      int visible = 15;

     private:
      int hidden = 16;
   };

   struct derived_public_base_private_member : public_base_with_private_member
   {
      int own = 17;
   };

   // The private weak_ptr inside the base must not enter the serialized form
   struct shared_from_this_node : std::enable_shared_from_this<shared_from_this_node>
   {
      shared_from_this_node() = default;
      int value = 18;
   };

   struct shadowing : base
   {
      int id = 10;
   };

   struct meta_base
   {
      int value = 11;
   };

   struct local_meta_base
   {
      int value = 12;
      struct glaze
      {
         static constexpr auto value = glz::object("v", &local_meta_base::value);
      };
   };

   struct derived_from_meta : meta_base
   {
      int own = 13;
   };

   struct derived_from_local_meta : local_meta_base
   {
      int own = 14;
   };

   struct indirectly_from_meta : derived_from_meta
   {};
}

template <>
struct glz::meta<inheritance::meta_base>
{
   using T = inheritance::meta_base;
   static constexpr auto value = glz::object("renamed", &T::value);
};

static_assert(glz::detail::count_members<inheritance::derived> == 3);
static_assert(glz::detail::count_members<inheritance::grandchild> == 4);
static_assert(glz::detail::count_members<inheritance::multiple> == 4);
static_assert(glz::detail::count_members<inheritance::with_mixin> == 2);
static_assert(glz::detail::count_members<inheritance::diamond> == 4);
static_assert(glz::detail::count_members<inheritance::constructed> == 2);
static_assert(glz::detail::count_members<inheritance::derived_public_base_private_member> == 2);
static_assert(glz::detail::count_members<inheritance::shared_from_this_node> == 1);
static_assert(glz::member_names<inheritance::grandchild> ==
              std::array<std::string_view, 4>{"id", "name", "extra", "flag"});
static_assert(glz::member_names<inheritance::multiple> == std::array<std::string_view, 4>{"id", "name", "x", "own"});
static_assert(glz::member_names<inheritance::diamond> == std::array<std::string_view, 4>{"value", "l", "r", "d"});
static_assert(glz::member_names<inheritance::constructed> == std::array<std::string_view, 2>{"value", "label"});
static_assert(glz::member_names<inheritance::derived_public_base_private_member> ==
              std::array<std::string_view, 2>{"visible", "own"});
static_assert(glz::member_names<inheritance::shared_from_this_node> ==
              std::array<std::string_view, 1>{"value"});

// These would give two members one key, or write a base's members under names its glz::meta does not use, so
// reflecting them is a compile error
static_assert(!glz::detail::has_distinct_member_names(^^inheritance::shadowing));
static_assert(glz::detail::has_distinct_member_names(^^inheritance::diamond));
static_assert(glz::detail::has_base_with_glaze_metadata(^^inheritance::derived_from_meta));
static_assert(glz::detail::has_base_with_glaze_metadata(^^inheritance::derived_from_local_meta));
static_assert(glz::detail::has_base_with_glaze_metadata(^^inheritance::indirectly_from_meta));
static_assert(!glz::detail::has_base_with_glaze_metadata(^^inheritance::diamond));

suite p2996_inheritance = [] {
   using namespace inheritance;

   "single inheritance writes base members first"_test = [] {
      auto json = glz::write_json(derived{}).value_or("error");
      expect(json == R"({"id":1,"name":"base","extra":2.5})") << json;
   };

   "multi-level inheritance"_test = [] {
      auto json = glz::write_json(grandchild{}).value_or("error");
      expect(json == R"({"id":1,"name":"base","extra":2.5,"flag":true})") << json;

      grandchild value{};
      expect(not glz::read_json(value, R"({"id":7,"name":"read","extra":1.0,"flag":false})"));
      expect(value.id == 7);
      expect(value.name == "read");
      expect(value.extra == 1.0);
      expect(not value.flag);
   };

   "multiple inheritance in base declaration order"_test = [] {
      auto json = glz::write_json(multiple{}).value_or("error");
      expect(json == R"({"id":1,"name":"base","x":3,"own":4})") << json;

      multiple value{};
      expect(not glz::read_json(value, R"({"x":30,"own":40,"id":10})"));
      expect(value.x == 30);
      expect(value.own == 40);
      expect(value.id == 10);
      expect(value.name == "base");
   };

   "stateless mixin"_test = [] {
      auto json = glz::write_json(with_mixin{}).value_or("error");
      expect(json == R"({"id":1,"name":"base"})") << json;
   };

   "virtual diamond has one copy of the shared base"_test = [] {
      auto json = glz::write_json(diamond{}).value_or("error");
      expect(json == R"({"value":5,"l":6,"r":7,"d":8})") << json;

      diamond value{};
      expect(not glz::read_json(value, R"({"value":50,"l":60,"r":70,"d":80})"));
      expect(value.value == 50);
      expect(value.l == 60);
      expect(value.r == 70);
      expect(value.d == 80);
   };

   "private base of a non-aggregate is not reflected"_test = [] {
      auto json = glz::write_json(constructed{3, "c"}).value_or("error");
      expect(json == R"({"value":3,"label":"c"})") << json;

      constructed value{};
      expect(not glz::read_json(value, R"({"value":30,"label":"read"})"));
      expect(value.base_secret() == 9); // the base was not part of the payload
      expect(value.value == 30);
      expect(value.label == "read");
   };

   "public base contributes public members only"_test = [] {
      auto json = glz::write_json(derived_public_base_private_member{}).value_or("error");
      expect(json == R"({"visible":15,"own":17})") << json;
   };

   "enable_shared_from_this stays out of the payload"_test = [] {
      auto node = std::make_shared<shared_from_this_node>();
      auto json = glz::write_json(*node).value_or("error");
      expect(json == R"({"value":18})") << json;
   };

   "beve round trip"_test = [] {
      grandchild original{};
      original.id = 21;
      original.name = "beve";
      original.extra = 0.5;
      original.flag = false;
      std::string buffer{};
      expect(not glz::write_beve(original, buffer));

      grandchild value{};
      expect(not glz::read_beve(value, buffer));
      expect(value.id == 21);
      expect(value.name == "beve");
      expect(value.extra == 0.5);
      expect(not value.flag);
   };
};

int main() {}
