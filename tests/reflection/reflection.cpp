// Glaze Library
// For the license information refer to glaze.hpp

#include <ut/ut.hpp>
#include <variant>

#include "glaze/core/convert_struct.hpp"
#include "glaze/glaze.hpp"

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
struct Person
{
   std::string name;
   int age;
};

struct Animal
{
   std::string species;
   float weight;
};

struct Vehicle
{
   std::string model;
   int wheels;
};

// Define variant with tag and IDs
using ReflectableVariant = std::variant<Person, Animal, Vehicle>;

template <>
struct glz::meta<ReflectableVariant>
{
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
struct CommandA
{
   int code;
   std::string data;
};

struct CommandB
{
   int code;
   float value;
};

using CommandVariant = std::variant<CommandA, CommandB>;

template <>
struct glz::meta<CommandVariant>
{
   static constexpr std::string_view tag = "code"; // Same as struct field name
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

   "reading when field matches tag name"_test = [] {
      CommandVariant cmd;

      // Test reading CommandA - the 'code' field serves as both data and discriminator
      std::string json = R"({"code":100,"data":"test"})";
      auto ec = glz::read_json(cmd, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<CommandA>(cmd));
      auto& cmdA = std::get<CommandA>(cmd);
      expect(cmdA.code == 100);
      expect(cmdA.data == "test");

      // Test reading CommandB
      json = R"({"code":200,"value":3.14})";
      ec = glz::read_json(cmd, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<CommandB>(cmd));
      auto& cmdB = std::get<CommandB>(cmd);
      expect(cmdB.code == 200);
      expect(cmdB.value == 3.14f);

      // Test with different order of fields
      json = R"({"data":"hello","code":100})";
      ec = glz::read_json(cmd, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<CommandA>(cmd));
      auto& cmdA2 = std::get<CommandA>(cmd);
      expect(cmdA2.code == 100);
      expect(cmdA2.data == "hello");

      // Test invalid code value (should fail)
      json = R"({"code":999,"data":"invalid"})";
      ec = glz::read_json(cmd, json);
      expect(ec) << "Should fail with invalid discriminator value";
   };
};

// Test that primitive types in variants still work without object tagging
using PrimitiveVariant = std::variant<bool, std::string, double>;

template <>
struct glz::meta<PrimitiveVariant>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"boolean", "string", "double"};
};

suite variant_primitive_types = [] {
   "variant with primitive types (no object tagging)"_test = [] {
      PrimitiveVariant variant = true;
      auto json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == "true") << json.value();

      variant = std::string("hello");
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"("hello")") << json.value();

      variant = 3.14;
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == "3.14") << json.value();
   };

   "variant with primitive types reading"_test = [] {
      PrimitiveVariant variant;

      // Even with tag defined, primitive types should read directly without object wrapping

      // Test reading boolean directly
      std::string json = "true";
      auto ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<bool>(variant));
      expect(std::get<bool>(variant) == true);

      // Test reading string directly
      json = R"("hello world")";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<std::string>(variant));
      expect(std::get<std::string>(variant) == "hello world");

      // Test reading double directly
      json = "3.14159";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<double>(variant));
      expect(std::get<double>(variant) == 3.14159);
   };
};

// Test auto-deduced variants with reflectable structs (no tags/ids needed)
struct Book
{
   std::string title;
   std::string author;
   int pages;
};

struct Movie
{
   std::string director;
   int duration;
   float rating;
};

struct Song
{
   std::string artist;
   std::string album;
   int year;
};

// Variant WITHOUT meta specialization - relies on auto-deduction
using AutoDeducedVariant = std::variant<Book, Movie, Song>;

