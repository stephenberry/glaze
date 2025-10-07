#include <deque>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct my_struct
{
   int i{};
   double d{};
   std::string hello{};
   std::array<uint64_t, 3> arr{};
};

static_assert(glz::reflectable<my_struct>);

static_assert(glz::name_v<my_struct> == "my_struct");

static_assert(glz::meta_has_skip<my_struct> == false);

struct modify_demo
{
   int x{};
   int y{};
};

template <>
struct glz::meta<modify_demo>
{
   static constexpr auto modify = glz::object(
      "renamed_x", &modify_demo::x, "x_alias", [](auto& self) -> auto& { return self.x; }, "alias_y",
      [](auto& self) -> auto& { return self.y; });
};

static_assert(glz::glaze_object_t<modify_demo>);
static_assert(!glz::reflectable<modify_demo>);

struct nested_entry
{
   int id{};
   std::string label{};
};

struct complex_modify
{
   std::vector<nested_entry> records{};
   std::map<std::string, int> metrics{};
   std::optional<int> flag{};
};

template <>
struct glz::meta<complex_modify>
{
   static constexpr auto modify = glz::object(
      "items", &complex_modify::records, "records_alias", [](auto& self) -> auto& { return self.records; },
      "statistics", &complex_modify::metrics, "metrics_alias", [](auto& self) -> auto& { return self.metrics; },
      "flag_status", &complex_modify::flag);
};

static_assert(glz::glaze_object_t<complex_modify>);

struct schema_modify_sample
{
   int value{};
   std::optional<std::string> note{};
};

template <>
struct glz::meta<schema_modify_sample>
{
   static constexpr auto modify = glz::object(
      "primary", &schema_modify_sample::value, "primary_alias", [](auto& self) -> auto& { return self.value; }, "note",
      &schema_modify_sample::note);
};

static_assert(glz::glaze_object_t<schema_modify_sample>);

struct modify_header
{
   std::string id{"id"};
   std::string type{"type"};
};

template <>
struct glz::meta<modify_header>
{
   static constexpr auto modify = glz::object(
      "identifier", &modify_header::id, "id_alias", [](auto& self) -> auto& { return self.id; }, "message_type",
      &modify_header::type);
};

static_assert(glz::glaze_object_t<modify_header>);

struct large_reflect_many
{
   int a{1};
   double b{2.5};
   std::string c{"three"};
   bool d{true};
   std::array<int, 3> e{1, 2, 3};
   std::optional<int> f{};
   float g{7.7f};
   long h{8};
};

template <>
struct glz::meta<large_reflect_many>
{
   static constexpr auto modify = glz::object(
      "alias_optional", [](auto& self) -> auto& { return self.f; }, "alias_float",
      [](auto& self) -> auto& { return self.g; });
};

static_assert(glz::glaze_object_t<large_reflect_many>);

struct server_status
{
   std::string name{};
   std::string region{};
   uint64_t active_sessions{};
   std::optional<std::string> maintenance{};
   double cpu_percent{};
};

template <>
struct glz::meta<server_status>
{
   static constexpr auto modify = glz::object(
      "maintenance_alias", [](auto& self) -> auto& { return self.maintenance; }, "cpuPercent",
      &server_status::cpu_percent);
};

static_assert(glz::glaze_object_t<server_status>);

struct test_skip
{};

template <>
struct glz::meta<test_skip>
{
   static constexpr bool skip(const std::string_view, const meta_context&) { return true; }
};

static_assert(glz::meta_has_skip<test_skip>);

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
      expect(not glz::write_json(obj, buffer));

      expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
   };

   "reflect_write prettify"_test = [] {
      std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
      my_struct obj{};
      expect(!glz::read_json(obj, buffer));

      buffer.clear();
      expect(not glz::write<glz::opts{.prettify = true}>(obj, buffer));

      expect(buffer == R"({
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [
      1,
      2,
      3
   ]
})");
   };
};

