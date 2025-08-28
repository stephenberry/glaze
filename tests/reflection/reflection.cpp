// Glaze Library
// For the license information refer to glaze.hpp

#include <ut/ut.hpp>

#include "glaze/core/convert_struct.hpp"
#include "glaze/glaze.hpp"
#include <variant>

using namespace ut;

struct test_type
{
   int32_t int1{};
   int64_t int2{};
};

suite reflect_test_type = [] {
   static_assert(glz::reflect<test_type>::size == 2);
   static_assert(glz::reflect<test_type>::keys[0] == "int1");

   "for_each_field"_test = [] {
      test_type var{42, 43};

      glz::for_each_field(var, [](auto& field) { field += 1; });

      expect(var.int1 == 43);
      expect(var.int2 == 44);
   };
};

struct test_type_meta
{
   int32_t int1{};
   int64_t int2{};
};

template <>
struct glz::meta<test_type_meta>
{
   using T = test_type_meta;
   static constexpr auto value = object(&T::int1, &T::int2);
};

suite meta_reflect_test_type = [] {
   static_assert(glz::reflect<test_type_meta>::size == 2);
   static_assert(glz::reflect<test_type_meta>::keys[0] == "int1");

   "for_each_field"_test = [] {
      test_type_meta var{42, 43};

      glz::for_each_field(var, [](auto& field) { field += 1; });

      expect(var.int1 == 43);
      expect(var.int2 == 44);
   };
};

struct a_type
{
   float fluff = 1.1f;
   int goo = 1;
   std::string stub = "a";
};

struct b_type
{
   float fluff = 2.2f;
   int goo = 2;
   std::string stub = "b";
};

struct c_type
{
   std::optional<float> fluff = 3.3f;
   std::optional<int> goo = 3;
   std::optional<std::string> stub = "c";
};

suite convert_tests = [] {
   "convert a to b"_test = [] {
      a_type in{};
      b_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 1.1f);
      expect(out.goo == 1);
      expect(out.stub == "a");
   };

   "convert a to c"_test = [] {
      a_type in{};
      c_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff.value() == 1.1f);
      expect(out.goo.value() == 1);
      expect(out.stub.value() == "a");
   };

   "convert c to a"_test = [] {
      c_type in{};
      a_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 3.3f);
      expect(out.goo == 3);
      expect(out.stub == "c");
   };
};

// Tests for variant tagging with reflectable structs (no explicit meta)
struct Person {
   std::string name;
   int age;
};

struct Animal {
   std::string species;
   float weight;
};

struct Vehicle {
   std::string model;
   int wheels;
};

// Define variant with tag and IDs
using ReflectableVariant = std::variant<Person, Animal, Vehicle>;

template <>
struct glz::meta<ReflectableVariant> {
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"person", "animal", "vehicle"};
};

suite variant_tagging_reflectable = [] {
   "variant tagging with reflectable structs"_test = [] {
      // Test serialization with tagging
      ReflectableVariant variant = Person{"Alice", 30};
      auto json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"type":"person","name":"Alice","age":30})") << json.value();
      
      variant = Animal{"Lion", 190.5f};
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"type":"animal","species":"Lion","weight":190.5})") << json.value();
      
      variant = Vehicle{"Car", 4};
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"type":"vehicle","model":"Car","wheels":4})") << json.value();
   };
   
   "variant parsing with reflectable structs"_test = [] {
      // Test deserialization with tagging
      std::string json = R"({"type":"person","name":"Bob","age":25})";
      ReflectableVariant variant;
      auto ec = glz::read_json(variant, json);
      expect(!ec);
      
      auto* person = std::get_if<Person>(&variant);
      expect(person != nullptr);
      expect(person->name == "Bob");
      expect(person->age == 25);
      
      json = R"({"type":"animal","species":"Tiger","weight":220.5})";
      ec = glz::read_json(variant, json);
      expect(!ec);
      
      auto* animal = std::get_if<Animal>(&variant);
      expect(animal != nullptr);
      expect(animal->species == "Tiger");
      expect(animal->weight == 220.5f);
      
      json = R"({"type":"vehicle","model":"Truck","wheels":6})";
      ec = glz::read_json(variant, json);
      expect(!ec);
      
      auto* vehicle = std::get_if<Vehicle>(&variant);
      expect(vehicle != nullptr);
      expect(vehicle->model == "Truck");
      expect(vehicle->wheels == 6);
   };
};

// Test structs with a field that matches the tag name (shouldn't get double-tagged)
struct CommandA {
   int code;
   std::string data;
};

struct CommandB {
   int code;
   float value;
};

using CommandVariant = std::variant<CommandA, CommandB>;

template <>
struct glz::meta<CommandVariant> {
   static constexpr std::string_view tag = "code";  // Same as struct field name
   static constexpr auto ids = std::array{100, 200};
};

suite variant_no_double_tagging = [] {
   "no double tagging when field matches tag name"_test = [] {
      // Structs with 'code' field should NOT get an additional 'code' tag
      CommandVariant cmd = CommandA{100, "test"};
      auto json = glz::write_json(cmd);
      expect(json.has_value());
      // Should not have duplicate "code" fields
      expect(json.value() == R"({"code":100,"data":"test"})") << json.value();
      
      cmd = CommandB{200, 3.14f};
      json = glz::write_json(cmd);
      expect(json.has_value());
      expect(json.value() == R"({"code":200,"value":3.14})") << json.value();
   };
};

// Test that primitive types in variants still work without object tagging
using PrimitiveVariant = std::variant<int, std::string, double>;

template <>
struct glz::meta<PrimitiveVariant> {
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"integer", "string", "double"};
};

suite variant_primitive_types = [] {
   "variant with primitive types (no object tagging)"_test = [] {
      PrimitiveVariant variant = 42;
      auto json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == "42") << json.value();
      
      variant = std::string("hello");
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"("hello")") << json.value();
      
      variant = 3.14;
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == "3.14") << json.value();
   };
};

int main() { return 0; }