suite variant_auto_deduction = [] {
   "variant auto-deduction writing"_test = [] {
      // Test that structs are written without type tags
      AutoDeducedVariant variant = Book{"1984", "George Orwell", 328};
      auto json = glz::write_json(variant);
      expect(json.has_value());
      // No type tag should be present
      expect(json.value() == R"({"title":"1984","author":"George Orwell","pages":328})") << json.value();

      variant = Movie{"Christopher Nolan", 148, 8.8f};
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"director":"Christopher Nolan","duration":148,"rating":8.8})") << json.value();

      variant = Song{"The Beatles", "Abbey Road", 1969};
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"artist":"The Beatles","album":"Abbey Road","year":1969})") << json.value();
   };

   "variant auto-deduction reading"_test = [] {
      AutoDeducedVariant variant;

      // Test reading Book - should deduce from field names
      std::string json = R"({"title":"The Hobbit","author":"J.R.R. Tolkien","pages":310})";
      auto ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Book>(variant));
      auto& book = std::get<Book>(variant);
      expect(book.title == "The Hobbit");
      expect(book.author == "J.R.R. Tolkien");
      expect(book.pages == 310);

      // Test reading Movie - should deduce from field names
      json = R"({"director":"Steven Spielberg","duration":127,"rating":9.0})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Movie>(variant));
      auto& movie = std::get<Movie>(variant);
      expect(movie.director == "Steven Spielberg");
      expect(movie.duration == 127);
      expect(movie.rating == 9.0f);

      // Test reading Song - should deduce from field names
      json = R"({"artist":"Queen","album":"A Night at the Opera","year":1975})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Song>(variant));
      auto& song = std::get<Song>(variant);
      expect(song.artist == "Queen");
      expect(song.album == "A Night at the Opera");
      expect(song.year == 1975);
   };

   "variant auto-deduction with partial fields"_test = [] {
      AutoDeducedVariant variant;

      // Test with only unique fields - should still deduce correctly
      // Book has unique "title" field
      std::string json = R"({"title":"Partial Book"})";
      auto ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Book>(variant));
      expect(std::get<Book>(variant).title == "Partial Book");

      // Movie has unique "director" field
      json = R"({"director":"Unknown Director"})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Movie>(variant));
      expect(std::get<Movie>(variant).director == "Unknown Director");

      // Song has unique "artist" field
      json = R"({"artist":"Unknown Artist"})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Song>(variant));
      expect(std::get<Song>(variant).artist == "Unknown Artist");
   };

   "variant auto-deduction field order independence"_test = [] {
      AutoDeducedVariant variant;

      // Test that field order doesn't matter for deduction
      std::string json = R"({"pages":500,"author":"Test Author","title":"Test Book"})";
      auto ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Book>(variant));
      auto& book = std::get<Book>(variant);
      expect(book.title == "Test Book");
      expect(book.author == "Test Author");
      expect(book.pages == 500);

      // Different order for Movie
      json = R"({"rating":7.5,"director":"Test Director","duration":120})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<Movie>(variant));
      auto& movie = std::get<Movie>(variant);
      expect(movie.director == "Test Director");
      expect(movie.duration == 120);
      expect(movie.rating == 7.5f);
   };
};

// Test embedded tags in variant structs
// String-based embedded tags
struct PutActionStr
{
   std::string action{"PUT"}; // Embedded string tag
   std::string data;
};

struct DeleteActionStr
{
   std::string action{"DELETE"}; // Embedded string tag
   std::string target;
};

using EmbeddedStringTagVariant = std::variant<PutActionStr, DeleteActionStr>;

template <>
struct glz::meta<EmbeddedStringTagVariant>
{
   static constexpr std::string_view tag = "action";
   static constexpr auto ids = std::array{"PUT", "DELETE"};
};

// Enum-based embedded tags
enum class ActionType { PUT, DELETE };

template <>
struct glz::meta<ActionType>
{
   static constexpr auto value = enumerate(ActionType::PUT, ActionType::DELETE);
};

struct PutActionEnum
{
   ActionType action{ActionType::PUT}; // Embedded enum tag
   std::string data;
};

struct DeleteActionEnum
{
   ActionType action{ActionType::DELETE}; // Embedded enum tag
   std::string target;
};

using EmbeddedEnumTagVariant = std::variant<PutActionEnum, DeleteActionEnum>;

