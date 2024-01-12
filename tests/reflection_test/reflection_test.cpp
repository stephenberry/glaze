#include <deque>
#include <map>
#include <unordered_map>

#include "boost/ut.hpp"
#include "glaze/glaze.hpp"

using namespace boost::ut;

struct my_struct
{
   int i{};
   double d{};
   std::string hello{};
   std::array<uint64_t, 3> arr{};
};

static_assert(glz::detail::reflectable<my_struct>);

static_assert(glz::name_v<my_struct> == "my_struct");

suite reflection = [] {
   "reflect_write"_test = [] {
      std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
      my_struct obj{};
      expect(!glz::read_json(obj, buffer));

      expect(obj.i == 287);
      expect(obj.d == 3.14);
      expect(obj.hello == "Hello World");
      expect(obj.arr == std::array<uint64_t, 3>{1, 2, 3});

      buffer.clear();
      glz::write_json(obj, buffer);

      expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
   };
};

struct non_default_t
{
   non_default_t(int) {}
};

struct nested_t
{
   std::optional<std::string> str{};
   my_struct thing{};
};

static_assert(glz::detail::reflectable<nested_t>);

#ifndef _MSC_VER
suite nested_reflection = [] {
   "nested_reflection"_test = [] {
      std::string buffer = R"({"thing":{"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]},"str":"reflection"})";
      nested_t obj{};
      expect(!glz::read_json(obj, buffer));

      expect(obj.thing.i == 287);
      expect(obj.thing.d == 3.14);
      expect(obj.thing.hello == "Hello World");
      expect(obj.thing.arr == std::array<uint64_t, 3>{1, 2, 3});

      buffer.clear();
      glz::write_json(obj, buffer);

      expect(buffer == R"({"str":"reflection","thing":{"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]}})")
         << buffer;
   };
};
#endif

struct sub_thing
{
   double a{3.14};
   std::string b{"stuff"};
};

struct sub_thing2
{
   double a{3.14};
   std::string b{"stuff"};
   double c{999.342494903};
   double d{0.000000000001};
   double e{203082348402.1};
   float f{89.089f};
   double g{12380.00000013};
   double h{1000000.000001};
};

struct V3
{
   double x{3.14};
   double y{2.7};
   double z{6.5};

   bool operator==(const V3& rhs) const { return (x == rhs.x) && (y == rhs.y) && (z == rhs.z); }
};

enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color>
{
   static constexpr auto value = enumerate(Color::Red, //
                                           Color::Green, //
                                           Color::Blue //
   );
};

struct var1_t
{
   double x{};
};

struct var2_t
{
   double y{};
};

struct Thing
{
   sub_thing thing{};
   std::array<sub_thing2, 1> thing2array{};
   V3 vec3{};
   std::array<std::string, 4> array = {"as\"df\\ghjkl", "pie", "42", "foo"};
   std::vector<V3> vector = {{9.0, 6.7, 3.1}, {}};
   int i{8};
   double d{2};
   bool b{};
   char c{'W'};
   Color color{Color::Green};
   std::vector<bool> vb = {true, false, false, true, true, true, true};
   std::optional<V3> optional{};
   sub_thing* thing_ptr{&thing};
   std::map<std::string, int> map{{"eleven", 11}, {"twelve", 12}};
};

