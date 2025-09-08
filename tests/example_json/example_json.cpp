// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze.hpp" // Glaze main header (most Glaze headers are included)
#include "ut/ut.hpp"

using namespace ut;

//------------------------------------
// Basic Struct with Reflection
//------------------------------------
struct BasicStruct
{
   int i{};
   double d{};
   std::string str{};
   std::array<uint32_t, 3> arr{};
};
static_assert(glz::reflectable<BasicStruct>);

// Demonstration of reading and writing a basic reflectable struct without `glz::meta`.
suite basic_reflection = [] {
   "basic_struct_reflection"_test = [] {
      BasicStruct obj{42, 3.14, "Hello", {1, 2, 3}};
      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"i":42,"d":3.14,"str":"Hello","arr":[1,2,3]})");

      BasicStruct obj2{};
      expect(!glz::read_json(obj2, json));
      expect(obj2.i == 42);
      expect(obj2.d == 3.14);
      expect(obj2.str == "Hello");
      expect(obj2.arr == std::array<uint32_t, 3>{1, 2, 3});
   };
};

//------------------------------------
// Adding Custom Meta for Renaming Fields
//------------------------------------
struct MetaStruct
{
   int count{};
   std::string name{};
};

template <>
struct glz::meta<MetaStruct>
{
   using T = MetaStruct;
   static constexpr auto value = object("cnt", &T::count, "label", &T::name);
};

suite meta_struct_demo = [] {
   "meta_struct_test"_test = [] {
      MetaStruct obj{5, "Gadget"};
      std::string json;
      expect(not glz::write_json(obj, json));
      // Keys renamed to cnt and label
      expect(json == R"({"cnt":5,"label":"Gadget"})");

      MetaStruct obj2{};
      std::string input = R"({"cnt":10,"label":"Widget"})";
      expect(!glz::read_json(obj2, input));
      expect(obj2.count == 10);
      expect(obj2.name == "Widget");
   };
};

//------------------------------------
// Optional Fields
//------------------------------------
struct WithOptional
{
   std::string required = "default";
   std::optional<double> maybe = {};
};
static_assert(glz::reflectable<WithOptional>);

suite optional_fields = [] {
   "optional_fields_test"_test = [] {
      WithOptional obj{};
      std::string json;
      // skip_null_members option defaults to `true`
      expect(not glz::write<glz::opts{.skip_null_members = false}>(obj, json));
      // maybe is empty -> null
      expect(json == R"({"required":"default","maybe":null})");

      std::string input = R"({"required":"changed","maybe":3.1415})";
      expect(!glz::read_json(obj, input));
      expect(obj.required == "changed");
      expect(obj.maybe && *obj.maybe == 3.1415);

      // skip null members
      obj.maybe.reset();
      json.clear();
      expect(not glz::write_json(obj, json));
      expect(json == R"({"required":"changed"})");
   };
};

//------------------------------------
// Enumerations as Strings
//------------------------------------
enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color>
{
   using enum Color;
   static constexpr auto value = enumerate(Red, Green, Blue);
};

struct EnumHolder
{
   Color c{Color::Green};
};
static_assert(glz::reflectable<EnumHolder>);

suite enum_test = [] {
   "enum_as_string_key"_test = [] {
      std::map<Color, bool> obj{{Color::Red, true}};
      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"Red":true})");

      std::string input = R"({"Green":true})";
      expect(!glz::read_json(obj, input));
      expect(obj[Color::Green]);
   };

   "enum_as_string_value"_test = [] {
      EnumHolder obj{};
      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"c":"Green"})");

      std::string input = R"({"c":"Blue"})";
      expect(!glz::read_json(obj, input));
      expect(obj.c == Color::Blue);
   };

   "enum_as_key_vector_pair_concatenate"_test = [] {
      std::vector<std::pair<Color, int>> obj{{Color::Red, 1}, {Color::Green, 2}};
      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"Red":1,"Green":2})");

      std::vector<std::pair<Color, int>> obj2{};
      std::string input = R"({"Blue":3})";
      expect(!glz::read_json(obj2, input));
      expect(obj2.size() == 1);
      expect(obj2[0].first == Color::Blue);
      expect(obj2[0].second == 3);
   };
};

//------------------------------------
// Arrays, Tuples, and Vectors
//------------------------------------
struct ContainerStruct
{
   std::vector<int> vec{1, 2, 3};
   std::array<std::string, 2> arr{"Hello", "World"};
   std::tuple<int, double, std::string> tup{42, 2.718, "pi?"};
   std::deque<float> dq{3.14f, 2.71f};
   std::list<int> lis{10, 11, 12};
};
static_assert(glz::reflectable<ContainerStruct>);

suite container_test = [] {
   "containers_read_write"_test = [] {
      ContainerStruct c{};
      std::string json;
      expect(not glz::write_json(c, json));
      // Check output structure includes all
      expect(json ==
             R"({"vec":[1,2,3],"arr":["Hello","World"],"tup":[42,2.718,"pi?"],"dq":[3.14,2.71],"lis":[10,11,12]})");

      ContainerStruct c2{};
      expect(!glz::read_json(c2, json));
      expect(c2.vec == std::vector<int>{1, 2, 3});
      expect(c2.lis == std::list<int>{10, 11, 12});
      expect(std::get<2>(c2.tup) == "pi?");
   };
};