suite modify_reflection = [] {
   "modify rename and extend"_test = [] {
      modify_demo value{.x = 1, .y = 2};
      std::string buffer;
      expect(not glz::write_json(value, buffer));
      expect(buffer == std::string{R"({"renamed_x":1,"y":2,"x_alias":1,"alias_y":2})"}) << buffer;
   };

   "modify read alias"_test = [] {
      modify_demo value{};
      std::string buffer = R"({"renamed_x":5,"y":7,"x_alias":6,"alias_y":9})";
      expect(not glz::read_json(value, buffer));
      expect(value.x == 6);
      expect(value.y == 9);
   };

   "modify read alias only"_test = [] {
      modify_demo value{};
      std::string buffer = R"({"x_alias":15,"alias_y":25})";
      expect(not glz::read_json(value, buffer));
      expect(value.x == 15);
      expect(value.y == 25);
   };

   "modify rename preserves other names"_test = [] {
      modify_demo value{.x = 3, .y = 4};
      std::string buffer;
      expect(not glz::write_json(value, buffer));
      expect(buffer.find("\"y\"") != std::string::npos);
      expect(buffer.find("\"renamed_x\"") != std::string::npos);
      expect(buffer.find("\"x\"") == std::string::npos);
   };

   "modify alias overrides base"_test = [] {
      modify_demo value{};
      std::string buffer = R"({"renamed_x":2,"x_alias":9,"y":4,"alias_y":11})";
      expect(not glz::read_json(value, buffer));
      expect(value.x == 9);
      expect(value.y == 11);
   };

   "modify complex write"_test = [] {
      complex_modify value{};
      value.records = {{1, "one"}, {2, "two"}};
      value.metrics = {{"one", 1}, {"two", 2}};
      value.flag = 42;

      std::string buffer;
      expect(not glz::write_json(value, buffer));

      expect(buffer.find("\"items\"") != std::string::npos) << buffer;
      expect(buffer.find("\"records_alias\"") != std::string::npos);
      expect(buffer.find("\"statistics\"") != std::string::npos);
      expect(buffer.find("\"metrics_alias\"") != std::string::npos);
      expect(buffer.find("\"flag_status\"") != std::string::npos);
      expect(buffer.find("\"records\"") == std::string::npos);
      expect(buffer.find("\"metrics\"") == std::string::npos);
   };

   "modify complex read aliases"_test = [] {
      complex_modify value{};
      std::string buffer = R"({
         "records_alias": [{"id": 10, "label": "ten"}, {"id": 20, "label": "twenty"}],
         "metrics_alias": {"ten": 10, "twenty": 20},
         "flag_status": null
      })";

      expect(not glz::read_json(value, buffer));
      expect(value.records.size() == 2);
      expect(value.records[0].id == 10);
      expect(value.records[1].label == "twenty");
      expect(value.metrics.at("ten") == 10);
      expect(value.flag.has_value() == false);
   };

   "modify complex mixed keys"_test = [] {
      complex_modify value{};
      std::string buffer = R"({
         "items": [{"id": 1, "label": "one"}],
         "metrics_alias": {"alpha": 7},
         "flag_status": 99
      })";

      expect(not glz::read_json(value, buffer));
      expect(value.records.size() == 1);
      expect(value.records.front().label == "one");
      expect(value.metrics.at("alpha") == 7);
      expect(value.flag.value() == 99);
   };

   "modify large reflected add extras"_test = [] {
      large_reflect_many value{};
      value.f = 5;

      auto json = glz::write_json(value).value_or("error");
      expect(
         json ==
         R"({"a":1,"b":2.5,"c":"three","d":true,"e":[1,2,3],"f":5,"g":7.7,"h":8,"alias_optional":5,"alias_float":7.7})")
         << json;

      large_reflect_many roundtrip{};
      expect(!glz::read_json(roundtrip, json));
      expect(roundtrip.a == 1);
      expect(roundtrip.b == 2.5);
      expect(roundtrip.c == "three");
      expect(roundtrip.d == true);
      expect(roundtrip.e[2] == 3);
      expect(roundtrip.f.value() == 5);
      expect(roundtrip.g == 7.7f);
   };

   "modify realistic status"_test = [] {
      server_status value{
         .name = "edge-01",
         .region = "us-east",
         .active_sessions = 2412,
         .maintenance = std::string{"scheduled"},
         .cpu_percent = 73.5,
      };

      auto json = glz::write_json(value).value_or("error");
      expect(json.find(R"("name":"edge-01")") != std::string::npos) << json;
      expect(json.find(R"("region":"us-east")") != std::string::npos) << json;
      expect(json.find(R"("active_sessions":2412)") != std::string::npos) << json;
      expect(json.find(R"("maintenance":"scheduled")") != std::string::npos) << json;
      expect(json.find(R"("maintenance_alias":"scheduled")") != std::string::npos) << json;
      expect(json.find(R"("cpuPercent":73.5)") != std::string::npos) << json;
      expect(json.find(R"("cpu_percent")") == std::string::npos) << json;

      server_status roundtrip{};
      expect(!glz::read_json(roundtrip, json));
      expect(roundtrip.name == "edge-01");
      expect(roundtrip.region == "us-east");
      expect(roundtrip.active_sessions == 2412);
      expect(roundtrip.maintenance.value() == "scheduled");
      expect(roundtrip.cpu_percent == 73.5);
   };
};