suite user_types = [] {
   "complex user obect"_test = [] {
      Thing obj{};
      std::string buffer{};
      glz::write_json(obj, buffer);
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":{"x":3.14,"y":2.7,"z":6.5},"array":["as\"df\\ghjkl","pie","42","foo"],"vector":[{"x":9,"y":6.7,"z":3.1},{"x":3.14,"y":2.7,"z":6.5}],"i":8,"d":2,"b":false,"c":"W","color":"Green","vb":[true,false,false,true,true,true,true],"thing_ptr":{"a":3.14,"b":"stuff"},"map":{"eleven":11,"twelve":12}})")
         << buffer;

      buffer.clear();
      glz::write<glz::opts{.skip_null_members = false}>(obj, buffer);
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":{"x":3.14,"y":2.7,"z":6.5},"array":["as\"df\\ghjkl","pie","42","foo"],"vector":[{"x":9,"y":6.7,"z":3.1},{"x":3.14,"y":2.7,"z":6.5}],"i":8,"d":2,"b":false,"c":"W","color":"Green","vb":[true,false,false,true,true,true,true],"optional":null,"thing_ptr":{"a":3.14,"b":"stuff"},"map":{"eleven":11,"twelve":12}})")
         << buffer;

      expect(!glz::read_json(obj, buffer));
   };

   "complex user obect get"_test = [] {
      Thing obj{};
      auto i = glz::get<int>(obj, "/i");
      expect(i.has_value());
      if (i.has_value()) {
         expect(i.value() == 8);
      }

      auto array = glz::get<std::array<std::string, 4>>(obj, "/array");
      expect(array.has_value());
      if (array.has_value()) {
         expect(array.value().get()[1] == "pie");
      }

      auto b = glz::get<std::string>(obj, "/thing_ptr/b");
      expect(b.has_value());
      if (b.has_value()) {
         expect(b.value().get() == "stuff");
      }

      std::string out;
      expect(glz::seek([&](auto& value) { glz::write_json(value, out); }, obj, "/d"));

      expect(out == "2");

      expect(glz::seek([&](auto& value) { glz::write_json(value, out); }, obj, "/thing_ptr/b"));

      expect(out == R"("stuff")");
   };
};

struct single_t
{
   int integer{};
};

suite single_test = [] {
   "single_t"_test = [] {
      single_t obj{};
      std::string buffer{};
      glz::write_json(obj, buffer);

      expect(!glz::read_json(obj, buffer));
   };
};

struct two_elements_t
{
   int integer0{};
   int integer1{};
};

suite two_elements_test = [] {
   "two_elements_t"_test = [] {
      two_elements_t obj{};
      std::string buffer{};
      glz::write_json(obj, buffer);

      expect(!glz::read_json(obj, buffer));
   };
};

struct string_view_member_count
{
   int one{};
   int two{};
   std::string_view three{};
   int four{};
   int five{};
};

static_assert(glz::detail::count_members<string_view_member_count> == 5);

namespace testing
{

   enum Q {
      A1 = 0,
      A2 = 1,
   };

   enum B {
      B1 = 0,
      B2 = 1,
   };

   struct V
   {
      Q v1;
      uint8_t v2;
      B v3;
      uint64_t v4;
      uint8_t v5;
      std::vector<uint8_t> v6;
   };

   struct VS
   {
      uint16_t w;
      uint16_t h;
      uint8_t f;
   };

   struct VC
   {
      std::string c;
      bool l;
      bool s;
      uint8_t sn;
      std::string sid;
      uint64_t time;
      uint8_t p;
      uint64_t age;
      uint32_t gs;
      VS srs;
      std::map<uint8_t, V> layers;
   };

   struct A
   {
      uint64_t b;
      std::vector<uint8_t> e;
   };

   struct ASS
   {
      uint32_t sr;
      uint8_t cc;
   };

   struct AC
   {
      std::string c;
      bool m;
      bool s;
      uint8_t sn;
      std::string sid;
      uint64_t time;
      uint8_t p;
      uint64_t age;
      ASS srs;
      std::map<uint8_t, A> layers;
   };

   struct C
   {
      bool a;
      std::variant<VC, AC> Config;
   };

   struct UD
   {
      std::string id;
      std::string n;
      std::string e;
      std::string aid;
      uint64_t o;
      bool ob;
      std::string ri;
      std::map<uint8_t, VC> v;
      std::map<uint8_t, AC> a;
   };

};

template <>
struct glz::meta<testing::Q>
{
   using enum testing::Q;
   static constexpr auto value = enumerate("0", testing::A1, "1", testing::A2);
};

template <>
struct glz::meta<testing::B>
{
   using enum testing::B;
   static constexpr auto value = enumerate("0", testing::B1, "1", testing::B2);
};

suite testing_structures = [] {
   "testing_structures"_test = [] {
      testing::UD obj{};
      std::string buffer{};
      glz::write_json(obj, buffer);

      expect(!glz::read_json(obj, buffer));
   };
};

struct structure_t
{
   std::string doc;
   std::string id;
};

