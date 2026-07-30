
// Glaze Library
// For the license information refer to glaze.hpp

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include "glaze/containers/flat_map.hpp"
#include "glaze/json.hpp"
#include "json_test_shared_types.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct put_action
{
   std::map<std::string, int> data{};
   [[nodiscard]] bool operator==(const put_action&) const = default;
};

template <>
struct glz::meta<put_action>
{
   using T = put_action;
   static constexpr std::string_view name = "put_action";
   static constexpr auto value = object("data", &T::data);
};

struct delete_action
{
   std::string data{};
   [[nodiscard]] bool operator==(const delete_action&) const = default;
};

template <>
struct glz::meta<delete_action>
{
   using T = delete_action;
   static constexpr std::string_view name = "delete_action";
   static constexpr auto value = object("data", &T::data);
};

using tagged_variant = std::variant<put_action, delete_action>;

template <>
struct glz::meta<tagged_variant>
{
   static constexpr std::string_view tag = "action";
   static constexpr auto ids = std::array{"PUT", "DELETE"}; // Defaults to glz::name_v of the type
};

// Test automatic ids
using tagged_variant2 = std::variant<put_action, delete_action, std::monostate>;
template <>
struct glz::meta<tagged_variant2>
{
   static constexpr std::string_view tag = "type";
   // ids defaults to glz::name_v of the type
};

// Test array based variant (experimental, not meant for external usage since api might change)
using num_variant = std::variant<double, int32_t, uint64_t, int8_t, float>;
struct holds_some_num
{
   num_variant num{};
};
template <>
struct glz::meta<holds_some_num>
{
   using T = holds_some_num;
   static constexpr auto value = object("num", glz::array_variant{&T::num});
};

struct OptionA
{
   std::string tag{};
   int a{};
};

struct OptionB
{
   std::string tag{};
   int a{};
};

using TaggedObject = std::variant<OptionA, OptionB>;

template <>
struct glz::meta<TaggedObject>
{
   static constexpr std::string_view tag = "tag";
   static constexpr auto ids = std::array{"A", "B"};
};

suite tagged_variant_tests = [] {
   "TaggedObject"_test = [] {
      TaggedObject content;
      std::string data = R"({ "tag": "A", "a": 2 })";
      expect(!glz::read_json<TaggedObject>(content, data));
      expect(std::get<OptionA>(content).a == 2);
   };

   "tagged_variant_read_tests"_test = [] {
      tagged_variant var{};
      expect(glz::read_json(var, R"({"action":"DELETE","data":"the_internet"})") == glz::error_code::none);
      expect(std::holds_alternative<delete_action>(var));
      expect(std::get<delete_action>(var).data == "the_internet");

      // tag at end
      expect(glz::read_json(var, R"({"data":"the_internet","action":"DELETE"})") == glz::error_code::none);
      expect(std::holds_alternative<delete_action>(var));
      expect(std::get<delete_action>(var).data == "the_internet");

      tagged_variant2 var2{};
      expect(glz::read_json(var2, R"({"type":"put_action","data":{"x":100,"y":200}})") == glz::error_code::none);
      expect(std::holds_alternative<put_action>(var2));
      expect(std::get<put_action>(var2).data["x"] == 100);
      expect(std::get<put_action>(var2).data["y"] == 200);

      // tag at end
      expect(glz::read_json(var2, R"({"data":{"x":100,"y":200},"type":"put_action"})") == glz::error_code::none);
      expect(std::holds_alternative<put_action>(var2));
      expect(std::get<put_action>(var2).data["x"] == 100);
      expect(std::get<put_action>(var2).data["y"] == 200);

      //
      const auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(
         var2, R"({"type":"put_action","data":{"x":100,"y":200}})");
      expect(err == glz::error_code::none);
      expect(std::holds_alternative<put_action>(var2));
      expect(std::get<put_action>(var2).data["x"] == 100);
      expect(std::get<put_action>(var2).data["y"] == 200);
   };

   "tagged_variant_write_tests"_test = [] {
      // custom tagged discriminator ids
      tagged_variant var = delete_action{{"the_internet"}};
      std::string s{};
      expect(not glz::write_json(var, s));
      expect(s == R"({"action":"DELETE","data":"the_internet"})");
      s.clear();

      // Automatic tagged discriminator ids
      tagged_variant2 var2 = put_action{{{"x", 100}, {"y", 200}}};
      expect(not glz::write_json(var2, s));
      expect(s == R"({"type":"put_action","data":{"x":100,"y":200}})");
      s.clear();

      // prettifies valid JSON
      expect(not glz::write<glz::opts{.prettify = true}>(var, s));
      tagged_variant parsed_var;
      expect(glz::read_json(parsed_var, s) == glz::error_code::none);
      expect(parsed_var == var);
   };

#if !defined(_MSC_VER)
   "tagged_variant_schema_tests"_test = [] {
      auto s = glz::write_json_schema<tagged_variant>().value_or("error");
      // P2996 reflection: Bloomberg Clang returns unqualified names, GCC returns qualified names
      // Accept both forms for the title
      auto expected_qualified =
         R"({"type":"object","$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"oneOf":[{"type":"object","properties":{"action":{"const":"PUT"},"data":{"type":"object","additionalProperties":{"$ref":"#/$defs/int32_t"}}},"additionalProperties":false,"required":["action"],"title":"PUT"},{"type":"object","properties":{"action":{"const":"DELETE"},"data":{"type":"string"}},"additionalProperties":false,"required":["action"],"title":"DELETE"}],"title":"std::variant<put_action, delete_action>"})";
#if GLZ_REFLECTION26
      auto expected_unqualified =
         R"({"type":"object","$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"oneOf":[{"type":"object","properties":{"action":{"const":"PUT"},"data":{"type":"object","additionalProperties":{"$ref":"#/$defs/int32_t"}}},"additionalProperties":false,"required":["action"],"title":"PUT"},{"type":"object","properties":{"action":{"const":"DELETE"},"data":{"type":"string"}},"additionalProperties":false,"required":["action"],"title":"DELETE"}],"title":"variant<put_action, delete_action>"})";
      expect(s == expected_qualified || s == expected_unqualified) << s;
#else
      expect(s == expected_qualified) << s;
#endif
   };
#endif

   "array_variant_tests"_test = [] {
      // Test array based variant (experimental, not meant for external usage since api might change)

      holds_some_num obj{};
      auto ec = glz::read_json(obj, R"({"num":["float", 3.14]})");
      std::string b = R"({"num":["float", 3.14]})";
      expect(ec == glz::error_code::none) << glz::format_error(ec, b);
      expect(std::get<float>(obj.num) == 3.14f);
      expect(not glz::read_json(obj, R"({"num":["uint64_t", 5]})"));
      expect(std::get<uint64_t>(obj.num) == 5);
      expect(not glz::read_json(obj, R"({"num":["int8_t", -3]})"));
      expect(std::get<int8_t>(obj.num) == -3);
      expect(not glz::read_json(obj, R"({"num":["int32_t", -2]})"));
      expect(std::get<int32_t>(obj.num) == -2);

      obj.num = 5.0;
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"num":["double",5]})");
      obj.num = uint64_t{3};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"num":["uint64_t",3]})");
      obj.num = int8_t{-5};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"num":["int8_t",-5]})");
   };

#if !defined(_MSC_VER)
   "shared_ptr variant schema"_test = [] {
      const auto schema = glz::write_json_schema<std::shared_ptr<tagged_variant2>>().value_or("error");
      // P2996 reflection: Bloomberg Clang returns unqualified names, GCC returns qualified names
      // Accept both forms for the title
      auto expected_qualified =
         R"({"type":["object","null"],"$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"oneOf":[{"type":"object","properties":{"data":{"type":"object","additionalProperties":{"$ref":"#/$defs/int32_t"}},"type":{"const":"put_action"}},"additionalProperties":false,"required":["type"],"title":"put_action"},{"type":"object","properties":{"data":{"type":"string"},"type":{"const":"delete_action"}},"additionalProperties":false,"required":["type"],"title":"delete_action"},{"type":"object","properties":{"type":{"const":"std::monostate"}},"additionalProperties":false,"required":["type"],"title":"std::monostate"}],"title":"std::shared_ptr<std::variant<put_action, delete_action, std::monostate>>"})";
#if GLZ_REFLECTION26
      auto expected_unqualified =
         R"({"type":["object","null"],"$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"oneOf":[{"type":"object","properties":{"data":{"type":"object","additionalProperties":{"$ref":"#/$defs/int32_t"}},"type":{"const":"put_action"}},"additionalProperties":false,"required":["type"],"title":"put_action"},{"type":"object","properties":{"data":{"type":"string"},"type":{"const":"delete_action"}},"additionalProperties":false,"required":["type"],"title":"delete_action"},{"type":"object","properties":{"type":{"const":"std::monostate"}},"additionalProperties":false,"required":["type"],"title":"std::monostate"}],"title":"std::shared_ptr<variant<put_action, delete_action, monostate>>"})";
      expect(schema == expected_qualified || schema == expected_unqualified) << schema;
#else
      expect(schema == expected_qualified) << schema;
#endif
   };
#endif
};

struct variant_obj
{
   std::variant<double, std::string> v{};
};

template <>
struct glz::meta<variant_obj>
{
   static constexpr std::string_view name = "variant_obj";
   using T = variant_obj;
   static constexpr auto value = object("v", &T::v);
};

struct var_a1
{
   int i{};
};

struct var_a2
{
   double i{};
};