suite embedded_tag_variants = [] {
   "embedded string tag writing"_test = [] {
      // Test that structs with embedded tags don't get double-tagged
      EmbeddedStringTagVariant variant = PutActionStr{"PUT", "test_data"};
      auto json = glz::write_json(variant);
      expect(json.has_value());
      // Should have single "action" field, not duplicated
      expect(json.value() == R"({"action":"PUT","data":"test_data"})") << json.value();

      variant = DeleteActionStr{"DELETE", "test_target"};
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"action":"DELETE","target":"test_target"})") << json.value();
   };

   "embedded string tag reading"_test = [] {
      EmbeddedStringTagVariant variant;

      // Test reading PutActionStr
      std::string json = R"({"action":"PUT","data":"restored_data"})";
      auto ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<PutActionStr>(variant));
      auto& put = std::get<PutActionStr>(variant);
      expect(put.action == "PUT"); // Verify the embedded tag is populated
      expect(put.data == "restored_data");

      // Test reading DeleteActionStr
      json = R"({"action":"DELETE","target":"removed_item"})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<DeleteActionStr>(variant));
      auto& del = std::get<DeleteActionStr>(variant);
      expect(del.action == "DELETE"); // Verify the embedded tag is populated
      expect(del.target == "removed_item");

      // Test with fields in different order
      json = R"({"data":"more_data","action":"PUT"})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<PutActionStr>(variant));
      expect(std::get<PutActionStr>(variant).action == "PUT");
      expect(std::get<PutActionStr>(variant).data == "more_data");
   };

   "embedded enum tag writing"_test = [] {
      // Test that enums are properly serialized as strings
      EmbeddedEnumTagVariant variant = PutActionEnum{ActionType::PUT, "enum_data"};
      auto json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"action":"PUT","data":"enum_data"})") << json.value();

      variant = DeleteActionEnum{ActionType::DELETE, "enum_target"};
      json = glz::write_json(variant);
      expect(json.has_value());
      expect(json.value() == R"({"action":"DELETE","target":"enum_target"})") << json.value();
   };

   "embedded enum tag reading"_test = [] {
      EmbeddedEnumTagVariant variant;

      // Test reading PutActionEnum
      std::string json = R"({"action":"PUT","data":"enum_restored"})";
      auto ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<PutActionEnum>(variant));
      auto& put = std::get<PutActionEnum>(variant);
      expect(put.action == ActionType::PUT); // Verify the embedded enum tag is populated
      expect(put.data == "enum_restored");

      // Test reading DeleteActionEnum
      json = R"({"action":"DELETE","target":"enum_removed"})";
      ec = glz::read_json(variant, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<DeleteActionEnum>(variant));
      auto& del = std::get<DeleteActionEnum>(variant);
      expect(del.action == ActionType::DELETE); // Verify the embedded enum tag is populated
      expect(del.target == "enum_removed");
   };

   "embedded tag round-trip"_test = [] {
      // Test complete round-trip serialization

      // String-based
      {
         EmbeddedStringTagVariant original = PutActionStr{"PUT", "round_trip_data"};
         auto json = glz::write_json(original);
         expect(json.has_value());

         EmbeddedStringTagVariant restored;
         auto ec = glz::read_json(restored, json.value());
         expect(!ec);
         expect(std::holds_alternative<PutActionStr>(restored));
         auto& put = std::get<PutActionStr>(restored);
         expect(put.action == "PUT");
         expect(put.data == "round_trip_data");
      }

      // Enum-based
      {
         EmbeddedEnumTagVariant original = DeleteActionEnum{ActionType::DELETE, "round_trip_target"};
         auto json = glz::write_json(original);
         expect(json.has_value());

         EmbeddedEnumTagVariant restored;
         auto ec = glz::read_json(restored, json.value());
         expect(!ec);
         expect(std::holds_alternative<DeleteActionEnum>(restored));
         auto& del = std::get<DeleteActionEnum>(restored);
         expect(del.action == ActionType::DELETE);
         expect(del.target == "round_trip_target");
      }
   };

   "embedded tag runtime access"_test = [] {
      // Demonstrate that embedded tags are accessible at runtime without std::visit
      EmbeddedStringTagVariant str_variant = PutActionStr{"PUT", "test"};

      // Direct access to the action field after deserialization
      std::string json = R"({"action":"DELETE","target":"xyz"})";
      auto ec = glz::read_json(str_variant, json);
      expect(!ec);

      // Can check the type directly via the embedded field
      if (std::holds_alternative<DeleteActionStr>(str_variant)) {
         auto& del = std::get<DeleteActionStr>(str_variant);
         expect(del.action == "DELETE"); // Direct runtime access to discriminator
      }

      // Same for enum variant
      EmbeddedEnumTagVariant enum_variant;
      json = R"({"action":"PUT","data":"abc"})";
      ec = glz::read_json(enum_variant, json);
      expect(!ec);

      if (std::holds_alternative<PutActionEnum>(enum_variant)) {
         auto& put = std::get<PutActionEnum>(enum_variant);
         expect(put.action == ActionType::PUT); // Direct runtime access to discriminator
      }
   };
};

