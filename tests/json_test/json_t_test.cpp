// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite generic_json_tests = [] {
   "generic_json_write"_test = [] {
      glz::generic json = {{"pi", 3.141},
                           {"happy", true},
                           {"name", "Niels"},
                           {"nothing", nullptr},
                           {"answer", {{"everything", 42.0}}},
                           {"list", {1.0, 0.0, 2.0}},
                           {"object", {{"currency", "USD"}, {"value", 42.99}}}};
      std::string buffer{};
      expect(not glz::write_json(json, buffer));
      expect(
         buffer ==
         R"({"answer":{"everything":42},"happy":true,"list":[1,0,2],"name":"Niels","nothing":null,"object":{"currency":"USD","value":42.99},"pi":3.141})")
         << buffer;
   };

   "generic_json_read"_test = [] {
      glz::generic json{};
      std::string buffer = R"([5,"Hello World",{"pi":3.14},null])";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json[0].get<double>() == 5.0);
      expect(json[1].get<std::string>() == "Hello World");
      expect(json[2]["pi"].get<double>() == 3.14);
      expect(json[3].holds<glz::generic::null_t>());
   };

   "generic_json_roundtrip"_test = [] {
      glz::generic json{};
      std::string buffer = R"([5,"Hello World",{"pi":3.14},null])";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(glz::write_json(json) == buffer);
      expect(json.dump().value() == buffer);
   };

   "generic_json_const"_test = [] {
      auto foo = [](const glz::generic& json) { return json["s"].get<std::string>(); };
      glz::generic json = {{"s", "hello world"}};
      expect(foo(json) == "hello world");
      expect(json.dump().value() == R"({"s":"hello world"})");

      std::map<std::string, std::string> obj{};
      expect(not glz::read_json(obj, json));
      expect(obj.contains("s"));
      expect(obj.at("s") == "hello world");
   };

   "generic_json_int"_test = [] {
      glz::generic json = {{"i", 1}};
      expect(json["i"].get<double>() == 1);
      expect(json.dump().value() == R"({"i":1})");
   };

   "generic_json_as"_test = [] {
      glz::generic json = {{"pi", 3.141},
                           {"happy", true},
                           {"name", "Niels"},
                           {"nothing", nullptr},
                           {"answer", {{"everything", 42.0}}},
                           {"list", {1.0, 0.0, 2.0}},
                           {"object", {{"currency", "USD"}, {"value", 42.99}}}};
      expect(json["list"][2].as<int>() == 2);
      expect(json["pi"].as<double>() == 3.141);
      expect(json["name"].as<std::string_view>() == "Niels");
      expect(
         json.dump().value() ==
         R"({"answer":{"everything":42},"happy":true,"list":[1,0,2],"name":"Niels","nothing":null,"object":{"currency":"USD","value":42.99},"pi":3.141})")
         << json.dump().value();
   };

   "generic_json_nested_initialization"_test = [] {
      static const glz::generic messageSchema = {{"type", "struct"},
                                                 {"fields",
                                                  {
                                                     {{"field", "branch"}, {"type", "string"}},
                                                  }}};
      std::string buffer{};
      expect(not glz::write_json(messageSchema, buffer));
      expect(buffer == R"({"fields":[{"field":"branch","type":"string"}],"type":"struct"})") << buffer;
   };

   "generic_contains"_test = [] {
      auto json = glz::read_json<glz::generic>(R"({"foo":"bar"})");
      expect(bool(json));
      expect(!json->contains("id"));
      expect(json->contains("foo"));
      auto obj = glz::read_json<std::map<std::string, std::string>>(json.value());
      expect(bool(obj));
      expect(obj->contains("foo"));
      expect(obj->at("foo") == "bar");
   };

   "buffer underrun"_test = [] {
      std::string buffer{"000000000000000000000"};
      glz::generic json{};
      expect(glz::read_json(json, buffer) == glz::error_code::parse_number_failure);
   };

   "generic copy construction"_test = [] {
      std::string s{};
      expect(not glz::write_json(glz::generic(*(glz::read_json<glz::generic>("{}"))), s));
      expect(s == "{}");
   };

   "generic is_object"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, "{}"));
      expect(json.is_object());
      expect(glz::is_object(json));
      expect(json.empty());
      expect(json.size() == 0);
      expect(json.get_object().size() == 0);
   };

   "generic is_object"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, R"({"age":"22","name":"Noah"})"));
      expect(json.is_object());
      expect(glz::is_object(json));
      expect(not json.empty());
      expect(json.size() == 2);
      expect(json.get_object().size() == 2);
   };

   "generic is_array"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, "[]"));
      expect(json.is_array());
      expect(glz::is_array(json));
      expect(json.empty());
      expect(json.size() == 0);
      expect(json.get_array().size() == 0);
   };

   "generic is_array"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, "[1,2,3]"));
      expect(json.is_array());
      expect(glz::is_array(json));
      expect(not json.empty());
      expect(json.size() == 3);
      expect(json.get_array().size() == 3);
      std::array<int, 3> v{};
      expect(not glz::read<glz::opts{}>(v, json));
      expect(v == std::array{1, 2, 3});
   };

   "generic is_string"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, R"("")"));
      expect(json.is_string());
      expect(glz::is_string(json));
      expect(json.empty());
      expect(json.size() == 0);
      expect(json.get_string() == "");
   };

   "generic is_string"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, R"("Beautiful beginning")"));
      expect(json.is_string());
      expect(glz::is_string(json));
      expect(not json.empty());
      expect(json.size() == 19);
      expect(json.get_string() == "Beautiful beginning");
      std::string v{};
      expect(not glz::read<glz::opts{}>(v, json));
      expect(v == "Beautiful beginning");
   };

   "generic is_number"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, "3.882e2"));
      expect(json.is_number());
      expect(glz::is_number(json));
      expect(not json.empty());
      expect(json.size() == 0);
      expect(json.get_number() == 3.882e2);
      double v{};
      expect(not glz::read<glz::opts{}>(v, json));
      expect(v == 3.882e2);
   };

   "generic is_boolean"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, "true"));
      expect(json.is_boolean());
      expect(glz::is_boolean(json));
      expect(not json.empty());
      expect(json.size() == 0);
      expect(json.get_boolean());
      bool v{};
      expect(not glz::read<glz::opts{}>(v, json));
      expect(v);
   };

   "generic is_null"_test = [] {
      glz::generic json{};
      expect(not glz::read_json(json, "null"));
      expect(json.is_null());
      expect(glz::is_null(json));
      expect(json.empty());
      expect(json.size() == 0);
   };

   "generic garbage input"_test = [] {
      glz::generic json{};
      expect(glz::read_json(json, "\x22\x5c\x75\xff\x22"));
   };

   "generic string_view"_test = [] {
      glz::generic json = std::string_view{"Hello"};
      expect(glz::write_json(json).value() == R"("Hello")");
      json = std::string_view{"World"};
      expect(glz::write_json(json).value() == R"("World")");
   };

   "generic int"_test = [] {
      glz::generic json = 55;
      expect(glz::write_json(json).value() == "55");
      json = 44;
      expect(glz::write_json(json).value() == "44");
   };

   "generic const char*"_test = [] {
      glz::generic j{};
      j["some key"] = "some value";
      expect(j.dump() == R"({"some key":"some value"})");
   };
};