suite variant_tests = [] {
   "variant_write_tests"_test = [] {
      std::variant<double, std::string> d = "not_a_fish";
      std::string s{};

      expect(not glz::write_json(d, s));
      expect(s == R"("not_a_fish")");

      d = 5.7;
      s.clear();
      expect(not glz::write_json(d, s));
      expect(s == "5.7");

      std::variant<std::monostate, int, std::string> m{};
      expect(not glz::write_json(m, s));
      expect(s == "null") << s;
   };

   "variant_read_"_test = [] {
      std::variant<int32_t, double> x = 44;
      expect(glz::read_json(x, "33") == glz::error_code::none);
      expect(std::get<int32_t>(x) == 33);
   };

   // TODO: Make reading into the active element work here
   /*"variant read active"_test = [] {
      std::variant<var_a1, var_a2> v = var_a2{};
      std::string json = R"({"i":6})";
      expect(not glz::read_json(v, json));
      expect(v.index() == 1);
      expect(std::get<var_a2>(v).i == 6);
   };*/

   "variant_read_auto"_test = [] {
      // Auto deduce variant with no conflicting basic types
      std::variant<std::monostate, int, std::string, bool, std::map<std::string, double>, std::vector<std::string>> m{};
      expect(glz::read_json(m, R"("Hello World")") == glz::error_code::none);
      expect[std::holds_alternative<std::string>(m)];
      expect(std::get<std::string>(m) == "Hello World");

      expect(glz::read_json(m, R"(872)") == glz::error_code::none);
      expect[std::holds_alternative<int>(m)];
      expect(std::get<int>(m) == 872);

      expect(glz::read_json(m, R"({"pi":3.14})") == glz::error_code::none);
      expect[std::holds_alternative<std::map<std::string, double>>(m)];
      expect(std::get<std::map<std::string, double>>(m)["pi"] == 3.14);

      expect(glz::read_json(m, R"(true)") == glz::error_code::none);
      expect[std::holds_alternative<bool>(m)];
      expect(std::get<bool>(m) == true);

      expect(glz::read_json(m, R"(["a", "b", "c"])") == glz::error_code::none);
      expect[std::holds_alternative<std::vector<std::string>>(m)];
      expect(std::get<std::vector<std::string>>(m)[1] == "b");

      expect(glz::read_json(m, "null") == glz::error_code::none);
      expect[std::holds_alternative<std::monostate>(m)];
   };

   "variant_read_obj"_test = [] {
      variant_obj obj{};

      obj.v = double{};
      expect(glz::read_json(obj, R"({"v": 5.5})") == glz::error_code::none);

      expect(std::get<double>(obj.v) == 5.5);
   };

   "variant_request"_test = [] {
      std::map<std::string, std::variant<std::string, int, bool>> request;

      request["username"] = "paulo";
      request["password"] = "123456";
      request["remember"] = true;

      auto str = glz::write_json(request).value_or("error");

      expect(str == R"({"password":"123456","remember":true,"username":"paulo"})") << str;
   };

   "variant write/read enum"_test = [] {
      std::variant<Color, std::uint16_t> var{Color::Red};
      auto res{glz::write_json(var).value_or("error")};
      expect(res == "\"Red\"") << res;
      auto read{glz::read_json<std::variant<Color, std::uint16_t>>(res)};
      expect(read.has_value());
      expect(std::holds_alternative<Color>(read.value()));
      expect(std::get<Color>(read.value()) == Color::Red);
   };

   "variant read tuple"_test = [] {
      using int_int_tuple_t = std::tuple<int, int>;
      std::variant<int, int_int_tuple_t, std::string> var;

      expect(glz::read_json(var, R"(1)") == glz::error_code::none);
      expect(std::get<int>(var) == 1);

      expect(glz::read_json(var, R"("str")") == glz::error_code::none);
      expect(std::get<std::string>(var) == "str");

      expect(glz::read_json(var, R"([2, 3])") == glz::error_code::none);
      expect(std::get<int_int_tuple_t>(var) == int_int_tuple_t{2, 3});
   };
};

// Tests for std::vector<std::variant<...>> with purely reflected structs
struct reflected_person
{
   std::string name{};
   int age{};
   double height{};
};

struct reflected_animal
{
   std::string species{};
   std::string name{};
   int weight{};
};

struct reflected_vehicle
{
   std::string make{};
   std::string model{};
   int year{};
   double price{};
};

struct reflected_book
{
   std::string title{};
   std::string author{};
   int pages{};
   std::string isbn{};
};

static_assert(glz::reflectable<reflected_person>);
static_assert(glz::reflectable<reflected_animal>);
static_assert(glz::reflectable<reflected_vehicle>);
static_assert(glz::reflectable<reflected_book>);

suite vector_variant_reflection_tests = [] {
   "vector of variant with two reflected structs"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      entities.push_back(reflected_person{"Alice", 30, 165.5});
      entities.push_back(reflected_animal{"Dog", "Buddy", 25});
      entities.push_back(reflected_person{"Bob", 25, 180.0});
      entities.push_back(reflected_animal{"Cat", "Whiskers", 4});

      std::string json;
      expect(!glz::write_json(entities, json));

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);

      expect(read_entities.size() == 4);

      expect(std::holds_alternative<reflected_person>(read_entities[0]));
      auto& p1 = std::get<reflected_person>(read_entities[0]);
      expect(p1.name == "Alice");
      expect(p1.age == 30);
      expect(p1.height == 165.5);

      expect(std::holds_alternative<reflected_animal>(read_entities[1]));
      auto& a1 = std::get<reflected_animal>(read_entities[1]);
      expect(a1.species == "Dog");
      expect(a1.name == "Buddy");
      expect(a1.weight == 25);

      expect(std::holds_alternative<reflected_person>(read_entities[2]));
      auto& p2 = std::get<reflected_person>(read_entities[2]);
      expect(p2.name == "Bob");
      expect(p2.age == 25);
      expect(p2.height == 180.0);

      expect(std::holds_alternative<reflected_animal>(read_entities[3]));
      auto& a2 = std::get<reflected_animal>(read_entities[3]);
      expect(a2.species == "Cat");
      expect(a2.name == "Whiskers");
      expect(a2.weight == 4);
   };

   "vector of variant with three reflected structs"_test = [] {
      using item_variant = std::variant<reflected_person, reflected_vehicle, reflected_book>;
      std::vector<item_variant> items;

      items.push_back(reflected_person{"Charlie", 35, 175.0});
      items.push_back(reflected_vehicle{"Toyota", "Camry", 2022, 25000.0});
      items.push_back(reflected_book{"The Great Gatsby", "F. Scott Fitzgerald", 180, "978-0-7432-7356-5"});
      items.push_back(reflected_person{"Diana", 28, 160.0});

      std::string json;
      expect(!glz::write_json(items, json));

      std::vector<item_variant> read_items;
      expect(glz::read_json(read_items, json) == glz::error_code::none);

      expect(read_items.size() == 4);

      expect(std::holds_alternative<reflected_person>(read_items[0]));
      expect(std::holds_alternative<reflected_vehicle>(read_items[1]));
      expect(std::holds_alternative<reflected_book>(read_items[2]));
      expect(std::holds_alternative<reflected_person>(read_items[3]));

      auto& vehicle = std::get<reflected_vehicle>(read_items[1]);
      expect(vehicle.make == "Toyota");
      expect(vehicle.model == "Camry");
      expect(vehicle.year == 2022);
      expect(vehicle.price == 25000.0);

      auto& book = std::get<reflected_book>(read_items[2]);
      expect(book.title == "The Great Gatsby");
      expect(book.author == "F. Scott Fitzgerald");
      expect(book.pages == 180);
      expect(book.isbn == "978-0-7432-7356-5");
   };

   "empty vector of variant"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      std::string json;
      expect(!glz::write_json(entities, json));
      expect(json == "[]");

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);
      expect(read_entities.empty());
   };

   "vector with single variant element"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      entities.push_back(reflected_person{"Eve", 40, 170.0});

      std::string json;
      expect(!glz::write_json(entities, json));

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);

      expect(read_entities.size() == 1);
      expect(std::holds_alternative<reflected_person>(read_entities[0]));
      auto& person = std::get<reflected_person>(read_entities[0]);
      expect(person.name == "Eve");
      expect(person.age == 40);
      expect(person.height == 170.0);
   };

   "roundtrip with mixed types"_test = [] {
      using mixed_variant = std::variant<reflected_person, reflected_animal, reflected_vehicle, reflected_book>;
      std::vector<mixed_variant> original;

      original.push_back(reflected_book{"1984", "George Orwell", 328, "978-0-452-28423-4"});
      original.push_back(reflected_animal{"Horse", "Thunder", 500});
      original.push_back(reflected_vehicle{"Honda", "Accord", 2023, 27000.0});
      original.push_back(reflected_person{"Frank", 45, 185.0});
      original.push_back(reflected_book{"To Kill a Mockingbird", "Harper Lee", 281, "978-0-06-112008-4"});

      std::string json;
      expect(!glz::write_json(original, json));

      std::vector<mixed_variant> decoded;
      expect(glz::read_json(decoded, json) == glz::error_code::none);

      expect(decoded.size() == original.size());

      for (size_t i = 0; i < original.size(); ++i) {
         expect(original[i].index() == decoded[i].index());
      }
   };

   "prettified json output"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      entities.push_back(reflected_person{"Grace", 32, 168.0});
      entities.push_back(reflected_animal{"Bird", "Tweety", 1});

      std::string json;
      expect(!glz::write<glz::opts{.prettify = true}>(entities, json));

      expect(json.find("\n") != std::string::npos); // Should contain newlines
      expect(json.find("   ") != std::string::npos); // Should contain indentation

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);
      expect(read_entities.size() == 2);
   };

   "vector of variant with structs having overlapping field names"_test = [] {
      // Both structs have a 'name' field, but different other fields
      using ambiguous_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<ambiguous_variant> items;

      // The variant should deduce the correct type based on all fields present
      items.push_back(reflected_person{"Henry", 50, 175.5});
      items.push_back(reflected_animal{"Lion", "Simba", 190});

      std::string json;
      expect(!glz::write_json(items, json));

      std::vector<ambiguous_variant> read_items;
      expect(glz::read_json(read_items, json) == glz::error_code::none);

      expect(read_items.size() == 2);
      expect(std::holds_alternative<reflected_person>(read_items[0]));
      expect(std::holds_alternative<reflected_animal>(read_items[1]));
   };
};

struct yz_t
{
   int y{};
   int z{};
};

template <>
struct glz::meta<yz_t>
{
   using T = yz_t;
   static constexpr auto value = object("y", &T::y, "z", &T::z);
};

struct xz_t
{
   int x{};
   int z{};
};

template <>
struct glz::meta<xz_t>
{
   using T = xz_t;
   static constexpr auto value = object("x", &T::x, "z", &T::z);
};

suite metaobject_variant_auto_deduction = [] {
   "metaobject_variant_auto_deduction"_test = [] {
      std::variant<xy_t, yz_t, xz_t> var{};

      std::string b = R"({"y":1,"z":2})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<yz_t>(var));
      expect(std::get<yz_t>(var).y == 1);
      expect(std::get<yz_t>(var).z == 2);

      b = R"({"x":5,"y":7})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xy_t>(var));
      expect(std::get<xy_t>(var).x == 5);
      expect(std::get<xy_t>(var).y == 7);

      b = R"({"z":3,"x":4})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xz_t>(var));
      expect(std::get<xz_t>(var).z == 3);
      expect(std::get<xz_t>(var).x == 4);
   };
};