//------------------------------------
// Maps and Unordered Maps
//------------------------------------
struct MapStruct
{
   std::map<std::string, int> str_map{{"one", 1}, {"two", 2}};
   std::unordered_map<int, std::string> umap{{5, "five"}, {7, "seven"}};
};
static_assert(glz::reflectable<MapStruct>);

suite map_test = [] {
   "map_unordered_map"_test = [] {
      MapStruct ms{};
      std::string json;
      expect(not glz::write_json(ms, json));
      // Keys become strings; int keys also become strings by default
      // Ordering in unordered_map not guaranteed
      expect(json.find("\"one\":1") != std::string::npos);
      expect(json.find("\"5\":\"five\"") != std::string::npos);

      MapStruct ms2{};
      expect(!glz::read_json(ms2, json));
      expect(ms2.str_map["one"] == 1);
      expect(ms2.umap[5] == "five");
   };
};

//------------------------------------
// Variants
//------------------------------------
struct VarA
{
   int x{};
};
struct VarB
{
   double y{};
};

template <>
struct glz::meta<VarA>
{
   using T = VarA;
   static constexpr auto value = object("x", &T::x);
};

template <>
struct glz::meta<VarB>
{
   using T = VarB;
   static constexpr auto value = object("y", &T::y);
};

using VarType = std::variant<VarA, VarB>;

struct VariantHolder
{
   VarType var;
};

template <>
struct glz::meta<VariantHolder>
{
   using T = VariantHolder;
   static constexpr auto value = object("var", &T::var);
};

suite variant_test = [] {
   "variant_read_write"_test = [] {
      VariantHolder h{};
      h.var = VarB{3.14};
      std::string json;
      expect(not glz::write_json(h, json));
      expect(json == R"({"var":{"y":3.14}})");

      std::string input = R"({"var":{"x":5}})";
      expect(!glz::read_json(h, input));
      expect(std::holds_alternative<VarA>(h.var));
      expect(std::get<VarA>(h.var).x == 5);
   };
};

//------------------------------------
// Partial Reading and Writing
//------------------------------------
struct PartialStruct
{
   int a{1};
   int b{2};
   int c{3};
};

suite partial_test = [] {
   "partial_write"_test = [] {
      PartialStruct p{};
      static constexpr auto selected = glz::json_ptrs("/a", "/c");
      std::string json;
      expect(not glz::write_json<selected>(p, json));
      expect(json == R"({"a":1,"c":3})");
   };
};

//------------------------------------
// Comment and Flexible Parsing
//------------------------------------
// Glaze can parse JSON with comments if enabled.

struct CommentStruct
{
   int val{};
};
static_assert(glz::reflectable<CommentStruct>);

suite comment_parsing = [] {
   "comment_test"_test = [] {
      CommentStruct cs{};
      // JSON with comments: Glaze allows C-style and // comments if opts.comments = true
      std::string input = R"({
         // here is a comment
         "val": 99 /* inline comment */
      })";
      constexpr glz::opts options{.comments = true};
      expect(!glz::read<options>(cs, input));
      expect(cs.val == 99);
   };
};

//------------------------------------
// Prettify, Minify, Raw Fields
//------------------------------------
struct PrettifyStruct
{
   int id;
   std::string msg;
};

struct RawExample
{
   glz::raw_json raw_data;
};

suite formatting_and_raw = [] {
   "prettify_minify"_test = [] {
      PrettifyStruct pd{123, "Hello"};
      std::string json;
      expect(!glz::write_json(pd, json));
      // Minify is default, let's prettify
      std::string pretty = glz::prettify_json(json);
      expect(pretty.find('\n') != std::string::npos);
      std::string minified = glz::minify_json(pretty);
      expect(minified == json);
   };

   "raw_data_handling"_test = [] {
      RawExample re{};
      std::string input = R"({"raw_data":{"some":"object","arr":[1,2,3]}})";
      expect(!glz::read_json(re, input));
      std::string out;
      expect(!glz::write_json(re, out));
      expect(out == input);
   };
};

//------------------------------------
// JSON Pointer Access for Get/Set
//------------------------------------
struct PointerStruct
{
   std::array<int, 3> arr{10, 20, 30};
   std::map<std::string, std::string> m{{"key", "value"}};
};

suite pointer_access = [] {
   "json_pointer_get_set"_test = [] {
      PointerStruct ps{};
      auto val = glz::get<int>(ps, "/arr/1");
      expect(val.has_value());
      if (val) expect(val.value().get() == 20);

      glz::set(ps, "/arr/1", 42);
      expect(ps.arr[1] == 42);

      auto map_val = glz::get<std::string>(ps, "/m/key");
      expect(map_val.has_value());
      if (map_val) expect(map_val.value().get() == "value");

      glz::set(ps, "/m/key", "new_value");
      expect(ps.m["key"] == "new_value");
   };
};