glz::generic create_test_json()
{
   glz::generic root;

   // Create nested object structure
   root["user"]["name"] = "John Doe";
   root["user"]["age"] = 30.0;
   root["user"]["active"] = true;
   root["user"]["address"]["street"] = "123 Main St";
   root["user"]["address"]["city"] = "Anytown";
   root["user"]["address"]["zip"] = "12345";

   // Create array structure
   root["items"] = glz::generic::array_t{};
   auto& items = root["items"].get_array();

   glz::generic item1;
   item1["id"] = 1.0;
   item1["name"] = "Widget A";
   item1["price"] = 19.99;
   items.push_back(std::move(item1));

   glz::generic item2;
   item2["id"] = 2.0;
   item2["name"] = "Widget B";
   item2["price"] = 29.99;
   items.push_back(std::move(item2));

   glz::generic item3;
   item3["id"] = 3.0;
   item3["name"] = "Widget C";
   item3["price"] = 39.99;
   items.push_back(std::move(item3));

   // Create mixed nested structure
   root["metadata"]["version"] = "1.0";
   root["metadata"]["tags"] = glz::generic::array_t{"production", "stable", "v1"};

   return root;
}

suite json_pointer_extraction_tests = [] {
   "seek_extract_string"_test = [] {
      auto json = create_test_json();
      std::string result;

      bool found = glz::seek(
         [&](auto&& val) {
            if constexpr (std::same_as<std::decay_t<decltype(val)>, std::string>) {
               result = val;
            }
         },
         json, "/user/name");

      expect(found) << "Should find user name\n";
      expect(result == "John Doe") << "Should extract correct user name\n";
   };

   "seek_extract_number"_test = [] {
      auto json = create_test_json();
      double age = 0.0;

      bool found = glz::seek(
         [&](auto&& val) {
            if constexpr (std::same_as<std::decay_t<decltype(val)>, double>) {
               age = val;
            }
         },
         json, "/user/age");

      expect(found) << "Should find user age\n";
      expect(age == 30.0) << "Should extract correct age\n";
   };

   "seek_extract_boolean"_test = [] {
      auto json = create_test_json();
      bool active = false;

      bool found = glz::seek(
         [&](auto&& val) {
            if constexpr (std::same_as<std::decay_t<decltype(val)>, bool>) {
               active = val;
            }
         },
         json, "/user/active");

      expect(found) << "Should find user active status\n";
      expect(active == true) << "Should extract correct active status\n";
   };

   "get_string_reference"_test = [] {
      auto json = create_test_json();

      auto name_ref = glz::get<std::string>(json, "/user/name");
      expect(name_ref.has_value()) << "Should successfully get string reference\n";
      expect(name_ref->get() == "John Doe") << "Should get correct name value\n";
   };

   "get_number_reference"_test = [] {
      auto json = create_test_json();

      auto age_ref = glz::get<double>(json, "/user/age");
      expect(age_ref.has_value()) << "Should successfully get number reference\n";
      expect(age_ref->get() == 30.0) << "Should get correct age value\n";
   };

   "get_boolean_reference"_test = [] {
      auto json = create_test_json();

      auto active_ref = glz::get<bool>(json, "/user/active");
      expect(active_ref.has_value()) << "Should successfully get boolean reference\n";
      expect(active_ref->get() == true) << "Should get correct active value\n";
   };

   "get_nested_string"_test = [] {
      auto json = create_test_json();

      auto city_ref = glz::get<std::string>(json, "/user/address/city");
      expect(city_ref.has_value()) << "Should successfully get nested string\n";
      expect(city_ref->get() == "Anytown") << "Should get correct city value\n";
   };

   "get_array_element_string"_test = [] {
      auto json = create_test_json();

      auto item_name_ref = glz::get<std::string>(json, "/items/1/name");
      expect(item_name_ref.has_value()) << "Should successfully get array element string\n";
      expect(item_name_ref->get() == "Widget B") << "Should get correct item name\n";
   };

   "get_array_element_number"_test = [] {
      auto json = create_test_json();

      auto item_id_ref = glz::get<double>(json, "/items/0/id");
      expect(item_id_ref.has_value()) << "Should successfully get array element number\n";
      expect(item_id_ref->get() == 1.0) << "Should get correct item id\n";

      auto item_price_ref = glz::get<double>(json, "/items/2/price");
      expect(item_price_ref.has_value()) << "Should successfully get item price\n";
      expect(item_price_ref->get() == 39.99) << "Should get correct item price\n";
   };

   "get_if_string_success"_test = [] {
      auto json = create_test_json();

      auto name_ptr = glz::get_if<std::string>(json, "/user/name");
      expect(name_ptr != nullptr) << "Should get valid pointer to string\n";
      expect(*name_ptr == "John Doe") << "Should get correct name value\n";
   };

   "get_if_number_success"_test = [] {
      auto json = create_test_json();

      auto age_ptr = glz::get_if<double>(json, "/user/age");
      expect(age_ptr != nullptr) << "Should get valid pointer to number\n";
      expect(*age_ptr == 30.0) << "Should get correct age value\n";
   };

   "get_if_failure_wrong_type"_test = [] {
      auto json = create_test_json();

      // Try to get string as number
      auto wrong_type_ptr = glz::get_if<double>(json, "/user/name");
      expect(wrong_type_ptr == nullptr) << "Should get null pointer for wrong type\n";

      // Try to get number as string
      auto wrong_type_ptr2 = glz::get_if<std::string>(json, "/user/age");
      expect(wrong_type_ptr2 == nullptr) << "Should get null pointer for wrong type\n";
   };

   "get_if_failure_invalid_path"_test = [] {
      auto json = create_test_json();

      auto invalid_ptr = glz::get_if<std::string>(json, "/nonexistent/path");
      expect(invalid_ptr == nullptr) << "Should get null pointer for invalid path\n";

      auto out_of_bounds_ptr = glz::get_if<double>(json, "/items/999/id");
      expect(out_of_bounds_ptr == nullptr) << "Should get null pointer for out of bounds array access\n";
   };

   "get_value_copy"_test = [] {
      auto json = create_test_json();

      auto name_copy = glz::get_value<std::string>(json, "/user/name");
      expect(name_copy.has_value()) << "Should successfully copy string value\n";
      expect(*name_copy == "John Doe") << "Should get correct copied name\n";

      auto age_copy = glz::get_value<double>(json, "/user/age");
      expect(age_copy.has_value()) << "Should successfully copy number value\n";
      expect(*age_copy == 30.0) << "Should get correct copied age\n";
   };

   "set_string_value"_test = [] {
      auto json = create_test_json();

      bool success = glz::set(json, "/user/name", std::string{"Jane Smith"});
      expect(success) << "Should successfully set string value\n";

      auto updated_name = glz::get<std::string>(json, "/user/name");
      expect(updated_name.has_value()) << "Should be able to retrieve updated value\n";
      expect(updated_name->get() == "Jane Smith") << "Should have updated name\n";
   };

   "set_number_value"_test = [] {
      auto json = create_test_json();

      bool success = glz::set(json, "/user/age", 35.0);
      expect(success) << "Should successfully set number value\n";

      auto updated_age = glz::get<double>(json, "/user/age");
      expect(updated_age.has_value()) << "Should be able to retrieve updated value\n";
      expect(updated_age->get() == 35.0) << "Should have updated age\n";
   };

   "set_boolean_value"_test = [] {
      auto json = create_test_json();

      bool success = glz::set(json, "/user/active", false);
      expect(success) << "Should successfully set boolean value\n";

      auto updated_active = glz::get<bool>(json, "/user/active");
      expect(updated_active.has_value()) << "Should be able to retrieve updated value\n";
      expect(updated_active->get() == false) << "Should have updated active status\n";
   };

   "array_of_primitives_access"_test = [] {
      auto json = create_test_json();

      auto first_tag = glz::get<std::string>(json, "/metadata/tags/0");
      expect(first_tag.has_value()) << "Should get reference to first tag\n";
      expect(first_tag->get() == "production") << "Should get correct first tag\n";

      auto second_tag = glz::get<std::string>(json, "/metadata/tags/1");
      expect(second_tag.has_value()) << "Should get reference to second tag\n";
      expect(second_tag->get() == "stable") << "Should get correct second tag\n";

      auto last_tag = glz::get<std::string>(json, "/metadata/tags/2");
      expect(last_tag.has_value()) << "Should get reference to last tag\n";
      expect(last_tag->get() == "v1") << "Should get correct last tag\n";
   };

   "json_pointer_escaping"_test = [] {
      glz::generic json;
      json["key~with~tilde"] = "tilde value";
      json["key/with/slash"] = "slash value";

      // Test tilde escaping (~0 for ~)
      auto tilde_value = glz::get<std::string>(json, "/key~0with~0tilde");
      expect(tilde_value.has_value()) << "Should handle tilde escaping\n";
      expect(tilde_value->get() == "tilde value") << "Should get correct tilde value\n";

      // Test slash escaping (~1 for /)
      auto slash_value = glz::get<std::string>(json, "/key~1with~1slash");
      expect(slash_value.has_value()) << "Should handle slash escaping\n";
      expect(slash_value->get() == "slash value") << "Should get correct slash value\n";
   };

   "type_mismatch_errors"_test = [] {
      auto json = create_test_json();

      // Try to get string as bool
      auto wrong_bool = glz::get<bool>(json, "/user/name");
      expect(not wrong_bool.has_value()) << "Should fail when requesting string as bool\n";

      // Try to get number as string
      auto wrong_string = glz::get<std::string>(json, "/user/age");
      expect(not wrong_string.has_value()) << "Should fail when requesting number as string\n";

      // Try to get object as primitive
      auto wrong_primitive = glz::get<double>(json, "/user");
      expect(not wrong_primitive.has_value()) << "Should fail when requesting object as primitive\n";
   };

   "const_json_access"_test = [] {
      const auto json = create_test_json();

      // Test const access
      auto name = glz::get<std::string>(json, "/user/name");
      expect(name.has_value()) << "Should get string from const json\n";
      expect(name->get() == "John Doe") << "Should get correct value from const json\n";

      auto age = glz::get<double>(json, "/user/age");
      expect(age.has_value()) << "Should get number from const json\n";
      expect(age->get() == 30.0) << "Should get correct age from const json\n";
   };

   "simple_usage_example"_test = [] {
      glz::generic json{{"test", true}};

      auto result = glz::get<bool>(json, "/test");
      expect(result.has_value()) << "Should successfully get boolean value\n";
      expect(result->get() == true) << "Should get correct boolean value\n";

      // Also test get_if
      auto bool_ptr = glz::get_if<bool>(json, "/test");
      expect(bool_ptr != nullptr) << "Should get valid pointer\n";
      expect(*bool_ptr == true) << "Should get correct value via pointer\n";
   };
};

