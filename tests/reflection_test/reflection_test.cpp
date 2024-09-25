#define UT_RUN_TIME_ONLY

#include <deque>
#include <map>
#include <unordered_map>

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
         R"({"type":["object"],"properties":{"array":{"$ref":"#/$defs/std::array<std::string,4>"},"b":{"$ref":"#/$defs/bool"},"c":{"$ref":"#/$defs/char"},"color":{"$ref":"#/$defs/Color"},"d":{"$ref":"#/$defs/double"},"i":{"$ref":"#/$defs/int32_t"},"map":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"optional":{"$ref":"#/$defs/std::optional<V3>"},"thing":{"$ref":"#/$defs/sub_thing"},"thing2array":{"$ref":"#/$defs/std::array<sub_thing2,1>"},"thing_ptr":{"$ref":"#/$defs/sub_thing"},"vb":{"$ref":"#/$defs/std::vector<bool>"},"vec3":{"$ref":"#/$defs/V3"},"vector":{"$ref":"#/$defs/std::vector<V3>"}},"additionalProperties":false,"$defs":{"Color":{"type":["string"],"oneOf":[{"title":"Red","const":"Red"},{"title":"Green","const":"Green"},{"title":"Blue","const":"Blue"}]},"V3":{"type":["object"],"properties":{"x":{"$ref":"#/$defs/double"},"y":{"$ref":"#/$defs/double"},"z":{"$ref":"#/$defs/double"}},"additionalProperties":false},"bool":{"type":["boolean"]},"char":{"type":["string"]},"double":{"type":["number"],"minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"float":{"type":["number"],"minimum":-3.4028234663852886E38,"maximum":3.4028234663852886E38},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::array<std::string,4>":{"type":["array"],"items":{"$ref":"#/$defs/std::string"},"minItems":4,"maxItems":4},"std::array<sub_thing2,1>":{"type":["array"],"items":{"$ref":"#/$defs/sub_thing2"},"minItems":1,"maxItems":1},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::optional<V3>":{"type":["object","null"],"properties":{"x":{"$ref":"#/$defs/double"},"y":{"$ref":"#/$defs/double"},"z":{"$ref":"#/$defs/double"}},"additionalProperties":false},"std::string":{"type":["string"]},"std::vector<V3>":{"type":["array"],"items":{"$ref":"#/$defs/V3"}},"std::vector<bool>":{"type":["array"],"items":{"$ref":"#/$defs/bool"}},"sub_thing":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"}},"additionalProperties":false},"sub_thing2":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"},"c":{"$ref":"#/$defs/double"},"d":{"$ref":"#/$defs/double"},"e":{"$ref":"#/$defs/double"},"f":{"$ref":"#/$defs/float"},"g":{"$ref":"#/$defs/double"},"h":{"$ref":"#/$defs/double"}},"additionalProperties":false}}})")
         << schema;
   };
};

struct empty_t
{};

static_assert(glz::reflect<empty_t>::size == 0);
static_assert(not glz::object_info<glz::opts{}, empty_t>::first_will_be_written);
static_assert(not glz::object_info<glz::opts{}, empty_t>::maybe_skipped);

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

static_assert(glz::detail::reflectable<V2Wrapper>);
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

static_assert(glz::detail::reflectable<glz::json_schema<meta_schema_t>>);
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

static_assert(glz::detail::local_json_schema_t<local_schema_t>);

suite meta_schema_reflection_tests = [] {
   "meta_schema_reflection"_test = [] {
      meta_schema_t obj;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"x":0,"file_name":"","is_valid":false})") << buffer;

      const auto json_schema = glz::write_json_schema<meta_schema_t>().value_or("error");
      expect(
         json_schema ==
         R"({"type":["object"],"properties":{"file_name":{"$ref":"#/$defs/std::string","description":"provide a file name to load"},"is_valid":{"$ref":"#/$defs/bool","description":"for validation"},"x":{"$ref":"#/$defs/int32_t","description":"x is a special integer","minimum":1}},"additionalProperties":false,"$defs":{"bool":{"type":["boolean"]},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::string":{"type":["string"]}}})")
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
         R"({"type":["object"],"properties":{"file_name":{"$ref":"#/$defs/std::string","description":"provide a file name to load"},"is_valid":{"$ref":"#/$defs/bool","description":"for validation"},"x":{"$ref":"#/$defs/int32_t","description":"x is a special integer","minimum":1}},"additionalProperties":false,"$defs":{"bool":{"type":["boolean"]},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::string":{"type":["string"]}}})")
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

namespace glz::detail
{
   template <>
   struct from<JSON, std::chrono::seconds>
   {
      template <auto Opts>
      static void op(std::chrono::seconds& value, is_context auto&& ctx, auto&&... args)
      {
         int32_t sec_count{};
         read<JSON>::op<Opts>(sec_count, ctx, args...);
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
      glz::detail::keys_info_t info{.min_length = 8, .max_length = 8};
      [[maybe_unused]] const auto valid =
         glz::detail::front_bytes_hash_info<uint64_t>(glz::reflect<front_64_t>::keys, info);

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

int main() { return 0; }