// Tests for nested array variants parsing
suite nested_array_variant_tests = [] {
   "nested array variant - vector<double>"_test = [] {
      using NestedArrayVariant = std::variant<std::vector<double>, std::vector<std::vector<double>>>;
      NestedArrayVariant var;
      std::string json = "[1.0, 2.0, 3.0]";
      auto ec = glz::read_json(var, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<std::vector<double>>(var));
      auto& vec = std::get<std::vector<double>>(var);
      expect(vec.size() == 3);
      expect(vec[0] == 1.0);
      expect(vec[1] == 2.0);
      expect(vec[2] == 3.0);
   };

   "nested array variant - vector<vector<double>>"_test = [] {
      using NestedArrayVariant = std::variant<std::vector<double>, std::vector<std::vector<double>>>;
      NestedArrayVariant var;
      std::string json = "[[1.0, 1.0], [2.0, 2.0]]";
      auto ec = glz::read_json(var, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<std::vector<std::vector<double>>>(var));
      auto& vec = std::get<std::vector<std::vector<double>>>(var);
      expect(vec.size() == 2);
      expect(vec[0].size() == 2);
      expect(vec[0][0] == 1.0);
      expect(vec[0][1] == 1.0);
      expect(vec[1][0] == 2.0);
      expect(vec[1][1] == 2.0);
   };

   "nested array variant - integer vectors"_test = [] {
      // Test with integers that should work as doubles too
      using NestedArrayVariant = std::variant<std::vector<double>, std::vector<std::vector<double>>>;
      NestedArrayVariant var;
      std::string json = "[[1, 1], [2, 2]]";
      auto ec = glz::read_json(var, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<std::vector<std::vector<double>>>(var));
      auto& vec = std::get<std::vector<std::vector<double>>>(var);
      expect(vec.size() == 2);
      expect(vec[0][0] == 1.0);
      expect(vec[1][1] == 2.0);
   };

   "nested array variant - round trip"_test = [] {
      using NestedArrayVariant = std::variant<std::vector<double>, std::vector<std::vector<double>>>;

      // Test vector<double> round trip
      {
         NestedArrayVariant original = std::vector<double>{1.5, 2.5, 3.5};
         auto json = glz::write_json(original);
         expect(json.has_value());

         NestedArrayVariant restored;
         auto ec = glz::read_json(restored, json.value());
         expect(!ec);
         expect(std::holds_alternative<std::vector<double>>(restored));
         auto& vec = std::get<std::vector<double>>(restored);
         expect(vec.size() == 3);
         expect(vec[0] == 1.5);
      }

      // Test vector<vector<double>> round trip
      {
         NestedArrayVariant original = std::vector<std::vector<double>>{{1.5, 2.5}, {3.5, 4.5}};
         auto json = glz::write_json(original);
         expect(json.has_value());

         NestedArrayVariant restored;
         auto ec = glz::read_json(restored, json.value());
         expect(!ec);
         expect(std::holds_alternative<std::vector<std::vector<double>>>(restored));
         auto& vec = std::get<std::vector<std::vector<double>>>(restored);
         expect(vec.size() == 2);
         expect(vec[0][0] == 1.5);
         expect(vec[1][1] == 4.5);
      }
   };

   "nested array variant - empty arrays"_test = [] {
      using NestedArrayVariant = std::variant<std::vector<double>, std::vector<std::vector<double>>>;

      // Empty outer array should parse as vector<double>
      NestedArrayVariant var;
      std::string json = "[]";
      auto ec = glz::read_json(var, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<std::vector<double>>(var));
      expect(std::get<std::vector<double>>(var).empty());

      // Array with empty inner arrays should parse as vector<vector<double>>
      json = "[[], []]";
      ec = glz::read_json(var, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(std::holds_alternative<std::vector<std::vector<double>>>(var));
      auto& vec = std::get<std::vector<std::vector<double>>>(var);
      expect(vec.size() == 2);
      expect(vec[0].empty());
      expect(vec[1].empty());
   };
};

// Define structs and variant for tag validation tests
namespace tag_validation
{
   struct Person
   {
      std::string name;
      int age;
   };

   struct Animal
   {
      std::string species;
      float weight;
   };

   struct Vehicle
   {
      std::string model;
      int wheels;
   };

   using EntityVariant = std::variant<Person, Animal, Vehicle>;
}

template <>
struct glz::meta<tag_validation::EntityVariant>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"person", "animal", "vehicle"};
};