struct varx
{
   struct glaze
   {
      static constexpr std::string_view name = "varx";
      static constexpr auto value = glz::object();
   };
};
static_assert(glz::name_v<varx> == "varx");
struct vary
{
   struct glaze
   {
      static constexpr std::string_view name = "vary";
      static constexpr auto value = glz::object();
   };
};

using vari = std::variant<varx, vary>;

template <>
struct glz::meta<vari>
{
   static constexpr std::string_view name = "vari";
   static constexpr std::string_view tag = "type";
};

static_assert(glz::named<vari>);
static_assert(glz::name_v<vari> == "vari");

struct var_schema
{
   std::string schema{};
   vari variant{};

   struct glaze
   {
      using T = var_schema;
      static constexpr auto value = glz::object("$schema", &T::schema, &T::variant);
   };
};

suite empty_variant_objects = [] {
   "empty_variant_objects"_test = [] {
      vari v = varx{};
      std::string s;
      expect(not glz::write_json(v, s));
      expect(s == R"({"type":"varx"})");

      v = vary{};

      expect(!glz::read_json(v, s));
      expect(std::holds_alternative<varx>(v));
   };

   "empty_variant_objects schema"_test = [] {
      const auto s = glz::write_json_schema<var_schema>().value_or("error");
      expect(
         s ==
         R"({"type":"object","properties":{"$schema":{"type":"string"},"variant":{"type":"object","oneOf":[{"type":"object","properties":{"type":{"const":"varx"}},"additionalProperties":false,"required":["type"],"title":"varx"},{"type":"object","properties":{"type":{"const":"vary"}},"additionalProperties":false,"required":["type"],"title":"vary"}]}},"additionalProperties":false,"title":"var_schema"})")
         << s;
   };
};

struct Obj1
{
   int value;
   std::string text;
};

template <>
struct glz::meta<Obj1>
{
   using T = Obj1;
   static constexpr auto list_write = [](T& obj1) {
      const auto& value = obj1.value;
      return std::vector<int>{value, value + 1, value + 2};
   };
   static constexpr auto value = object(&T::value, &T::text, "list", glz::custom<skip{}, list_write>);
};

struct Obj2
{
   int value;
   std::string text;
   Obj1 obj1;
};

suite custom_object_variant_test = [] {
   "custom_object_variant"_test = [] {
      using Serializable = std::variant<Obj1, Obj2>;
      std::vector<Serializable> objects{
         Obj1{1, "text 1"},
         Obj1{2, "text 2"},
         Obj2{3, "text 3", 10, "1000"},
         Obj1{4, "text 4"},
      };

      constexpr auto prettify_json = glz::opts{.prettify = true};

      std::string data = glz::write<prettify_json>(objects).value_or("error");

      expect(data == R"([
   {
      "value": 1,
      "text": "text 1",
      "list": [
         1,
         2,
         3
      ]
   },
   {
      "value": 2,
      "text": "text 2",
      "list": [
         2,
         3,
         4
      ]
   },
   {
      "value": 3,
      "text": "text 3",
      "obj1": {
         "value": 10,
         "text": "1000",
         "list": [
            10,
            11,
            12
         ]
      }
   },
   {
      "value": 4,
      "text": "text 4",
      "list": [
         4,
         5,
         6
      ]
   }
])");

      objects.clear();

      expect(!glz::read_json(objects, data));

      expect(data == glz::write<prettify_json>(objects));
   };
};

struct var_a
{
   int m1;

   struct glaze
   {
      static constexpr auto value = glz::object("a", &var_a::m1);
   };
};

struct var_b
{
   std::vector<var_a> m1;
   bool m2;

   struct glaze
   {
      static constexpr auto value = glz::object("b", &var_b::m1, "c", &var_b::m2);
   };
};

struct var_c
{
   std::vector<var_a> m1;
   struct glaze
   {
      static constexpr auto value = &var_c::m1;
   };
};

struct var_abc_t
{
   std::variant<var_a, var_b, var_c> m1;
   struct glaze
   {
      static constexpr auto value = &var_abc_t::m1;
   };
};

suite nested_variants = [] {
   "nested_variants"_test = [] {
      var_abc_t v{};

      auto ec = glz::read_json(v, std::string{R"({"a":5})"});

      expect(not ec) << glz::format_error(ec);
      expect(std::get<var_a>(v.m1).m1 == 5);
   };
};

struct hammerhead_t
{
   double length{};
};

struct mako_t
{
   double length{};
};

using shark_t = std::variant<hammerhead_t, mako_t>;

template <>
struct glz::meta<shark_t>
{
   static constexpr std::string_view tag = "name";
   static constexpr auto ids = std::array{"hammerhead", "mako"};
};

using shark_ptr_t = std::variant<std::shared_ptr<hammerhead_t>, std::shared_ptr<mako_t>>;

template <>
struct glz::meta<shark_ptr_t>
{
   static constexpr std::string_view tag = "name";
   static constexpr auto ids = std::array{"hammerhead", "mako"};
};

struct chair_t
{
   float height{};
   uint8_t number_of_legs{};
   bool has_back{};
};

struct bed_t
{
   float height{};
   bool has_headboard{};
};

using furniture_ptr_t = std::variant<std::shared_ptr<chair_t>, std::shared_ptr<bed_t>>;

suite shark_variant = [] {
   "shark_variant"_test = [] {
      shark_t shark{};
      auto ec = glz::read_json(shark, R"({"name":"mako","length":44.0})");
      expect(!ec);
      expect(std::holds_alternative<mako_t>(shark));
      expect(std::get<mako_t>(shark).length == 44.0);
   };

   "shark_ptr variant"_test = [] {
      shark_ptr_t shark{};
      auto ec = glz::read_json(shark, R"({"name":"mako","length":44.0})");
      expect(!ec);
      expect(std::holds_alternative<std::shared_ptr<mako_t>>(shark));
      expect(std::get<std::shared_ptr<mako_t>>(shark)->length == 44.0);
   };

   "furniture_ptr variant auto-deduction "_test = [] {
      furniture_ptr_t furniture{};
      auto ec = glz::read_json(furniture, R"({"height":44.0,"has_headboard":true})");
      expect(!ec);
      expect(std::holds_alternative<std::shared_ptr<bed_t>>(furniture));
      expect(std::get<std::shared_ptr<bed_t>>(furniture)->height == 44.0f);
      expect(std::get<std::shared_ptr<bed_t>>(furniture)->has_headboard);
   };
};

struct A_empty
{};

struct B_empty
{};

using C_empty = std::variant<A_empty, B_empty>;

template <>
struct glz::meta<C_empty>
{
   static constexpr std::string_view tag = "op";
};

suite empty_variant_testing = [] {
   "empty_variant 1"_test = [] {
      std::string_view text = R"({"xxx":"x","op":"B_empty"})";

      C_empty c;
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(c, text);
      expect(not ec) << glz::format_error(ec, text);
      expect(c.index() == 1);
   };

   "empty_variant 2"_test = [] {
      std::string_view text = R"({"xx":"x","op":"B_empty"})";

      C_empty c;
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(c, text);
      expect(not ec) << glz::format_error(ec, text);
      expect(c.index() == 1);
   };
};

struct A1
{
   int p{};
};

struct B1
{
   float p{};
};

using X1 = std::variant<A1>;
using Y1 = std::variant<A1, B1>;

template <>
struct glz::meta<A1>
{
   static constexpr auto value = object("p", &A1::p);
};

template <>
struct glz::meta<B1>
{
   static constexpr auto value = object("p", &B1::p);
};

template <>
struct glz::meta<X1>
{
   static constexpr std::string_view tag = "tag";
};

template <>
struct glz::meta<Y1>
{
   static constexpr std::string_view tag = "tag";
};

suite variant_tag_tests = [] {
   "variant tag"_test = [] {
      auto xString = glz::write_json(X1(A1()));
      expect(xString.has_value());

      auto x = glz::read_json<X1>(*xString);
      expect(bool(x));
      if (not x.has_value()) {
         std::cerr << glz::format_error(x.error(), *xString);
      }
   };
};

struct Number
{
   std::optional<double> minimum;
   std::optional<double> maximum;
};

template <>
struct glz::meta<Number>
{
   static constexpr auto value = glz::object(&Number::minimum, &Number::maximum);
};

struct Boolean
{};

template <>
struct glz::meta<Boolean>
{
   static constexpr auto value = glz::object();
};

struct Integer
{
   std::optional<int> minimum;
   std::optional<int> maximum;
};

template <>
struct glz::meta<Integer>
{
   static constexpr auto value = glz::object(&Integer::minimum, &Integer::maximum);
};

using Data = std::variant<Number, Integer>;

template <>
struct glz::meta<Data>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"number", "integer"};
};

struct Array
{
   Data items;
};

template <>
struct glz::meta<Array>
{
   static constexpr auto value = glz::object(&Array::items);
};

using Data2 = std::variant<Number, Boolean>;

template <>
struct glz::meta<Data2>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"number", "boolean"};
};

struct Array2
{
   Data2 items;
};

template <>
struct glz::meta<Array2>
{
   static constexpr auto value = glz::object(&Array2::items);
};

suite tagged_variant_null_members = [] {
   "tagged_variant_null_members"_test = [] {
      Array var = Array{Number{}};

      std::string s{};
      expect(not glz::write_json(var, s));
      expect(s == R"({"items":{"type":"number"}})") << s;
   };

   "variant deduction"_test = [] {
      Array2 var;
      std::string str = R"({"items": { "type" : "boolean"}})";

      auto pe = glz::read_json(var, str);
      expect(not pe) << glz::format_error(pe, str);
   };
};

struct Command401
{
   int code{};
   int indent{};
   std::vector<std::string> parameters{};
};

struct Command250Params
{
   std::string name{};
   int volume{};
   int pitch{};
   int pan{};
};

struct Command250
{
   int code{};
   int indent{};
   std::vector<Command250Params> parameters{};
};

using CommandVariant = std::variant<Command250, Command401>;

template <>
struct glz::meta<CommandVariant>
{
   static constexpr std::string_view tag = "code";
   static constexpr std::array ids = {250, 401};
};