suite modify_json_schema = [] {
   "schema includes modify entries"_test = [] {
      const auto schema = glz::write_json_schema<schema_modify_sample>().value_or("error");
      expect(schema.find(R"("primary")") != std::string::npos) << schema;
      expect(schema.find(R"("primary_alias")") != std::string::npos) << schema;
      expect(schema.find(R"("value")") == std::string::npos) << schema;
      expect(schema.find(R"("note")") != std::string::npos) << schema;
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

static_assert(glz::reflectable<nested_t>);

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
      expect(not glz::write_json(obj, buffer));

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

struct thing_wrapper
{
   Thing thing{};

   struct glaze
   {
      static constexpr auto value{&thing_wrapper::thing};
   };
};

suite user_types = [] {
   "complex user obect"_test = [] {
      Thing obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":{"x":3.14,"y":2.7,"z":6.5},"array":["as\"df\\ghjkl","pie","42","foo"],"vector":[{"x":9,"y":6.7,"z":3.1},{"x":3.14,"y":2.7,"z":6.5}],"i":8,"d":2,"b":false,"c":"W","color":"Green","vb":[true,false,false,true,true,true,true],"thing_ptr":{"a":3.14,"b":"stuff"},"map":{"eleven":11,"twelve":12}})")
         << buffer;

      buffer.clear();
      expect(not glz::write<glz::opts{.skip_null_members = false}>(obj, buffer));
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
      expect(glz::seek([&](auto& value) { std::ignore = glz::write_json(value, out); }, obj, "/d"));

      expect(out == "2");

      expect(glz::seek([&](auto& value) { std::ignore = glz::write_json(value, out); }, obj, "/thing_ptr/b"));

      expect(out == R"("stuff")");
   };

#if ((defined _MSC_VER) && (!defined __clang__))
   // The "thing_wrapper seek" test is broken in MSVC, because MSVC has internal compiler errors for seeking
   // glaze_value_t Uncomment this when MSVC is fixed
#else
   "thing_wrapper seek"_test = [] {
      thing_wrapper obj{};
      std::string out;
      expect(glz::seek([&](auto& value) { std::ignore = glz::write_json(value, out); }, obj, "/thing_ptr/b"));

      expect(out == R"("stuff")");
   };
#endif
};

struct single_t
{
   int integer{};
};