// Define structs at namespace level for linkage requirements
struct Thing
{
   int value1;
   std::string value2;
};

struct Person
{
   std::string name;
   int age;
   std::vector<std::string> hobbies;
};

struct Address
{
   std::string street;
   std::string city;
   int zip;
};

struct Employee
{
   std::string name;
   Address address;
   double salary;
};

suite struct_assignment_tests = [] {
   "struct_to_generic_assignment"_test = [] {
      Thing t{42, "hello, world!"};
      glz::generic document;
      document["some"]["nested"]["key"] = t;

      auto json_str = document.dump();
      expect(json_str.has_value()) << "Should successfully dump generic to string";

      auto expected = R"({"some":{"nested":{"key":{"value1":42,"value2":"hello, world!"}}}})";
      expect(json_str.value() == expected) << "Should produce correct nested JSON structure";

      // Also verify we can access the values
      expect(document["some"]["nested"]["key"]["value1"].get<double>() == 42.0);
      expect(document["some"]["nested"]["key"]["value2"].get<std::string>() == "hello, world!");
   };

   "complex_struct_assignment"_test = [] {
      Person p{"Alice", 30, {"reading", "gaming", "cooking"}};
      glz::generic json;
      json["person"] = p;

      auto json_str = json.dump();
      expect(json_str.has_value()) << "Should successfully serialize complex struct";

      // Verify the structure
      expect(json["person"]["name"].get<std::string>() == "Alice");
      expect(json["person"]["age"].get<double>() == 30.0);
      expect(json["person"]["hobbies"][0].get<std::string>() == "reading");
      expect(json["person"]["hobbies"][1].get<std::string>() == "gaming");
      expect(json["person"]["hobbies"][2].get<std::string>() == "cooking");
   };

   "nested_struct_assignment"_test = [] {
      Employee e{"Bob", {"123 Main St", "Anytown", 12345}, 75000.50};
      glz::generic json;
      json["employee"] = e;

      auto json_str = json.dump();
      expect(json_str.has_value()) << "Should successfully serialize nested struct";

      // Verify nested structure
      expect(json["employee"]["name"].get<std::string>() == "Bob");
      expect(json["employee"]["address"]["street"].get<std::string>() == "123 Main St");
      expect(json["employee"]["address"]["city"].get<std::string>() == "Anytown");
      expect(json["employee"]["address"]["zip"].get<double>() == 12345.0);
      expect(json["employee"]["salary"].get<double>() == 75000.50);
   };
};

suite fuzz_tests = [] {
   "fuzz1"_test = [] {
      std::string_view s = "[true,true,tur";
      std::vector<char> buffer{s.data(), s.data() + s.size()};
      buffer.push_back('\0');
      glz::generic json{};
      auto ec = glz::read_json(json, buffer);
      expect(bool(ec));
   };
};

int main() { return 0; }