suite integer_id_variant_tests = [] {
   "command variant"_test = [] {
      std::vector<CommandVariant> v{};

      std::string buffer =
         R"([{"code":401,"indent":0,"parameters":["You light the torch."]},{"code":250,"indent":0,"parameters":[{"name":"fnh_book1","volume":90,"pitch":100,"pan":0}]}])";

      auto ec = glz::read_json(v, buffer);
      expect(not ec) << glz::format_error(ec, buffer);

      std::string out{};
      expect(not glz::write_json(v, out));

      expect(out == buffer) << out;

      expect(not glz::write<glz::opts{.prettify = true}>(v, out));
      expect(out == R"([
   {
      "code": 401,
      "indent": 0,
      "parameters": [
         "You light the torch."
      ]
   },
   {
      "code": 250,
      "indent": 0,
      "parameters": [
         {
            "name": "fnh_book1",
            "volume": 90,
            "pitch": 100,
            "pan": 0
         }
      ]
   }
])") << out;
   };
};

struct SiteDiagnostic
{
   std::string message{};
   int severity{};
};

template <>
struct glz::meta<SiteDiagnostic>
{
   using T = SiteDiagnostic;
   static constexpr std::string_view name = "SiteDiagnostic";
   static constexpr auto value = object(&T::message, &T::severity);
};

struct DerivedSite0
{
   std::string message{};
   int severity{};
   std::string additional_info{};
};

template <>
struct glz::meta<DerivedSite0>
{
   using T = DerivedSite0;
   static constexpr std::string_view name = "DerivedSite0";
   static constexpr auto value = object(&T::message, &T::severity, &T::additional_info);
};

using SiteDiagnosticsVariant = std::variant<std::unique_ptr<SiteDiagnostic>, std::unique_ptr<DerivedSite0>>;

template <>
struct glz::meta<SiteDiagnosticsVariant>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"SiteDiagnostic", "DerivedSite0"};
};

struct DiagnosticsConfig
{
   glz::flat_map<std::string, SiteDiagnosticsVariant> diagnostics{};
};

template <>
struct glz::meta<DiagnosticsConfig>
{
   using T = DiagnosticsConfig;
   static constexpr auto value = object(&T::diagnostics);
};

suite unique_ptr_variant_tests = [] {
   "flat_map with variant of unique_ptr"_test = [] {
      DiagnosticsConfig config;

      config.diagnostics["site1"] = std::make_unique<SiteDiagnostic>(SiteDiagnostic{"Basic diagnostic", 1});
      config.diagnostics["site2"] =
         std::make_unique<DerivedSite0>(DerivedSite0{"Advanced diagnostic", 2, "Extra details"});

      std::string json;
      auto write_result = glz::write_json(config, json);
      expect(!write_result) << "Failed to write JSON";

      expect(json.find(R"("message":"Basic diagnostic")") != std::string::npos);
      expect(json.find(R"("message":"Advanced diagnostic")") != std::string::npos);
      expect(json.find(R"("additional_info":"Extra details")") != std::string::npos);

      DiagnosticsConfig parsed_config;
      auto read_result = glz::read_json(parsed_config, json);
      expect(!read_result) << glz::format_error(read_result, json);

      expect(parsed_config.diagnostics.size() == 2);
      expect(parsed_config.diagnostics.contains("site1"));
      expect(parsed_config.diagnostics.contains("site2"));

      auto& variant1 = parsed_config.diagnostics["site1"];
      expect(std::holds_alternative<std::unique_ptr<SiteDiagnostic>>(variant1));
      if (std::holds_alternative<std::unique_ptr<SiteDiagnostic>>(variant1)) {
         auto& diag1 = std::get<std::unique_ptr<SiteDiagnostic>>(variant1);
         expect(diag1 != nullptr);
         expect(diag1->message == "Basic diagnostic");
         expect(diag1->severity == 1);
      }

      auto& variant2 = parsed_config.diagnostics["site2"];
      expect(std::holds_alternative<std::unique_ptr<DerivedSite0>>(variant2));
      if (std::holds_alternative<std::unique_ptr<DerivedSite0>>(variant2)) {
         auto& diag2 = std::get<std::unique_ptr<DerivedSite0>>(variant2);
         expect(diag2 != nullptr);
         expect(diag2->message == "Advanced diagnostic");
         expect(diag2->severity == 2);
         expect(diag2->additional_info == "Extra details");
      }
   };

   "flat_map with variant unique_ptr - empty map"_test = [] {
      DiagnosticsConfig config;

      std::string json;
      expect(!glz::write_json(config, json));
      expect(json == R"({"diagnostics":{}})") << json;

      DiagnosticsConfig parsed;
      expect(!glz::read_json(parsed, json));
      expect(parsed.diagnostics.empty());
   };

   "flat_map with variant unique_ptr - single entry"_test = [] {
      DiagnosticsConfig config;
      config.diagnostics["only_site"] = std::make_unique<DerivedSite0>(DerivedSite0{"Test", 3, "Info"});

      std::string json;
      expect(!glz::write_json(config, json));

      DiagnosticsConfig parsed;
      expect(!glz::read_json(parsed, json));
      expect(parsed.diagnostics.size() == 1);
      expect(parsed.diagnostics.contains("only_site"));

      auto& variant = parsed.diagnostics["only_site"];
      expect(std::holds_alternative<std::unique_ptr<DerivedSite0>>(variant));
   };

   "tagged variant with unique_ptr - tags are written"_test = [] {
      SiteDiagnosticsVariant variant = std::make_unique<SiteDiagnostic>(SiteDiagnostic{"Test message", 5});

      std::string json;
      auto write_result = glz::write_json(variant, json);
      expect(!write_result) << "Failed to write JSON";

      expect(json.find(R"("type":"SiteDiagnostic")") != std::string::npos) << json;
      expect(json.find(R"("message":"Test message")") != std::string::npos) << json;
      expect(json.find(R"("severity":5)") != std::string::npos) << json;
   };

   "tagged variant with unique_ptr - roundtrip with tags"_test = [] {
      SiteDiagnosticsVariant original = std::make_unique<DerivedSite0>(DerivedSite0{"Derived message", 7, "Extra"});

      std::string json;
      expect(!glz::write_json(original, json)) << "Write failed";

      expect(json.find(R"("type":"DerivedSite0")") != std::string::npos) << json;

      SiteDiagnosticsVariant parsed;
      auto read_result = glz::read_json(parsed, json);
      expect(!read_result) << glz::format_error(read_result, json);

      expect(std::holds_alternative<std::unique_ptr<DerivedSite0>>(parsed));
      if (std::holds_alternative<std::unique_ptr<DerivedSite0>>(parsed)) {
         auto& diag = std::get<std::unique_ptr<DerivedSite0>>(parsed);
         expect(diag != nullptr);
         expect(diag->message == "Derived message");
         expect(diag->severity == 7);
         expect(diag->additional_info == "Extra");
      }
   };

   "tagged variant with unique_ptr - flat_map preserves tags"_test = [] {
      DiagnosticsConfig config;
      config.diagnostics["a"] = std::make_unique<SiteDiagnostic>(SiteDiagnostic{"Msg A", 1});
      config.diagnostics["b"] = std::make_unique<DerivedSite0>(DerivedSite0{"Msg B", 2, "Details"});

      std::string json;
      expect(!glz::write_json(config, json));

      expect(json.find(R"("type":"SiteDiagnostic")") != std::string::npos) << json;
      expect(json.find(R"("type":"DerivedSite0")") != std::string::npos) << json;

      DiagnosticsConfig parsed;
      expect(!glz::read_json(parsed, json)) << json;

      expect(parsed.diagnostics.size() == 2);
      expect(std::holds_alternative<std::unique_ptr<SiteDiagnostic>>(parsed.diagnostics["a"]));
      expect(std::holds_alternative<std::unique_ptr<DerivedSite0>>(parsed.diagnostics["b"]));
   };
};

struct Animal
{
   std::string name{};
   int age{};
};

template <>
struct glz::meta<Animal>
{
   using T = Animal;
   static constexpr std::string_view name = "Animal";
   static constexpr auto value = object(&T::name, &T::age);
};

struct Dog
{
   std::string name{};
   int age{};
   std::string breed{};
};

template <>
struct glz::meta<Dog>
{
   using T = Dog;
   static constexpr std::string_view name = "Dog";
   static constexpr auto value = object(&T::name, &T::age, &T::breed);
};

using AnimalVariantShared = std::variant<std::shared_ptr<Animal>, std::shared_ptr<Dog>>;

template <>
struct glz::meta<AnimalVariantShared>
{
   static constexpr std::string_view tag = "species";
   static constexpr auto ids = std::array{"Animal", "Dog"};
};

suite shared_ptr_variant_tests = [] {
   "tagged variant with shared_ptr - tags are written"_test = [] {
      AnimalVariantShared variant = std::make_shared<Animal>(Animal{"Buddy", 3});

      std::string json;
      auto write_result = glz::write_json(variant, json);
      expect(!write_result) << "Failed to write JSON";

      expect(json.find(R"("species":"Animal")") != std::string::npos) << json;
      expect(json.find(R"("name":"Buddy")") != std::string::npos) << json;
      expect(json.find(R"("age":3)") != std::string::npos) << json;
   };

   "tagged variant with shared_ptr - roundtrip with tags"_test = [] {
      AnimalVariantShared original = std::make_shared<Dog>(Dog{"Max", 5, "Golden Retriever"});

      std::string json;
      expect(!glz::write_json(original, json)) << "Write failed";

      expect(json.find(R"("species":"Dog")") != std::string::npos) << json;

      AnimalVariantShared parsed;
      auto read_result = glz::read_json(parsed, json);
      expect(!read_result) << glz::format_error(read_result, json);

      expect(std::holds_alternative<std::shared_ptr<Dog>>(parsed));
      if (std::holds_alternative<std::shared_ptr<Dog>>(parsed)) {
         auto& dog = std::get<std::shared_ptr<Dog>>(parsed);
         expect(dog != nullptr);
         expect(dog->name == "Max");
         expect(dog->age == 5);
         expect(dog->breed == "Golden Retriever");
      }
   };

   "tagged variant with shared_ptr - multiple instances"_test = [] {
      std::vector<AnimalVariantShared> animals;
      animals.push_back(std::make_shared<Animal>(Animal{"Cat", 2}));
      animals.push_back(std::make_shared<Dog>(Dog{"Rover", 4, "Labrador"}));
      animals.push_back(std::make_shared<Animal>(Animal{"Bird", 1}));

      std::string json;
      expect(!glz::write_json(animals, json));

      std::vector<AnimalVariantShared> parsed;
      expect(!glz::read_json(parsed, json)) << json;

      expect(parsed.size() == 3);
      expect(std::holds_alternative<std::shared_ptr<Animal>>(parsed[0]));
      expect(std::holds_alternative<std::shared_ptr<Dog>>(parsed[1]));
      expect(std::holds_alternative<std::shared_ptr<Animal>>(parsed[2]));

      if (std::holds_alternative<std::shared_ptr<Dog>>(parsed[1])) {
         auto& dog = std::get<std::shared_ptr<Dog>>(parsed[1]);
         expect(dog->breed == "Labrador");
      }
   };
};