suite single_test = [] {
   "single_t"_test = [] {
      single_t obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

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
      expect(not glz::write_json(obj, buffer));

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
      expect(not glz::write_json(obj, buffer));

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
      expect(not glz::write_json(const_obj, s));
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
      std::string schema = glz::write_json_schema<Thing>().value_or("error");
      // Note: Check schema and sample output against a json schema validator like https://www.jsonschemavalidator.net/
      // when you update this string
      expect(
         schema ==
         R"({"type":["object"],"properties":{"array":{"$ref":"#/$defs/std::array<std::string,4>"},"b":{"$ref":"#/$defs/bool"},"c":{"$ref":"#/$defs/char"},"color":{"$ref":"#/$defs/Color"},"d":{"$ref":"#/$defs/double"},"i":{"$ref":"#/$defs/int32_t"},"map":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"optional":{"$ref":"#/$defs/std::optional<V3>"},"thing":{"$ref":"#/$defs/sub_thing"},"thing2array":{"$ref":"#/$defs/std::array<sub_thing2,1>"},"thing_ptr":{"$ref":"#/$defs/sub_thing*"},"vb":{"$ref":"#/$defs/std::vector<bool>"},"vec3":{"$ref":"#/$defs/V3"},"vector":{"$ref":"#/$defs/std::vector<V3>"}},"additionalProperties":false,"$defs":{"Color":{"type":["string"],"oneOf":[{"title":"Red","const":"Red"},{"title":"Green","const":"Green"},{"title":"Blue","const":"Blue"}]},"V3":{"type":["object"],"properties":{"x":{"$ref":"#/$defs/double"},"y":{"$ref":"#/$defs/double"},"z":{"$ref":"#/$defs/double"}},"additionalProperties":false},"bool":{"type":["boolean"]},"char":{"type":["string"]},"double":{"type":["number"],"minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"float":{"type":["number"],"minimum":-3.4028234663852886E38,"maximum":3.4028234663852886E38},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::array<std::string,4>":{"type":["array"],"items":{"$ref":"#/$defs/std::string"},"minItems":4,"maxItems":4},"std::array<sub_thing2,1>":{"type":["array"],"items":{"$ref":"#/$defs/sub_thing2"},"minItems":1,"maxItems":1},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::optional<V3>":{"type":["object","null"],"properties":{"x":{"$ref":"#/$defs/double"},"y":{"$ref":"#/$defs/double"},"z":{"$ref":"#/$defs/double"}},"additionalProperties":false},"std::string":{"type":["string"]},"std::vector<V3>":{"type":["array"],"items":{"$ref":"#/$defs/V3"}},"std::vector<bool>":{"type":["array"],"items":{"$ref":"#/$defs/bool"}},"sub_thing":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"}},"additionalProperties":false},"sub_thing*":{"type":["object","null"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"}},"additionalProperties":false},"sub_thing2":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"},"c":{"$ref":"#/$defs/double"},"d":{"$ref":"#/$defs/double"},"e":{"$ref":"#/$defs/double"},"f":{"$ref":"#/$defs/float"},"g":{"$ref":"#/$defs/double"},"h":{"$ref":"#/$defs/double"}},"additionalProperties":false}},"title":"Thing"})")
         << schema;
   };
};

struct empty_t
{};

static_assert(glz::reflect<empty_t>::size == 0);
static_assert(not glz::maybe_skipped<glz::opts{}, empty_t>);

suite empty_test = [] {
   "empty_t"_test = [] {
      empty_t obj;
      expect(glz::write_json(obj) == "{}");
      expect(!glz::read_json(obj, "{}"));
   };
};

struct V2
{
   float x{};
   float y{};

   V2(glz::make_reflectable) {}
   V2() = default;

   V2(V2&&) = default;
   V2(const float* arr) : x(arr[0]), y(arr[1]) {}
};

template <>
struct glz::meta<V2>
{
   static constexpr auto value = object(&V2::x, &V2::y);
};

struct V2Wrapper
{
   V2 x{};
};

static_assert(glz::reflectable<V2Wrapper>);
static_assert(glz::detail::count_members<V2Wrapper> == 1);

suite v2_wrapper_test = [] {
   "v2_wrapper"_test = [] {
      V2Wrapper obj;
      auto s = glz::write_json(obj).value_or("error");
      expect(s == R"({"x":{"x":0,"y":0}})") << s;
   };
};

struct port_struct
{
   int port{};
};

suite prefix_key_name_test = [] {
   "prefix_key_name"_test = [] {
      port_struct obj;
      std::string buffer = R"({"portmanteau":14,"port":17})";
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
      expect(!err) << glz::format_error(err, buffer);
   };
};

struct meta_schema_t
{
   int x{};
   std::string file_name{};
   bool is_valid{};
};

template <>
struct glz::json_schema<meta_schema_t>
{
   schema x{.description = "x is a special integer", .minimum = 1};
   schema file_name{.description = "provide a file name to load"};
   schema is_valid{.description = "for validation"};
};

static_assert(glz::json_schema_t<glz::json_schema<meta_schema_t>>);
static_assert(glz::reflectable<glz::json_schema<meta_schema_t>>);
static_assert(glz::detail::count_members<glz::json_schema<meta_schema_t>> == 3);

struct local_schema_t
{
   int x{};
   std::string file_name{};
   bool is_valid{};

   struct glaze_json_schema
   {
      glz::schema x{.description = "x is a special integer", .minimum = 1};
      glz::schema file_name{.description = "provide a file name to load"};
      glz::schema is_valid{.description = "for validation"};
   };
};

static_assert(glz::local_json_schema_t<local_schema_t>);
static_assert(glz::json_schema_t<local_schema_t>);

suite meta_schema_reflection_tests = [] {
   "meta_schema_reflection"_test = [] {
      meta_schema_t obj;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"x":0,"file_name":"","is_valid":false})") << buffer;

      const auto json_schema = glz::write_json_schema<meta_schema_t>().value_or("error");
      expect(
         json_schema ==
         R"({"type":["object"],"properties":{"file_name":{"$ref":"#/$defs/std::string","description":"provide a file name to load"},"is_valid":{"$ref":"#/$defs/bool","description":"for validation"},"x":{"$ref":"#/$defs/int32_t","description":"x is a special integer","minimum":1}},"additionalProperties":false,"$defs":{"bool":{"type":["boolean"]},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::string":{"type":["string"]}},"title":"meta_schema_t"})")
         << json_schema;
   };

   "local_schema"_test = [] {
      local_schema_t obj;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"x":0,"file_name":"","is_valid":false})") << buffer;

      const auto json_schema = glz::write_json_schema<local_schema_t>().value_or("error");
      expect(
         json_schema ==
         R"({"type":["object"],"properties":{"file_name":{"$ref":"#/$defs/std::string","description":"provide a file name to load"},"is_valid":{"$ref":"#/$defs/bool","description":"for validation"},"x":{"$ref":"#/$defs/int32_t","description":"x is a special integer","minimum":1}},"additionalProperties":false,"$defs":{"bool":{"type":["boolean"]},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::string":{"type":["string"]}},"title":"local_schema_t"})")
         << json_schema;
   };
};

