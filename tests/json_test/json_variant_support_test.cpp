
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

   "tagged_variant_schema_tests"_test = [] {
      auto s = glz::write_json_schema<tagged_variant>().value_or("error");
      expect(
         s ==
         R"({"type":["object"],"$defs":{"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::string":{"type":["string"]}},"oneOf":[{"type":["object"],"properties":{"action":{"const":"PUT"},"data":{"$ref":"#/$defs/std::map<std::string,int32_t>"}},"additionalProperties":false,"required":["action"],"title":"PUT"},{"type":["object"],"properties":{"action":{"const":"DELETE"},"data":{"$ref":"#/$defs/std::string"}},"additionalProperties":false,"required":["action"],"title":"DELETE"}],"title":"std::variant<put_action, delete_action>"})")
         << s;
   };

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

   "shared_ptr variant schema"_test = [] {
      const auto schema = glz::write_json_schema<std::shared_ptr<tagged_variant2>>().value_or("error");
      expect(
         schema ==
         R"({"type":["object","null"],"$defs":{"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::string":{"type":["string"]}},"oneOf":[{"type":["object"],"properties":{"data":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"type":{"const":"put_action"}},"additionalProperties":false,"required":["type"],"title":"put_action"},{"type":["object"],"properties":{"data":{"$ref":"#/$defs/std::string"},"type":{"const":"delete_action"}},"additionalProperties":false,"required":["type"],"title":"delete_action"},{"type":["null"],"title":"std::monostate","const":null}],"title":"std::shared_ptr<std::variant<put_action, delete_action, std::monostate>>"})")
         << schema;
   };
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
         R"({"type":["object"],"properties":{"$schema":{"$ref":"#/$defs/std::string"},"variant":{"$ref":"#/$defs/vari"}},"additionalProperties":false,"$defs":{"std::string":{"type":["string"]},"vari":{"type":["object"],"oneOf":[{"type":["object"],"properties":{"type":{"const":"varx"}},"additionalProperties":false,"required":["type"],"title":"varx"},{"type":["object"],"properties":{"type":{"const":"vary"}},"additionalProperties":false,"required":["type"],"title":"vary"}]}},"title":"var_schema"})")
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

int main() { return 0; }