// Test for GitHub issue #2172
// Empty struct in tagged variant roundtrip
namespace tagged_empty_variant
{
   struct EmptyA
   {
      bool operator==(const EmptyA&) const = default;
   };

   using VariantWithEmpty = std::variant<EmptyA>;

   struct Wrapper
   {
      VariantWithEmpty v;
      bool operator==(const Wrapper&) const = default;
   };
}

template <>
struct glz::meta<tagged_empty_variant::EmptyA>
{
   static constexpr auto value = glz::object();
};

template <>
struct glz::meta<tagged_empty_variant::VariantWithEmpty>
{
   static constexpr std::string_view tag = "tag";
   static constexpr auto ids = std::array{"A"};
};

template <>
struct glz::meta<tagged_empty_variant::Wrapper>
{
   static constexpr auto value = glz::object(&tagged_empty_variant::Wrapper::v);
};

suite empty_struct_variant_roundtrip = [] {
   "empty struct variant roundtrip minified"_test = [] {
      tagged_empty_variant::Wrapper x{tagged_empty_variant::EmptyA{}};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));
      expect(buffer == R"({"v":{"tag":"A"}})") << buffer;

      tagged_empty_variant::Wrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(parsed == x);
   };

   "empty struct variant roundtrip prettified"_test = [] {
      tagged_empty_variant::Wrapper x{tagged_empty_variant::EmptyA{}};
      std::string buffer{};
      expect(not glz::write<glz::opts{.prettify = true}>(x, buffer));

      // Verify no extra blank lines in prettified output
      expect(buffer == R"({
   "v": {
      "tag": "A"
   }
})") << buffer;

      tagged_empty_variant::Wrapper parsed{};
      auto ec = glz::read<glz::opts{.prettify = true}>(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(parsed == x);
   };

   "empty struct variant full roundtrip"_test = [] {
      // Test that write -> read roundtrip works for both minified and prettified
      tagged_empty_variant::Wrapper original{tagged_empty_variant::EmptyA{}};

      // Minified roundtrip
      std::string minified{};
      expect(not glz::write_json(original, minified));
      tagged_empty_variant::Wrapper parsed_min{};
      expect(not glz::read_json(parsed_min, minified)) << minified;
      expect(parsed_min == original);

      // Prettified roundtrip
      std::string prettified{};
      expect(not glz::write<glz::opts{.prettify = true}>(original, prettified));
      tagged_empty_variant::Wrapper parsed_pretty{};
      expect(not glz::read<glz::opts{.prettify = true}>(parsed_pretty, prettified)) << prettified;
      expect(parsed_pretty == original);
   };

   "empty struct variant direct parsing"_test = [] {
      // Test parsing the variant directly (not wrapped in another struct)
      std::string json = R"({"tag":"A"})";
      tagged_empty_variant::VariantWithEmpty v;
      auto ec = glz::read_json(v, json);
      expect(not ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(v));
   };

   "empty struct variant with extra whitespace"_test = [] {
      // Test parsing with various whitespace patterns
      std::string json = R"({  "v"  :  {  "tag"  :  "A"  }  })";
      tagged_empty_variant::Wrapper parsed{};
      auto ec = glz::read_json(parsed, json);
      expect(not ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed.v));
   };
};

// Additional test types for more comprehensive coverage
namespace tagged_variant_extended
{
   // Multiple empty struct types in a variant
   struct EmptyB
   {
      bool operator==(const EmptyB&) const = default;
   };

   struct EmptyC
   {
      bool operator==(const EmptyC&) const = default;
   };

   using MultiEmptyVariant = std::variant<tagged_empty_variant::EmptyA, EmptyB, EmptyC>;

   struct MultiEmptyWrapper
   {
      MultiEmptyVariant v;
      bool operator==(const MultiEmptyWrapper&) const = default;
   };

   // Mixed variant with empty and non-empty types
   struct NonEmpty
   {
      int x{};
      std::string y{};
      bool operator==(const NonEmpty&) const = default;
   };

   using MixedVariant = std::variant<tagged_empty_variant::EmptyA, NonEmpty>;

   struct MixedWrapper
   {
      MixedVariant v;
      bool operator==(const MixedWrapper&) const = default;
   };

   // Wrapper with multiple fields including variant
   struct MultiFieldWrapper
   {
      int before{};
      tagged_empty_variant::VariantWithEmpty v;
      std::string after{};
      bool operator==(const MultiFieldWrapper&) const = default;
   };

   // Nested wrapper
   struct OuterWrapper
   {
      tagged_empty_variant::Wrapper inner;
      bool operator==(const OuterWrapper&) const = default;
   };
}

template <>
struct glz::meta<tagged_variant_extended::EmptyB>
{
   static constexpr auto value = glz::object();
};

template <>
struct glz::meta<tagged_variant_extended::EmptyC>
{
   static constexpr auto value = glz::object();
};

template <>
struct glz::meta<tagged_variant_extended::MultiEmptyVariant>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"A", "B", "C"};
};

template <>
struct glz::meta<tagged_variant_extended::MultiEmptyWrapper>
{
   static constexpr auto value = glz::object(&tagged_variant_extended::MultiEmptyWrapper::v);
};

template <>
struct glz::meta<tagged_variant_extended::NonEmpty>
{
   static constexpr auto value =
      glz::object(&tagged_variant_extended::NonEmpty::x, &tagged_variant_extended::NonEmpty::y);
};

template <>
struct glz::meta<tagged_variant_extended::MixedVariant>
{
   static constexpr std::string_view tag = "kind";
   static constexpr auto ids = std::array{"empty", "non_empty"};
};

template <>
struct glz::meta<tagged_variant_extended::MixedWrapper>
{
   static constexpr auto value = glz::object(&tagged_variant_extended::MixedWrapper::v);
};

template <>
struct glz::meta<tagged_variant_extended::MultiFieldWrapper>
{
   static constexpr auto value =
      glz::object(&tagged_variant_extended::MultiFieldWrapper::before, &tagged_variant_extended::MultiFieldWrapper::v,
                  &tagged_variant_extended::MultiFieldWrapper::after);
};

template <>
struct glz::meta<tagged_variant_extended::OuterWrapper>
{
   static constexpr auto value = glz::object(&tagged_variant_extended::OuterWrapper::inner);
};

suite empty_struct_variant_extended = [] {
   using namespace tagged_variant_extended;

   "multiple empty types in variant - type A"_test = [] {
      MultiEmptyWrapper x{tagged_empty_variant::EmptyA{}};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));
      expect(buffer == R"({"v":{"type":"A"}})") << buffer;

      MultiEmptyWrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed.v));
   };

   "multiple empty types in variant - type B"_test = [] {
      MultiEmptyWrapper x{EmptyB{}};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));
      expect(buffer == R"({"v":{"type":"B"}})") << buffer;

      MultiEmptyWrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<EmptyB>(parsed.v));
   };

   "multiple empty types in variant - type C"_test = [] {
      MultiEmptyWrapper x{EmptyC{}};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));
      expect(buffer == R"({"v":{"type":"C"}})") << buffer;

      MultiEmptyWrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<EmptyC>(parsed.v));
   };

   "mixed variant with empty type"_test = [] {
      MixedWrapper x{tagged_empty_variant::EmptyA{}};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));
      expect(buffer == R"({"v":{"kind":"empty"}})") << buffer;

      MixedWrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed.v));
   };

   "mixed variant with non-empty type"_test = [] {
      MixedWrapper x{NonEmpty{42, "hello"}};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));

      MixedWrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<NonEmpty>(parsed.v));
      expect(std::get<NonEmpty>(parsed.v).x == 42);
      expect(std::get<NonEmpty>(parsed.v).y == "hello");
   };

   "multi-field wrapper with empty variant"_test = [] {
      MultiFieldWrapper x{10, tagged_empty_variant::EmptyA{}, "test"};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));

      MultiFieldWrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(parsed.before == 10);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed.v));
      expect(parsed.after == "test");
   };

   "nested wrapper with empty variant"_test = [] {
      OuterWrapper x{{tagged_empty_variant::EmptyA{}}};
      std::string buffer{};
      expect(not glz::write_json(x, buffer));
      expect(buffer == R"({"inner":{"v":{"tag":"A"}}})") << buffer;

      OuterWrapper parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed.inner.v));
   };

   "nested wrapper prettified"_test = [] {
      OuterWrapper x{{tagged_empty_variant::EmptyA{}}};
      std::string buffer{};
      expect(not glz::write<glz::opts{.prettify = true}>(x, buffer));

      OuterWrapper parsed{};
      auto ec = glz::read<glz::opts{.prettify = true}>(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed.inner.v));
   };

   "multi-field wrapper field order variations"_test = [] {
      // Test that field order in JSON doesn't matter
      std::string json1 = R"({"before":5,"v":{"tag":"A"},"after":"x"})";
      std::string json2 = R"({"v":{"tag":"A"},"before":5,"after":"x"})";
      std::string json3 = R"({"after":"x","before":5,"v":{"tag":"A"}})";

      for (const auto& json : {json1, json2, json3}) {
         MultiFieldWrapper parsed{};
         auto ec = glz::read_json(parsed, json);
         expect(not ec) << glz::format_error(ec, json);
         expect(parsed.before == 5);
         expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed.v));
         expect(parsed.after == "x");
      }
   };

   "empty variant in array"_test = [] {
      std::vector<tagged_empty_variant::Wrapper> vec{{tagged_empty_variant::EmptyA{}},
                                                     {tagged_empty_variant::EmptyA{}}};
      std::string buffer{};
      expect(not glz::write_json(vec, buffer));
      expect(buffer == R"([{"v":{"tag":"A"}},{"v":{"tag":"A"}}])") << buffer;

      std::vector<tagged_empty_variant::Wrapper> parsed{};
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(parsed.size() == 2);
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed[0].v));
      expect(std::holds_alternative<tagged_empty_variant::EmptyA>(parsed[1].v));
   };
};