struct animals_t
{
   std::string lion = "Lion";
   std::string tiger = "Tiger";
   std::string panda = "Panda";
};

struct zoo_t
{
   animals_t animals{};
   std::string name{"My Awesome Zoo"};
};

suite partial_write_tests = [] {
   "partial write"_test = [] {
      static constexpr auto partial = glz::json_ptrs("/name", "/animals/tiger");

      zoo_t obj{};
      std::string s{};
      const auto ec = glz::write_json<partial>(obj, s);
      expect(!ec);
      expect(s == R"({"animals":{"tiger":"Tiger"},"name":"My Awesome Zoo"})") << s;
   };

   "partial write with modify"_test = [] {
      static constexpr auto partial = glz::json_ptrs("/identifier", "/id_alias");

      modify_header header{};
      header.id = "101";
      header.type = "greeting";

      std::string buffer;
      const auto ec = glz::write_json<partial>(header, buffer);
      expect(!ec);
      expect(buffer == R"({"id_alias":"101","identifier":"101"})") << buffer;
   };
};

suite modify_partial_read = [] {
   static constexpr glz::opts partial_read_opts{.partial_read = true};

   "partial read with modify keys"_test = [] {
      modify_header header{};
      const std::string json = R"({"identifier":"abc","message_type":"notice"})";
      expect(!glz::read<partial_read_opts>(header, json));
      expect(header.id == "abc");
      expect(header.type == "notice");
   };

   "partial read modify alias unknown key"_test = [] {
      modify_header header{};
      const std::string json = R"({"id_alias":"xyz","type":"legacy","identifier":"def"})";
      expect(glz::read<glz::opts{.partial_read = true}>(header, json) == glz::error_code::unknown_key);
      expect(header.id == "xyz");
      expect(header.type == "type");
   };

   "partial read modify ignore unknown"_test = [] {
      modify_header header{};
      const std::string json = R"({"id_alias":"xyz","type":"legacy","identifier":"def"})";
      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(header, json));
      expect(header.id == "def") << header.id;
      expect(header.type == "type") << header.type;
   };
};