suite const_object_test = [] {
   "const_object"_test = [] {
      std::string buffer = R"({"doc":"aaa","id":"1111"})";
      structure_t obj{};

      expect(!glz::read_json(obj, buffer));

      const structure_t const_obj = obj;

      static_assert(std::is_const_v<std::remove_reference_t<decltype(const_obj)>>);
      std::string s{};
      glz::write_json(const_obj, s);
      expect(buffer == s);
   };
};

struct user
{
   std::string name;
   std::string email;
   int age;
};

suite error_on_missing_keys_test = [] {
   "error_on_missing_keys"_test = [] {
      constexpr std::string_view json = R"({"email":"test@email.com","age":20})";
      constexpr glz::opts options = {.error_on_missing_keys = true};

      user obj = {};
      const auto ec = glz::read<options>(obj, json);
      expect(ec != glz::error_code::none);
   };

   "success"_test = [] {
      constexpr std::string_view json = R"({"email":"test@email.com","age":20,"name":"Fred"})";
      constexpr glz::opts options = {.error_on_missing_keys = true};

      user obj = {};
      expect(!glz::read<options>(obj, json));
   };
};

suite json_schema = [] {
   "json schema"_test = [] {
      Thing obj{};
      std::string schema = glz::write_json_schema<Thing>();
      // Note: Check schema and sample output against a json schema validator like https://www.jsonschemavalidator.net/
      // when you update this string
      expect(
         schema ==
         R"({"type":["object"],"properties":{"array":{"$ref":"#/$defs/std::array<std::string,4>"},"b":{"$ref":"#/$defs/bool"},"c":{"$ref":"#/$defs/char"},"color":{"$ref":"#/$defs/Color"},"d":{"$ref":"#/$defs/double"},"i":{"$ref":"#/$defs/int32_t"},"map":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"optional":{"$ref":"#/$defs/std::optional<V3>"},"thing":{"$ref":"#/$defs/sub_thing"},"thing2array":{"$ref":"#/$defs/std::array<sub_thing2,1>"},"thing_ptr":{"$ref":"#/$defs/sub_thing"},"vb":{"$ref":"#/$defs/std::vector<bool>"},"vec3":{"$ref":"#/$defs/V3"},"vector":{"$ref":"#/$defs/std::vector<V3>"}},"additionalProperties":false,"$defs":{"Color":{"type":["string"],"oneOf":[{"const":"Red"},{"const":"Green"},{"const":"Blue"}]},"V3":{"type":["object"],"properties":{"x":{"$ref":"#/$defs/double"},"y":{"$ref":"#/$defs/double"},"z":{"$ref":"#/$defs/double"}},"additionalProperties":false},"bool":{"type":["boolean"]},"char":{"type":["string"]},"double":{"type":["number"]},"float":{"type":["number"]},"int32_t":{"type":["integer"]},"std::array<std::string,4>":{"type":["array"],"items":{"$ref":"#/$defs/std::string"}},"std::array<sub_thing2,1>":{"type":["array"],"items":{"$ref":"#/$defs/sub_thing2"}},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::optional<V3>":{"type":["object","null"],"properties":{"x":{"$ref":"#/$defs/double"},"y":{"$ref":"#/$defs/double"},"z":{"$ref":"#/$defs/double"}},"additionalProperties":false},"std::string":{"type":["string"]},"std::vector<V3>":{"type":["array"],"items":{"$ref":"#/$defs/V3"}},"std::vector<bool>":{"type":["array"],"items":{"$ref":"#/$defs/bool"}},"sub_thing":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"}},"additionalProperties":false},"sub_thing2":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"},"c":{"$ref":"#/$defs/double"},"d":{"$ref":"#/$defs/double"},"e":{"$ref":"#/$defs/double"},"f":{"$ref":"#/$defs/float"},"g":{"$ref":"#/$defs/double"},"h":{"$ref":"#/$defs/double"}},"additionalProperties":false}}})")
         << schema;
   };
};

struct empty_t
{};

suite empty_test = [] {
   "empty_t"_test = [] {
      empty_t obj;
      expect(glz::write_json(obj) == "{}");
      expect(!glz::read_json(obj, "{}"));
   };
};

int main()
{ // Explicitly run registered test suites and report errors
   // This prevents potential issues with thread local variables
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