// ============================================================================
// Tests for auto-inferred custom types in variants (issue #1641)
// ============================================================================

// Custom type with glz::custom that reads/writes as a number
struct Amount
{
   double value{};
   bool operator==(const Amount&) const = default;
};

template <>
struct glz::meta<Amount>
{
   static constexpr auto read_fn = [](Amount& a, const double& input) { a.value = input; };
   static constexpr auto write_fn = [](const Amount& a) -> const double& { return a.value; };
   static constexpr auto value = glz::custom<read_fn, write_fn>;
};

// Custom type with glz::custom that reads/writes as a string
struct Label
{
   std::string text{};
   bool operator==(const Label&) const = default;
   auto operator<=>(const Label&) const = default;
};

template <>
struct glz::meta<Label>
{
   static constexpr auto read_fn = [](Label& l, const std::string& input) { l.text = input; };
   static constexpr auto write_fn = [](const Label& l) -> const std::string& { return l.text; };
   static constexpr auto value = glz::custom<read_fn, write_fn>;
};

// Custom type with glz::custom that reads/writes as a bool
struct Flag
{
   bool state{};
   bool operator==(const Flag&) const = default;
};

template <>
struct glz::meta<Flag>
{
   static constexpr auto read_fn = [](Flag& f, const bool& input) { f.state = input; };
   static constexpr auto write_fn = [](const Flag& f) -> const bool& { return f.state; };
   static constexpr auto value = glz::custom<read_fn, write_fn>;
};

// Custom type using mimic for numeric behavior
struct MimicNumber
{
   int value{};
   bool operator==(const MimicNumber&) const = default;
};

template <>
struct glz::meta<MimicNumber>
{
   using mimic = int;
   static constexpr auto value = &MimicNumber::value;
};

// Custom type using mimic for string behavior (map key test)
struct StringKey
{
   std::string key{};
   auto operator<=>(const StringKey&) const = default;
};

template <>
struct glz::meta<StringKey>
{
   using mimic = std::string;
   static constexpr auto value = &StringKey::key;
};

suite auto_inferred_custom_types = [] {
   "custom_num_t concept check"_test = [] {
      static_assert(glz::custom_num_t<Amount>);
      static_assert(!glz::custom_str_t<Amount>);
      static_assert(!glz::custom_bool_t<Amount>);
   };

   "custom_str_t concept check"_test = [] {
      static_assert(glz::custom_str_t<Label>);
      static_assert(!glz::custom_num_t<Label>);
      static_assert(!glz::custom_bool_t<Label>);
   };

   "custom_bool_t concept check"_test = [] {
      static_assert(glz::custom_bool_t<Flag>);
      static_assert(!glz::custom_num_t<Flag>);
      static_assert(!glz::custom_str_t<Flag>);
   };

   "auto-inferred numeric type in variant"_test = [] {
      std::variant<std::string, Amount> v;

      // Parse as Amount (number)
      auto ec = glz::read_json(v, "42.5");
      expect(not ec) << glz::format_error(ec, "42.5");
      expect(std::holds_alternative<Amount>(v));
      expect(std::get<Amount>(v).value == 42.5);

      // Parse as string
      ec = glz::read_json(v, R"("hello")");
      expect(not ec) << glz::format_error(ec, R"("hello")");
      expect(std::holds_alternative<std::string>(v));
      expect(std::get<std::string>(v) == "hello");
   };

   "auto-inferred string type in variant"_test = [] {
      std::variant<int, Label> v;

      // Parse as Label (string)
      auto ec = glz::read_json(v, R"("world")");
      expect(not ec) << glz::format_error(ec, R"("world")");
      expect(std::holds_alternative<Label>(v));
      expect(std::get<Label>(v).text == "world");

      // Parse as int
      ec = glz::read_json(v, "123");
      expect(not ec) << glz::format_error(ec, "123");
      expect(std::holds_alternative<int>(v));
      expect(std::get<int>(v) == 123);
   };

   "auto-inferred bool type in variant"_test = [] {
      std::variant<std::string, Flag> v;

      // Parse as Flag (bool)
      auto ec = glz::read_json(v, "true");
      expect(not ec) << glz::format_error(ec, "true");
      expect(std::holds_alternative<Flag>(v));
      expect(std::get<Flag>(v).state == true);

      ec = glz::read_json(v, "false");
      expect(not ec) << glz::format_error(ec, "false");
      expect(std::holds_alternative<Flag>(v));
      expect(std::get<Flag>(v).state == false);

      // Parse as string
      ec = glz::read_json(v, R"("text")");
      expect(not ec) << glz::format_error(ec, R"("text")");
      expect(std::holds_alternative<std::string>(v));
   };

   "auto-inferred type roundtrip"_test = [] {
      std::variant<std::string, Amount> v = Amount{99.5};
      std::string buffer;
      expect(not glz::write_json(v, buffer));
      expect(buffer == "99.5") << buffer;

      std::variant<std::string, Amount> parsed;
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(std::holds_alternative<Amount>(parsed));
      expect(std::get<Amount>(parsed).value == 99.5);
   };

   "multiple auto-inferred types in variant"_test = [] {
      std::variant<Amount, Label, Flag> v;

      // Number -> Amount
      auto ec = glz::read_json(v, "3.14");
      expect(not ec);
      expect(std::holds_alternative<Amount>(v));

      // String -> Label
      ec = glz::read_json(v, R"("test")");
      expect(not ec);
      expect(std::holds_alternative<Label>(v));

      // Bool -> Flag
      ec = glz::read_json(v, "true");
      expect(not ec);
      expect(std::holds_alternative<Flag>(v));
   };
};