struct modify_unknown
{
   int value{};
   int extra{};
};

template <>
struct glz::meta<modify_unknown>
{
   static constexpr auto modify = glz::object("renamed_value", &modify_unknown::value, "extra", &modify_unknown::extra);
};

suite modify_unknown_keys = [] {
   "unknown original key triggers error"_test = [] {
      modify_unknown target{};
      const std::string json = R"({"value":1,"renamed_value":2,"extra":3})";
      expect(glz::read<glz::opts{.error_on_unknown_keys = true}>(target, json) == glz::error_code::unknown_key);
      expect(target.value == 0);
      expect(target.extra == 0);
   };

   "unknown original key ignored when allowed"_test = [] {
      modify_unknown target{};
      const std::string json = R"({"value":1,"renamed_value":2,"extra":3})";
      expect(!glz::read<glz::opts{.error_on_unknown_keys = false}>(target, json));
      expect(target.value == 2);
      expect(target.extra == 3);
   };
};

struct empty_optional_t
{
   std::string value{};
   std::optional<uint64_t> opt{};
};

suite empty_optional_tests = [] {
   "empty_optional_t"_test = [] {
      empty_optional_t obj{};
      expect(glz::write_json(obj) == R"({"value":""})");
   };
};

struct target_t
{
   std::optional<std::string> label{"label_optional"};
   std::string name{"name_string"};
   std::vector<int> ints{};
};

struct nested_target_t
{
   target_t target{};
   std::string test{"test"};
};

suite nested_target_tests = [] {
   "nested_target"_test = [] {
      nested_target_t obj{};
      auto buffer = glz::write_json(obj).value_or("error");
      expect(buffer == R"({"target":{"label":"label_optional","name":"name_string","ints":[]},"test":"test"})")
         << buffer;
      expect(!glz::read_json(obj, buffer));
   };
};

struct large_struct_t
{
   bool a = false;
   bool b = false;
   bool c = false;
   bool d = false;
   bool e = false;
   bool f = false;
   bool g = false;
   bool h = false;
   bool i = false;
   bool j = false;
   bool k = false;
   bool l = false;
   bool m = false;
   bool n = false;
   bool o = false;
   bool p = false;
   bool q = false;
   bool r = false;
   bool s = false;
   bool t = false;
   bool u = false;
   bool v = false;
   bool w = false;
   bool x = false;
   bool y = false;
   bool z = false;
   bool one = false;
   bool two = false;
   bool three = false;
   bool four = false;
   bool five = false;
   bool six = false;
   bool seven = false;
};

suite large_struct_tests = [] {
   "large_struct"_test = [] {
      large_struct_t obj{};
      std::string s = glz::write_json(obj).value_or("error");
      expect(
         s ==
         R"({"a":false,"b":false,"c":false,"d":false,"e":false,"f":false,"g":false,"h":false,"i":false,"j":false,"k":false,"l":false,"m":false,"n":false,"o":false,"p":false,"q":false,"r":false,"s":false,"t":false,"u":false,"v":false,"w":false,"x":false,"y":false,"z":false,"one":false,"two":false,"three":false,"four":false,"five":false,"six":false,"seven":false})")
         << s;
      expect(not glz::read_json(obj, s));
   };
};