//------------------------------------
// NDJSON (Newline-Delimited JSON) aka JSON Lines
//------------------------------------
// Glaze can read/write arrays of objects line by line.

struct NDJItem
{
   int x{};
   std::string y{};
};

suite ndjson_test = [] {
   "ndjson_io"_test = [] {
      std::vector<NDJItem> items{{1, "A"}, {2, "B"}};

      std::string ndjson;
      // NDJSON = each object on a newline
      expect(not glz::write<glz::opts{.format = glz::NDJSON}>(items, ndjson));
      // Should look like:
      // {"x":1,"y":"A"}
      // {"x":2,"y":"B"}

      std::vector<NDJItem> read_back;
      expect(!glz::read<glz::opts{.format = glz::NDJSON}>(read_back, ndjson));
      expect(read_back.size() == 2);
      expect(read_back[0].x == 1 && read_back[0].y == "A");
   };
};

//------------------------------------
// Error Handling
//------------------------------------
struct StrictData
{
   int must_exist;
   double must_exist_too;
};

suite error_handling = [] {
   "error_on_missing_keys"_test = [] {
      StrictData sd{};
      std::string input = R"({"must_exist":100})";
      constexpr glz::opts options{.error_on_missing_keys = true};
      auto ec = glz::read<options>(sd, input);
      expect(ec != glz::error_code::none); // fails because must_exist_too missing
   };
};

//------------------------------------
// Controlling float precision & formatting
//------------------------------------
struct FloatPrecision
{
   double val{3.141592653589793};
};

struct float_opts : glz::opts
{
   glz::float_precision float_max_write_precision{};
};

suite float_precision_test = [] {
   "float_precision"_test = [] {
      FloatPrecision fp{};
      std::string json;
      // limit float precision to float32
      expect(not glz::write<float_opts{{}, glz::float_precision::float32}>(fp, json));
      // This should produce fewer decimal places.
      expect(json.find("3.1415927") != std::string::npos); // approximate float32 rounding
   };
};

//------------------------------------
// Schema Generation
//------------------------------------
struct SchemaDemo
{
   int x{};
   std::string name{};
   bool flag{};
};

template <>
struct glz::json_schema<SchemaDemo>
{
   schema x{.description = "An integer x"};
   schema name{.description = "A name for something"};
   schema flag{.description = "A boolean flag"};
};

suite schema_generation = [] {
   "schema_demo"_test = [] {
      auto schema = glz::write_json_schema<SchemaDemo>().value_or("error");
      expect(
         schema ==
         R"({"type":["object"],"properties":{"flag":{"$ref":"#/$defs/bool","description":"A boolean flag"},"name":{"$ref":"#/$defs/std::string","description":"A name for something"},"x":{"$ref":"#/$defs/int32_t","description":"An integer x"}},"additionalProperties":false,"$defs":{"bool":{"type":["boolean"]},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::string":{"type":["string"]}},"title":"SchemaDemo"})")
         << schema;
   };
};

//------------------------------------
// Local Schemas
//------------------------------------
// You can also provide local schemas directly inside your struct with a nested `glaze_json_schema` struct.

struct LocalSchema
{
   int count{};
   std::string file{};
   bool valid{};

   struct glaze_json_schema
   {
      glz::schema count{.description = "A count"};
      glz::schema file{.description = "A file path"};
      glz::schema valid{.description = "Validity flag"};
   };
};

suite local_schema_test = [] {
   "local_schema"_test = [] {
      auto schema = glz::write_json_schema<LocalSchema>().value_or("error");
      expect(
         schema ==
         R"({"type":["object"],"properties":{"count":{"$ref":"#/$defs/int32_t","description":"A count"},"file":{"$ref":"#/$defs/std::string","description":"A file path"},"valid":{"$ref":"#/$defs/bool","description":"Validity flag"}},"additionalProperties":false,"$defs":{"bool":{"type":["boolean"]},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::string":{"type":["string"]}},"title":"LocalSchema"})")
         << schema;
   };
};

//------------------------------------
// Unknown Keys and Unknown Fields
//------------------------------------
// If we want to handle unknown fields gracefully, we can configure opts or even store unknown fields.

struct UnknownFields
{
   int known = 42;
   // Suppose we want to store unknown keys as raw_json in a map
   std::map<std::string_view, glz::raw_json> extra;
};

template <>
struct glz::meta<UnknownFields>
{
   using T = UnknownFields;
   static constexpr auto value = object("known", &T::known);
   static constexpr auto unknown_read = &T::extra;
   static constexpr auto unknown_write = &T::extra;
};

suite unknown_keys_handling = [] {
   "unknown_fields"_test = [] {
      UnknownFields uf{};
      std::string input = R"({"known":7,"xtra":"stuff","another":{"obj":true}})";
      expect(!glz::read<glz::opts{.error_on_unknown_keys = false}>(uf, input));
      expect(uf.known == 7);
      expect(uf.extra["xtra"].str == "\"stuff\"");
      expect(uf.extra["another"].str == "{\"obj\":true}");
   };
};

int main()
{
   return 0; // Running 'ut' will execute all test suites defined above.
}