suite mimic_types_in_variants = [] {
   "mimics_num_t concept check"_test = [] {
      static_assert(glz::has_mimic<MimicNumber>);
      static_assert(glz::mimics_num_t<MimicNumber>);
      static_assert(!glz::mimics_str_t<MimicNumber>);
   };

   "mimics_str_t concept check"_test = [] {
      static_assert(glz::has_mimic<StringKey>);
      static_assert(glz::mimics_str_t<StringKey>);
      static_assert(!glz::mimics_num_t<StringKey>);
   };

   "mimic numeric type in variant"_test = [] {
      std::variant<std::string, MimicNumber> v;

      auto ec = glz::read_json(v, "42");
      expect(not ec) << glz::format_error(ec, "42");
      expect(std::holds_alternative<MimicNumber>(v));
      expect(std::get<MimicNumber>(v).value == 42);

      ec = glz::read_json(v, R"("text")");
      expect(not ec);
      expect(std::holds_alternative<std::string>(v));
   };

   "mimic string type in variant"_test = [] {
      std::variant<int, StringKey> v;

      auto ec = glz::read_json(v, R"("mykey")");
      expect(not ec) << glz::format_error(ec, R"("mykey")");
      expect(std::holds_alternative<StringKey>(v));
      expect(std::get<StringKey>(v).key == "mykey");

      ec = glz::read_json(v, "100");
      expect(not ec);
      expect(std::holds_alternative<int>(v));
   };

   "mimic string as map key (no double quoting)"_test = [] {
      std::map<StringKey, int> m{{StringKey{"hello"}, 42}, {StringKey{"world"}, 99}};
      std::string buffer;
      expect(not glz::write_json(m, buffer));
      // Should be {"hello":42,"world":99} not {"\"hello\"":42,...}
      expect(buffer.find("\"hello\"") != std::string::npos) << buffer;
      expect(buffer.find("\"\"hello\"\"") == std::string::npos) << buffer;

      std::map<StringKey, int> parsed;
      auto ec = glz::read_json(parsed, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(parsed.size() == 2);
      expect(parsed[StringKey{"hello"}] == 42);
      expect(parsed[StringKey{"world"}] == 99);
   };
};

suite custom_type_schema_generation = [] {
   "schema for auto-inferred numeric type"_test = [] {
      std::string schema = glz::write_json_schema<Amount>().value_or("error");
      // Schema uses string format for single types: "type":"number"
      expect(schema.find("\"number\"") != std::string::npos) << schema;
   };

   "schema for auto-inferred string type"_test = [] {
      std::string schema = glz::write_json_schema<Label>().value_or("error");
      expect(schema.find("\"string\"") != std::string::npos) << schema;
   };

   "schema for auto-inferred bool type"_test = [] {
      std::string schema = glz::write_json_schema<Flag>().value_or("error");
      expect(schema.find("\"boolean\"") != std::string::npos) << schema;
   };

   "schema for mimic numeric type"_test = [] {
      std::string schema = glz::write_json_schema<MimicNumber>().value_or("error");
      expect(schema.find("\"integer\"") != std::string::npos) << schema;
   };

   "schema for mimic string type"_test = [] {
      std::string schema = glz::write_json_schema<StringKey>().value_or("error");
      expect(schema.find("\"string\"") != std::string::npos) << schema;
   };
};

// Regression test for https://github.com/stephenberry/glaze/issues/2529
// vector<pair<...>> is parsed from a JSON object in concatenate mode (default),
// so the variant deduction must treat it as an object alternative.
suite vector_pair_object_variant_tests = [] {
   using map_type = std::vector<std::pair<std::string_view, std::string_view>>;

   "vector<pair> direct read"_test = [] {
      map_type m;
      auto ec = glz::read_json(m, R"({"foo":"bar"})");
      expect(!ec) << glz::format_error(ec, std::string{R"({"foo":"bar"})"});
      expect(m.size() == 1);
      expect(m[0].first == "foo");
      expect(m[0].second == "bar");
   };

   "variant with vector<pair> alternative"_test = [] {
      std::variant<std::string_view, std::nullptr_t, map_type> v;
      auto ec = glz::read_json(v, R"({"foo":"bar"})");
      expect(!ec) << glz::format_error(ec, std::string{R"({"foo":"bar"})"});
      expect(v.index() == 2);
      auto& m = std::get<2>(v);
      expect(m.size() == 1);
      expect(m[0].first == "foo");
      expect(m[0].second == "bar");
   };

   "variant deduces null/string/object alternatives"_test = [] {
      std::variant<std::string_view, std::nullptr_t, map_type> v;

      auto ec = glz::read_json(v, R"(null)");
      expect(!ec);
      expect(v.index() == 1);

      ec = glz::read_json(v, R"("hello")");
      expect(!ec);
      expect(v.index() == 0);
      expect(std::get<0>(v) == "hello");

      ec = glz::read_json(v, R"({"k":"v"})");
      expect(!ec);
      expect(v.index() == 2);
      expect(std::get<2>(v).size() == 1);
   };

   // With concatenate=false the user opts out of object-shaped pair-ranges,
   // so the variant must NOT match vector<pair> against `{...}` JSON.
   "variant with concatenate=false does not match object to vector<pair>"_test = [] {
      struct opts_concat : glz::opts
      {
         bool concatenate = true;
      };
      static constexpr opts_concat cat_off{{glz::opts{}}, false};

      std::variant<std::string_view, std::nullptr_t, map_type> v;
      auto ec = glz::read<cat_off>(v, R"({"k":"v"})");
      expect(bool(ec));
      expect(ec == glz::error_code::no_matching_variant_type);
   };
};

// Regression coverage for https://github.com/stephenberry/glaze/issues/2589
//
// A tagged-variant alternative that uses custom_read/custom_write (for example to access
// the parse context) and serializes an object body should merge the variant tag into that
// body and round-trip, even alongside an empty (fieldless) alternative.

struct custom_map_action
{
   std::map<std::string, int>& data() { return m_data; }
   const std::map<std::string, int>& data() const { return m_data; }
   bool operator==(const custom_map_action&) const = default;

  private:
   std::map<std::string, int> m_data{};
};

template <>
struct glz::meta<custom_map_action>
{
   static constexpr auto custom_read = true;
   static constexpr auto custom_write = true;
};

template <uint32_t Format>
struct glz::from<Format, custom_map_action>
{
   template <auto Opts>
   static void op(custom_map_action& value, is_context auto&& ctx, auto&& it, auto&& end)
   {
      glz::parse<Format>::template op<Opts>(value.data(), ctx, it, end);
   }
};

template <uint32_t Format>
struct glz::to<Format, custom_map_action>
{
   template <auto Opts>
   static void op(auto&& value, is_context auto&& ctx, auto&&... args)
   {
      glz::serialize<Format>::template op<Opts>(value.data(), ctx, args...);
   }
};

struct empty_action
{
   bool operator==(const empty_action&) const = default;
};

template <>
struct glz::meta<empty_action>
{};

using custom_tagged_variant = std::variant<custom_map_action, empty_action>;

template <>
struct glz::meta<custom_tagged_variant>
{
   static constexpr std::string_view tag = "action";
   static constexpr auto ids = std::array{"PUT", "DELETE"};
};

suite custom_alternative_variant_tests = [] {
   "custom alternative tagged variant write+read"_test = [] {
      custom_map_action put{};
      put.data() = {{"a", 1}, {"b", 2}};
      std::vector<custom_tagged_variant> v{empty_action{}, custom_map_action{}, put};

      std::string s{};
      expect(not glz::write_json(v, s));
      expect(s == R"([{"action":"DELETE"},{"action":"PUT"},{"action":"PUT","a":1,"b":2}])") << s;

      std::vector<custom_tagged_variant> parsed{};
      expect(not glz::read_json(parsed, s));
      expect(parsed.size() == 3);
      expect(std::holds_alternative<empty_action>(parsed[0]));
      expect(std::holds_alternative<custom_map_action>(parsed[1]));
      expect(std::get<custom_map_action>(parsed[1]).data().empty());
      expect(std::holds_alternative<custom_map_action>(parsed[2]));
      expect(std::get<custom_map_action>(parsed[2]).data().at("a") == 1);
      expect(std::get<custom_map_action>(parsed[2]).data().at("b") == 2);
   };

   "custom alternative prettify round-trips"_test = [] {
      custom_map_action put{};
      put.data() = {{"x", 10}};
      custom_tagged_variant v{put};

      std::string s{};
      expect(not glz::write<glz::opts{.prettify = true}>(v, s));
      custom_tagged_variant parsed{};
      expect(not glz::read_json(parsed, s));
      std::string s2{};
      expect(not glz::write<glz::opts{.prettify = true}>(parsed, s2));
      expect(s == s2) << s;
   };

   "empty custom alternative prettify has no trailing comma"_test = [] {
      custom_tagged_variant v{custom_map_action{}};
      std::string s{};
      expect(not glz::write<glz::opts{.prettify = true}>(v, s));
      expect(s == "{\n   \"action\": \"PUT\"\n}") << s;
   };

   // A custom alternative receives the object body directly and cannot skip an interior
   // tag the way reflected objects do, so the tag must be the first key. glaze always
   // writes it first; reordered (e.g. hand-written) input must error rather than corrupt.
   "custom alternative requires the tag first"_test = [] {
      // tag first: parses the body
      custom_tagged_variant ok{};
      expect(not glz::read_json(ok, R"({"action":"PUT","a":1})"));
      expect(std::holds_alternative<custom_map_action>(ok));
      expect(std::get<custom_map_action>(ok).data().at("a") == 1);

      // tag after another key, error_on_unknown_keys = false: must not silently absorb
      // the tag into the custom value
      custom_tagged_variant corrupt{};
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(corrupt, R"({"a":1,"action":"PUT"})");
      expect(bool(ec));
      expect(ec == glz::error_code::feature_not_supported);

      // tag after another key, default keys handling: unknown key is reported
      custom_tagged_variant unknown{};
      expect(bool(glz::read_json(unknown, R"({"a":1,"action":"PUT"})")));
   };
};

// A std::variant whose own meta opts into custom_read/custom_write (with full from/to
// specializations) must not be ambiguous with the built-in variant handlers.

struct fully_custom_a
{};
struct fully_custom_b
{};
using fully_custom_variant = std::variant<fully_custom_a, fully_custom_b>;

template <>
struct glz::meta<fully_custom_variant>
{
   static constexpr auto custom_read = true;
   static constexpr auto custom_write = true;
};

template <uint32_t Format>
struct glz::from<Format, fully_custom_variant>
{
   template <auto Opts>
   static void op(fully_custom_variant&, is_context auto&&, auto&&, auto&&)
   {}
};

template <uint32_t Format>
struct glz::to<Format, fully_custom_variant>
{
   template <auto Opts>
   static void op(auto&&, is_context auto&& ctx, auto&&... args)
   {
      glz::serialize<Format>::template op<Opts>("custom", ctx, args...);
   }
};

suite fully_custom_variant_tests = [] {
   "fully custom variant specialization is unambiguous"_test = [] {
      fully_custom_variant v{};
      std::string s{};
      expect(not glz::write_json(v, s));
      expect(s == R"("custom")") << s;
   };
};

// ---------------------------------------------------------------------------------------------
// Variant tagging representation: the wire shape is chosen per variant, not per active alternative.
// ---------------------------------------------------------------------------------------------

namespace tagging_repr
{
   struct circle
   {
      double radius{};
   };

   // Every JSON shape in one variant. Internal tagging cannot represent this -- there is nowhere to
   // merge a key into an array, a scalar, or null -- so it declares `content`.
   using mixed = std::variant<circle, std::vector<double>, std::map<std::string, int>, int, std::monostate>;

   // Two alternatives with the same wire shape. Under internal tagging the discriminator is dropped
   // for both and the read silently returns the first; adjacent tagging separates them.
   using same_shape = std::variant<std::vector<double>, std::deque<double>>;

   struct int_a
   {
      int a{};
   };
   struct int_b
   {
      int b{};
   };
   using integral_ids = std::variant<int_a, int_b, double>;

   // Fewer ids than alternatives: the first unlabeled alternative is the read-side default, and the
   // writer has no id to emit for it.
   struct few_a
   {
      int a{};
   };
   struct few_b
   {
      int b{};
   };
   struct few_c
   {
      int c{};
   };
   using few_ids = std::variant<few_a, few_b, few_c>;

   // A unit alternative under *internal* tagging: written as the discriminator alone.
   struct put
   {
      int x{};
   };
   struct del
   {
      std::string id{};
   };
   using with_none = std::variant<put, del, std::monostate>;

   // A glaze_object_t alternative that declares a member named like the discriminator.
   struct owns_tag
   {
      std::string kind{};
      int x{};
   };
   struct plain
   {
      double y{};
   };
   using declares_tag = std::variant<owns_tag, plain>;
}

template <>
struct glz::meta<tagging_repr::mixed>
{
   static constexpr std::string_view tag = "type";
   static constexpr std::string_view content = "value";
   static constexpr auto ids = std::array{"circle", "vec", "map", "num", "none"};
};

template <>
struct glz::meta<tagging_repr::same_shape>
{
   static constexpr std::string_view tag = "type";
   static constexpr std::string_view content = "value";
   static constexpr auto ids = std::array{"vec", "deq"};
};

template <>
struct glz::meta<tagging_repr::integral_ids>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::string_view content = "c";
   static constexpr auto ids = std::array{10, 20, 30};
};

template <>
struct glz::meta<tagging_repr::few_ids>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::string_view content = "c";
   static constexpr auto ids = std::array{"a", "b"};
};

template <>
struct glz::meta<tagging_repr::with_none>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"PUT", "DELETE", "NONE"};
};

template <>
struct glz::meta<tagging_repr::owns_tag>
{
   using T = tagging_repr::owns_tag;
   static constexpr auto value = glz::object("kind", &T::kind, "x", &T::x);
};

template <>
struct glz::meta<tagging_repr::declares_tag>
{
   static constexpr std::string_view tag = "kind";
   static constexpr auto ids = std::array{"owns", "plain"};
};