namespace glz
{
   template <>
   struct from<JSON, std::chrono::seconds>
   {
      template <auto Opts>
      static void op(std::chrono::seconds& value, is_context auto&& ctx, auto&&... args)
      {
         int32_t sec_count{};
         parse<JSON>::op<Opts>(sec_count, ctx, args...);
         if (glz::error_code::none == ctx.error) value = std::chrono::seconds{sec_count};
      }
   };
}

struct chrono_data
{
   std::string message{};
   std::chrono::seconds seconds_duration{};
};

suite custom_chrono_tests = [] {
   "custom_chrono"_test = [] {
      constexpr std::string_view json = R"(
         {
            "message": "Hello",
            "seconds_duration": 5458
         }
      )";

      chrono_data obj{};
      expect(not glz::read_json(obj, json));

      expect(obj.message == "Hello");
      expect(obj.seconds_duration.count() == 5458);
   };
};

struct S1
{
   int a{};
   int b{};
   std::filesystem::path fn{};
};
static_assert(glz::detail::count_members<S1> == 3);

struct unique_index_t
{
   int apple{};
   int archer{};
   int arm{};
   int amiable{};
};

suite unique_index_test = [] {
   "unique_index"_test = [] {
      unique_index_t obj{};
      std::string buffer = R"({"apple":1,"archer":2,"arm":3,"amiable":4})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.apple == 1);
      expect(obj.archer == 2);
      expect(obj.arm == 3);
      expect(obj.amiable == 4);
   };
};

struct single_element_t
{
   int here_is_a_lonely_element{};
};

struct full_hash_t
{
   int collide{};
   int collide2{};
   int colllide{};
   int colilide{};
   int coiilide{};
};

struct front_32_t
{
   int aaaa{};
   int aaab{};
   int aaba{};
   int bbbb{};
   int aabb{};
};

struct front_64_t
{
   int aaaaaaaa{};
   int aaaaaaaz{};
   int aaaaaaza{};
   int zzzzzzzz{};
   int aaaaaazz{};
};

struct three_element_unique_t
{
   int aaaaaaaa{};
   int aaaaaaab{};
   int aaaaaabc{};
};

suite hash_tests = [] {
   "single_element"_test = [] {
      single_element_t obj{};
      std::string_view buffer = R"({"here_is_a_lonely_element":42})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.here_is_a_lonely_element == 42);
   };

   "full_hash"_test = [] {
      full_hash_t obj{};
      std::string_view buffer = R"({"collide":1,"collide2":2})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.collide == 1);
      expect(obj.collide2 == 2);
   };

   "front_32"_test = [] {
      front_32_t obj{};
      std::string_view buffer = R"({"aaaa":1,"aaab":2,"aaba":3})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.aaaa == 1);
      expect(obj.aaab == 2);
      expect(obj.aaba == 3);
   };

   "front_64"_test = [] {
      glz::keys_info_t info{.min_length = 8, .max_length = 8};
      [[maybe_unused]] const auto valid = glz::front_bytes_hash_info<uint64_t>(glz::reflect<front_64_t>::keys, info);

      front_64_t obj{};
      std::string_view buffer = R"({"aaaaaaaa":1,"aaaaaaaz":2,"aaaaaaza":3})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.aaaaaaaa == 1);
      expect(obj.aaaaaaaz == 2);
      expect(obj.aaaaaaza == 3);
   };

   "front_32"_test = [] {
      three_element_unique_t obj{};
      std::string_view buffer = R"({"aaaaaaaa":1,"aaaaaaab":2,"aaaaaabc":3})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.aaaaaaaa == 1);
      expect(obj.aaaaaaab == 2);
      expect(obj.aaaaaabc == 3);
   };
};

struct custom_state
{
   std::array<uint32_t, 8> statuses() { return {}; }
};