namespace tag_validation2
{
   struct Person
   {
      std::string name;
      int age;
   };

   struct Animal
   {
      std::string species;
      float weight;
   };

   using TaggedVariant = std::variant<Person, Animal>;
}

template <>
struct glz::meta<tag_validation2::TaggedVariant>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"person", "animal"};
};

namespace edge_case_tests
{
   struct Empty
   {};
   struct SingleField
   {
      int value;
   };
   struct TwoFields
   {
      int a;
      int b;
   };

   struct Car
   {
      std::string brand;
      int year;
   };

   struct Bike
   {
      std::string model;
      int gears;
   };
}

namespace perf_test
{
   struct SimpleA
   {
      int unique_a_field;
   };

   struct SimpleB
   {
      int unique_b_field;
   };
}

suite variant_tag_validation = [] {
   "tagged variant - tag/field mismatch detection"_test = [] {
      using namespace tag_validation;

      // Verify tag metadata is defined
      static_assert(glz::meta<EntityVariant>::tag == "type");

      // Test 1: Tag at beginning with correct fields (should work)
      {
         EntityVariant e;
         std::string json = R"({"type":"animal","species":"Lion","weight":190.5})";
         auto ec = glz::read_json(e, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(e.index() == 1);
         auto& animal = std::get<tag_validation::Animal>(e);
         expect(animal.species == "Lion");
         expect(animal.weight == 190.5f);
      }

      // Test 2: Tag in middle says person but fields are for animal (should error)
      {
         EntityVariant e;
         std::string json = R"({"species":"Lion","type":"person","weight":190.5})";
         auto ec = glz::read_json(e, json);
         expect(ec == glz::error_code::no_matching_variant_type);
      }

      // Test 3: Tag at end says vehicle but fields are for animal (should error)
      {
         EntityVariant e;
         std::string json = R"({"species":"Tiger","weight":220.0,"type":"vehicle"})";
         auto ec = glz::read_json(e, json);
         expect(ec == glz::error_code::no_matching_variant_type);
      }

      // Test 4: Person fields but tag says animal (should error)
      {
         EntityVariant e;
         std::string json = R"({"name":"John","age":30,"type":"animal"})";
         auto ec = glz::read_json(e, json);
         expect(ec == glz::error_code::no_matching_variant_type);
      }

      // Test 5: No tag present, rely on field deduction (should work)
      {
         EntityVariant e;
         std::string json = R"({"species":"Elephant","weight":5000.0})";
         auto ec = glz::read_json(e, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(e.index() == 1);
         auto& animal = std::get<tag_validation::Animal>(e);
         expect(animal.species == "Elephant");
         expect(animal.weight == 5000.0f);
      }

      // Test 6: Tag matches fields (should work)
      {
         EntityVariant e;
         std::string json = R"({"species":"Cat","type":"animal","weight":4.5})";
         auto ec = glz::read_json(e, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(e.index() == 1);
         auto& animal = std::get<tag_validation::Animal>(e);
         expect(animal.species == "Cat");
         expect(animal.weight == 4.5f);
      }

      // Test 7: Invalid tag value (should error)
      {
         EntityVariant e;
         std::string json = R"({"type":"invalid","species":"Dog","weight":25.0})";
         auto ec = glz::read_json(e, json);
         expect(ec == glz::error_code::no_matching_variant_type);
      }

      // Test 8: Tag first with mismatched fields (should error due to unknown keys)
      {
         EntityVariant e;
         std::string json = R"({"type":"person","species":"Lion","weight":190.5})";
         auto ec = glz::read<glz::opts{.error_on_unknown_keys = true}>(e, json);
         expect(ec == glz::error_code::unknown_key);
      }
   };

   "tagged variant - edge cases"_test = [] {
      using namespace edge_case_tests;
      using TestVariant = std::variant<Empty, SingleField, TwoFields>;

      // Test with empty object
      {
         TestVariant v;
         std::string json = R"({})";
         auto ec = glz::read_json(v, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(v.index() == 0); // Should select Empty
      }

      // Test with partial field match
      {
         TestVariant v;
         std::string json = R"({"value":42})";
         auto ec = glz::read_json(v, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(v.index() == 1); // Should select SingleField
         expect(std::get<SingleField>(v).value == 42);
      }

      // Test with ambiguous fields that match multiple types
      {
         TestVariant v;
         std::string json = R"({"a":1})";
         auto ec = glz::read_json(v, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(v.index() == 2); // Should select TwoFields (has field 'a')
      }
   };

   "tagged variant - minified option"_test = [] {
      using namespace tag_validation2;

      // Test with minified option and tag mismatch
      {
         TaggedVariant v;
         auto ec = glz::read<glz::opts{.minified = true}>(v, R"({"species":"Lion","type":"person","weight":190.5})");
         expect(ec == glz::error_code::no_matching_variant_type);
      }

      // Test with minified option and matching tag
      {
         TaggedVariant v;
         auto ec = glz::read<glz::opts{.minified = true}>(v, R"({"species":"Lion","type":"animal","weight":190.5})");
         expect(!ec);
         expect(v.index() == 1);
      }
   };

   "untagged variant - field deduction only"_test = [] {
      using namespace edge_case_tests;
      // Variant without tag metadata
      using UntaggedVariant = std::variant<Car, Bike>;

      // Should use field deduction
      {
         UntaggedVariant v;
         std::string json = R"({"brand":"Toyota","year":2022})";
         auto ec = glz::read_json(v, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(v.index() == 0);
         auto& car = std::get<Car>(v);
         expect(car.brand == "Toyota");
         expect(car.year == 2022);
      }

      {
         UntaggedVariant v;
         std::string json = R"({"model":"Mountain","gears":21})";
         auto ec = glz::read_json(v, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(v.index() == 1);
         auto& bike = std::get<Bike>(v);
         expect(bike.model == "Mountain");
         expect(bike.gears == 21);
      }
   };

   // Test that untagged variants still short-circuit for performance
   "performance_short_circuit"_test = [] {
      using PerfVariant = std::variant<perf_test::SimpleA, perf_test::SimpleB>;

      // Test immediate selection when field narrows to single type
      {
         PerfVariant v;
         // Only unique_a_field matches SimpleA, should select immediately
         std::string json = R"({"unique_a_field":42})";
         auto ec = glz::read_json(v, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(v.index() == 0);
         expect(std::get<perf_test::SimpleA>(v).unique_a_field == 42);
      }

      {
         PerfVariant v;
         // Only unique_b_field matches SimpleB, should select immediately
         std::string json = R"({"unique_b_field":99})";
         auto ec = glz::read_json(v, json);
         expect(!ec) << glz::format_error(ec, json);
         expect(v.index() == 1);
         expect(std::get<perf_test::SimpleB>(v).unique_b_field == 99);
      }
   };
};

// Types for has_reflect concept testing (defined outside suite for template specializations)
namespace has_reflect_test
{
   struct SimpleAggregate
   {
      int x;
      double y;
   };

   struct WithObjectMeta
   {
      int a;
      double b;
   };

   struct WithArrayMeta
   {
      int x;
      int y;
      int z;
   };

   enum class TestEnumMeta { A, B, C };

   struct NonAggregate
   {
      NonAggregate() {} // Has constructor, so not aggregate
      int x;
   };

   struct PrivateMember
   {
     private:
      [[maybe_unused]] int x;

     public:
      int y;
   };

   struct EmptyStruct
   {};
}

// Add meta specializations for testing
template <>
struct glz::meta<has_reflect_test::WithObjectMeta>
{
   using T = has_reflect_test::WithObjectMeta;
   static constexpr auto value = object(&T::a, &T::b);
};

template <>
struct glz::meta<has_reflect_test::WithArrayMeta>
{
   using T = has_reflect_test::WithArrayMeta;
   static constexpr auto value = array(&T::x, &T::y, &T::z);
};

template <>
struct glz::meta<has_reflect_test::TestEnumMeta>
{
   using enum has_reflect_test::TestEnumMeta;
   static constexpr auto value = enumerate(A, B, C);
};

// Test suite for has_reflect concept
suite has_reflect_concept_tests = [] {
   using namespace has_reflect_test;

   // Test aggregate types (should satisfy both reflectable and has_reflect)
   static_assert(glz::reflectable<SimpleAggregate>);
   static_assert(glz::has_reflect<SimpleAggregate>);

   // Test types with glz::meta (should satisfy has_reflect but not reflectable)
   static_assert(!glz::reflectable<WithObjectMeta>);
   static_assert(glz::has_reflect<WithObjectMeta>);
   static_assert(!glz::reflectable<WithArrayMeta>);
   static_assert(glz::has_reflect<WithArrayMeta>);
   static_assert(!glz::reflectable<TestEnumMeta>);
   static_assert(glz::has_reflect<TestEnumMeta>);

   // Test non-reflectable types
   static_assert(!glz::reflectable<NonAggregate>);
   static_assert(!glz::has_reflect<NonAggregate>);
   static_assert(!glz::reflectable<PrivateMember>);
   static_assert(!glz::has_reflect<PrivateMember>);

   // Test map types (have reflect specialization with size = 0)
   using TestMap = std::map<std::string, int>;
   using TestUnorderedMap = std::unordered_map<std::string, double>;
   static_assert(!glz::reflectable<TestMap>);
   static_assert(glz::has_reflect<TestMap>);
   static_assert(!glz::reflectable<TestUnorderedMap>);
   static_assert(glz::has_reflect<TestUnorderedMap>);

   // Test primitive and standard types (no reflect)
   static_assert(!glz::reflectable<int>);
   static_assert(!glz::has_reflect<int>);
   static_assert(!glz::reflectable<double>);
   static_assert(!glz::has_reflect<double>);
   static_assert(!glz::reflectable<std::string>);
   static_assert(!glz::has_reflect<std::string>);
   static_assert(!glz::reflectable<std::vector<int>>);
   static_assert(!glz::has_reflect<std::vector<int>>);

   // Test empty struct
   static_assert(glz::reflectable<EmptyStruct>);
   static_assert(glz::has_reflect<EmptyStruct>);

   "has_reflect with aggregate types"_test = [] {
      // Test that we can use reflect<T>::size for aggregate types
      constexpr auto aggregate_size = glz::reflect<SimpleAggregate>::size;
      expect(aggregate_size == 2);

      constexpr auto empty_size = glz::reflect<EmptyStruct>::size;
      expect(empty_size == 0);
   };

   "has_reflect with map types"_test = [] {
      // Maps have reflect specialization with size = 0
      constexpr auto map_size = glz::reflect<TestMap>::size;
      expect(map_size == 0);

      constexpr auto umap_size = glz::reflect<TestUnorderedMap>::size;
      expect(umap_size == 0);
   };

   "has_reflect with existing test types"_test = [] {
      // Test with types already defined in this file
      static_assert(glz::has_reflect<test_type>);
      static_assert(glz::has_reflect<test_type_meta>);
      static_assert(glz::has_reflect<a_type>);
      static_assert(glz::has_reflect<b_type>);
      static_assert(glz::has_reflect<c_type>);

      constexpr auto test_type_size = glz::reflect<test_type>::size;
      expect(test_type_size == 2);

      constexpr auto a_type_size = glz::reflect<a_type>::size;
      expect(a_type_size == 3);
   };
};

suite has_reflect_meta_types_tests = [] {
   using namespace has_reflect_test;

   "has_reflect with glaze_object_t"_test = [] {
      constexpr auto meta_size = glz::reflect<WithObjectMeta>::size;
      expect(meta_size == 2);

      // Verify keys are properly set
      constexpr auto keys = glz::reflect<WithObjectMeta>::keys;
      expect(keys[0] == "a");
      expect(keys[1] == "b");
   };

   "has_reflect with glaze_array_t"_test = [] {
      constexpr auto array_size = glz::reflect<WithArrayMeta>::size;
      expect(array_size == 3);
   };

   "has_reflect with glaze_enum_t"_test = [] {
      constexpr auto enum_size = glz::reflect<TestEnumMeta>::size;
      expect(enum_size == 3);

      // Verify enum keys
      constexpr auto keys = glz::reflect<TestEnumMeta>::keys;
      expect(keys[0] == "A");
      expect(keys[1] == "B");
      expect(keys[2] == "C");
   };
};

int main() { return 0; }