suite adjacent_tagging_tests = [] {
   using namespace tagging_repr;

   "adjacent tagging gives every alternative the same shape"_test = [] {
      const auto shape = [](auto alt, std::string_view expected) {
         mixed v{alt};
         auto s = glz::write_json(v);
         expect(s.has_value());
         expect(*s == expected) << *s;
         mixed out{};
         const auto ec = glz::read_json(out, *s);
         expect(!ec) << glz::format_error(ec, *s);
         expect(out.index() == v.index());
      };
      shape(circle{5}, R"({"type":"circle","value":{"radius":5}})");
      shape(std::vector<double>{1, 2}, R"({"type":"vec","value":[1,2]})");
      shape(std::map<std::string, int>{{"k", 1}}, R"({"type":"map","value":{"k":1}})");
      shape(42, R"({"type":"num","value":42})");
      shape(std::monostate{}, R"({"type":"none","value":null})");
   };

   "adjacent tagging separates alternatives that share a wire shape"_test = [] {
      // The defect this representation exists for: under internal tagging both alternatives write a
      // bare array, and the read always returns the first one with no error reported.
      same_shape v = std::deque<double>{1, 2, 3};
      auto s = glz::write_json(v);
      expect(s.has_value());
      expect(*s == R"({"type":"deq","value":[1,2,3]})") << *s;

      same_shape out{};
      expect(not glz::read_json(out, *s));
      expect(out.index() == 1);
      expect(std::get<1>(out) == std::deque<double>{1, 2, 3});
   };

   "adjacent tagging round-trips integral ids"_test = [] {
      integral_ids v{3.5};
      auto s = glz::write_json(v);
      expect(s.has_value());
      expect(*s == R"({"t":30,"c":3.5})") << *s;
      integral_ids out{};
      expect(not glz::read_json(out, *s));
      expect(out.index() == 2);
      expect(std::get<2>(out) == 3.5);
   };

   "adjacent tagging accepts either key order"_test = [] {
      same_shape out{};
      expect(not glz::read_json(out, R"({"value":[1,2,3],"type":"deq"})"));
      expect(out.index() == 1);
   };

   "adjacent tagging prettifies"_test = [] {
      std::string s{};
      expect(not glz::write<glz::opts{.prettify = true}>(mixed{circle{5}}, s));
      expect(s == "{\n   \"type\": \"circle\",\n   \"value\": {\n      \"radius\": 5\n   }\n}") << s;
      mixed out{};
      expect(not glz::read_json(out, s));
      expect(out.index() == 0);
   };

   "adjacent tagging rejects malformed objects"_test = [] {
      same_shape out{};
      expect(glz::read_json(out, R"({"type":"deq","value":[1],"extra":1})")); // unknown key
      expect(glz::read_json(out, R"({"type":"nope","value":[1]})")); // id names no alternative
      expect(glz::read_json(out, R"({"type":"deq"})")); // no content
      expect(glz::read_json(out, R"({"value":[1]})")); // no discriminator
      expect(glz::read_json(out, R"([1,2,3])")); // not the adjacent shape at all

      // Each declared key may appear once. Both duplicates report the same error; the content key
      // used to slip through the unknown-key check and be silently first-wins.
      expect(glz::read_json(out, R"({"type":"deq","value":[1],"value":[2]})") == glz::error_code::unknown_key);
      expect(glz::read_json(out, R"({"type":"deq","type":"vec","value":[1]})") == glz::error_code::unknown_key);

      // Unknown keys are tolerated when the caller asks for that.
      same_shape lax{};
      expect(not glz::read<glz::opts{.error_on_unknown_keys = false}>(lax, R"({"type":"deq","value":[1],"x":1})"));
      expect(lax.index() == 1);
   };

   "a truncated adjacent object is reported, not silently accepted"_test = [] {
      // Non-null-terminated reads (glz::read_streaming, and anything with null_terminated = false)
      // depend on ctx.depth staying elevated when a read bails out: finalize_read_context only
      // downgrades end_reached to success at depth 0. Restoring depth on the error path turned a
      // truncated stream into ec == none with the destination untouched.
      constexpr glz::opts streaming{.null_terminated = false};
      const std::string_view complete = R"({"type":"circle","value":{"radius":1}})";
      for (size_t n = 1; n < complete.size(); ++n) {
         mixed out{};
         const std::string prefix{complete.substr(0, n)};
         expect(bool(glz::read<streaming>(out, prefix))) << "accepted truncated prefix: " << prefix;
      }
      mixed whole{};
      expect(not glz::read<streaming>(whole, std::string{complete}));
      expect(whole.index() == 0);
   };

   "reading many adjacent variants does not leak depth"_test = [] {
      // Counting depth by hand (rather than with an RAII guard) must still balance on success, or a
      // long sequence of adjacent variants would falsely trip max_recursive_depth_limit.
      const std::vector<same_shape> many(400, same_shape{std::vector<double>{1}});
      const auto json = glz::write_json(many);
      expect(json.has_value());
      std::vector<same_shape> out{};
      expect(not glz::read_json(out, *json));
      expect(out.size() == 400);
      std::vector<same_shape> out_streaming{};
      expect(not glz::read<glz::opts{.null_terminated = false}>(out_streaming, *json));
      expect(out_streaming.size() == 400);
   };

   "an unlabeled alternative is the read default and is not writable"_test = [] {
      // `ids` is shorter than the alternative list. Reading an unrecognized id selects few_c; writing
      // few_c has no id to emit and must error rather than index ids_v past its end.
      few_ids out{};
      expect(not glz::read_json(out, R"({"t":"zzz","c":{"c":7}})"));
      expect(out.index() == 2);
      expect(std::get<2>(out).c == 7);

      std::string s{};
      expect(bool(glz::write_json(few_ids{few_c{7}}, s)));
   };
};

suite unit_alternative_tests = [] {
   using namespace tagging_repr;

   "a unit alternative is written as the discriminator alone"_test = [] {
      // std::monostate carries no data, so internal tagging renders it as the same
      // discriminator-only object an empty struct alternative produces. Writing it as a bare `null`
      // would leave one alternative of the union with no discriminator on it.
      with_none v{std::monostate{}};
      auto s = glz::write_json(v);
      expect(s.has_value());
      expect(*s == R"({"type":"NONE"})") << *s;

      with_none out{};
      expect(not glz::read_json(out, *s));
      expect(out.index() == 2);

      std::string pretty{};
      expect(not glz::write<glz::opts{.prettify = true}>(v, pretty));
      expect(pretty == "{\n   \"type\": \"NONE\"\n}") << pretty;
   };

   "a bare null still reads as the unit alternative"_test = [] {
      // Data written before the unit alternative had a discriminator must keep parsing.
      with_none out{put{1}};
      expect(not glz::read_json(out, "null"));
      expect(out.index() == 2);
   };

   "the object alternatives are unaffected"_test = [] {
      with_none v{del{"abc"}};
      auto s = glz::write_json(v);
      expect(s.has_value());
      expect(*s == R"({"type":"DELETE","id":"abc"})") << *s;
      with_none out{};
      expect(not glz::read_json(out, *s));
      expect(out.index() == 1);
   };
};

suite tag_owning_alternative_tests = [] {
   using namespace tagging_repr;

   "an alternative declaring the discriminator emits it once"_test = [] {
      // `owns_tag` has a member named "kind", which is the variant's tag, so it supplies the
      // discriminator itself. Previously a glaze_object_t alternative also had one merged in,
      // producing {"kind":"owns","kind":"owns","x":1} -- a duplicate key.
      declares_tag v{owns_tag{"owns", 1}};
      auto s = glz::write_json(v);
      expect(s.has_value());
      expect(*s == R"({"kind":"owns","x":1})") << *s;

      declares_tag out{};
      expect(not glz::read_json(out, *s));
      expect(out.index() == 0);
      expect(std::get<0>(out).kind == "owns");
      expect(std::get<0>(out).x == 1);
   };
};

namespace short_ids_oob
{
   struct a_t
   {
      int a{};
   };
   struct b_t
   {
      int b{};
   };
   struct c_t
   {
      int c{};
   };
   using v_t = std::variant<a_t, b_t, c_t>;
}

template <>
struct glz::meta<short_ids_oob::v_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr auto ids = std::array{"a", "b"}; // 2 ids, 3 alternatives
};

suite ids_bounds_tests = [] {
   using namespace short_ids_oob;

   "writing an alternative with no declared id errors instead of reading out of bounds"_test = [] {
      // `ids` shorter than the variant is a supported read-side feature (c_t is the default for an
      // unrecognized id), so it is reachable from a legal glz::meta. Writing c_t has no id to emit;
      // indexing ids_v there read past the end of a static array and emitted whatever followed it as
      // the tag value -- observed as {"t":"supported types: a, b","c":7}, and ~4 GB under ASan.
      std::string s{};
      expect(bool(glz::write_json(v_t{c_t{7}}, s)));

      // The labeled alternatives are unaffected.
      s.clear();
      expect(not glz::write_json(v_t{b_t{3}}, s));
      expect(s == R"({"t":"b","b":3})") << s;

      // Same guard on the array-variant wrapper, which indexes ids_v the same way.
      v_t unlabeled{c_t{7}};
      std::string arr{};
      expect(bool(glz::write_json(glz::array_variant_wrapper<v_t>{unlabeled}, arr)));
   };
};

suite adjacent_schema_tests = [] {
   using namespace tagging_repr;

   "the schema describes the adjacent shape as a discriminated union"_test = [] {
      // The point of the representation is that a consumer can dispatch on the tag, so the schema
      // has to say so: one branch per alternative, each a closed object with a const discriminator
      // and the alternative's own schema nested under the content key.
      //
      // Asserted as fragments rather than as one whole-document match, because the outer title
      // embeds the compiler's spelling of the type name and standard libraries disagree about it.
      const auto schema = glz::write_json_schema<same_shape>();
      expect(schema.has_value());
      expect(schema->find(R"("oneOf")") != std::string::npos) << *schema;
      for (const std::string id : {"vec", "deq"}) {
         // The discriminator is a const, and the alternative's own schema is nested beneath the
         // content key rather than merged alongside it.
         const auto nested = R"("properties":{"type":{"const":")" + id + R"("},"value":{)";
         expect(schema->find(nested) != std::string::npos) << id << " branch: " << *schema;
         // The branch is a closed object requiring exactly the two declared keys.
         const auto closed = R"("additionalProperties":false,"required":["type","value"],"title":")" + id + R"("})";
         expect(schema->find(closed) != std::string::npos) << id << " branch: " << *schema;
      }
   };

   "a unit alternative is described as a discriminator-only object"_test = [] {
      const auto schema = glz::write_json_schema<with_none>();
      expect(schema.has_value());
      // Not `{"type":"null"}`: internal tagging writes the unit alternative as {tag: id}.
      expect(
         schema->find(
            R"({"type":"object","properties":{"type":{"const":"NONE"}},"additionalProperties":false,"required":["type"],"title":"NONE"})") !=
         std::string::npos)
         << *schema;
      expect(schema->find(R"("type":"null")") == std::string::npos) << *schema;
   };
};

int main() { return 0; }