template <>
struct glz::meta<custom_state>
{
   using T = custom_state;
   static constexpr auto read = [](T&, const std::array<uint32_t, 8>&) {};
   static constexpr auto value = custom<read, &T::statuses>;
};

struct custom_holder
{
   uint32_t x{};
   uint32_t y{};
   uint32_t z{};
   custom_state state{};
};

suite custom_holder_tests = [] {
   "custom_holder"_test = [] {
      custom_holder obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(not glz::read_json(obj, buffer));
   };

   "custom_holder seek"_test = [] {
      custom_holder obj{};
      std::string buffer{};
      bool b = glz::seek([&](auto&& val) { std::ignore = glz::write_json(val, buffer); }, obj, "/state");
      expect(b);
      expect(buffer == "[0,0,0,0,0,0,0,0]") << buffer;
   };
};

enum struct some_enum { one, two, three };

template <>
struct glz::meta<some_enum>
{
   using enum some_enum;
   static constexpr auto value = enumerate(one, two, three);
};

struct struct_with_a_pair
{
   std::pair<some_enum, std::string> value{};

   constexpr auto operator<=>(const struct_with_a_pair& rhs) const = default;
};

suite enum_pair_tests = [] {
   "enum pair"_test = [] {
      struct_with_a_pair obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"value":{"one":""}})") << buffer;

      buffer = R"({"value":{"two":"message"}})";

      expect(not glz::read_json(obj, buffer));
      expect(obj.value.first == some_enum::two);
      expect(obj.value.second == "message");
   };
};

struct renamed_t
{
   std::string first_name{};
   std::string last_name{};
   int age{};
};

template <>
struct glz::meta<renamed_t>
{
   static constexpr std::string_view rename_key(const std::string_view key)
   {
      if (key == "first_name") {
         return "firstName";
      }
      else if (key == "last_name") {
         return "lastName";
      }
      return key;
   }
};

// This example shows how we use dynamic memory at compile time for string transformations
struct suffixed_keys_t
{
   std::string first{};
   std::string last{};
};

template <>
struct glz::meta<suffixed_keys_t>
{
   static constexpr std::string rename_key(const auto key) { return std::string(key) + "_name"; }
};

struct rename_with_modify
{
   int first{};
   int second{};
};

template <>
struct glz::meta<rename_with_modify>
{
   static constexpr std::string_view rename_key(const std::string_view key)
   {
      if (key == "first") {
         return "firstRenamed";
      }
      else if (key == "second") {
         return "secondRenamed";
      }
      return key;
   }

   static constexpr auto modify = glz::object("first_alias", &rename_with_modify::first);
};

suite rename_tests = [] {
   "rename"_test = [] {
      renamed_t obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"firstName":"","lastName":"","age":0})") << buffer;

      buffer = R"({"firstName":"Kira","lastName":"Song","age":29})";

      expect(not glz::read_json(obj, buffer));
      expect(obj.first_name == "Kira");
      expect(obj.last_name == "Song");
      expect(obj.age == 29);
   };

   "suffixed keys"_test = [] {
      suffixed_keys_t obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"first_name":"","last_name":""})") << buffer;

      buffer = R"({"first_name":"Kira","last_name":"Song"})";

      expect(not glz::read_json(obj, buffer));
      expect(obj.first == "Kira");
      expect(obj.last == "Song");
   };

   "rename with modify"_test = [] {
      rename_with_modify obj{.first = 7, .second = 8};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"firstRenamed":7,"secondRenamed":8,"first_alias":7})") << buffer;

      buffer = R"({"firstRenamed":3,"secondRenamed":4})";

      expect(not glz::read_json(obj, buffer));
      expect(obj.first == 3);
      expect(obj.second == 4);

      buffer = R"({"first_alias":11,"secondRenamed":12})";

      expect(not glz::read_json(obj, buffer));
      expect(obj.first == 11);
      expect(obj.second == 12);
   };
};

int main() { return 0; }
