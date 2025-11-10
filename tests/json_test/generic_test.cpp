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

// Tests for issue #1807: glz::get<T> with container types
// https://github.com/stephenberry/glaze/issues/1807
suite issue_1807_tests = [] {
   "get_vector_from_generic"_test = [] {
      std::string buffer = R"({"test0": false, "test1": ["alice", "bob"]})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // These should work
      auto test0_result = glz::get<bool>(json, "/test0");
      expect(test0_result.has_value()) << "Should get bool value";
      expect(*test0_result == false) << "Should get correct bool value";

      // This should work now with the new overload
      auto test1_result = glz::get<std::vector<std::string>>(json, "/test1");
      expect(test1_result.has_value()) << "Should get vector<string> value";
      if (test1_result.has_value()) {
         auto& vec = *test1_result;
         expect(vec.size() == 2) << "Vector should have 2 elements";
         expect(vec[0] == "alice") << "First element should be 'alice'";
         expect(vec[1] == "bob") << "Second element should be 'bob'";
      }

      // Individual element access works (returns reference wrapper)
      auto elem0_result = glz::get<std::string>(json, "/test1/0");
      expect(elem0_result.has_value()) << "Should get first array element";
      expect(elem0_result->get() == "alice") << "First element should be 'alice'";
   };

   "get_map_from_generic"_test = [] {
      std::string buffer = R"({"test0": false, "test1": {"0": "alice", "1": "bob"}})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      auto test0_result = glz::get<bool>(json, "/test0");
      expect(test0_result.has_value()) << "Should get bool value";

      // This should work now with the new overload
      auto test1_result = glz::get<std::map<std::string, std::string>>(json, "/test1");
      expect(test1_result.has_value()) << "Should get map<string, string> value";
      if (test1_result.has_value()) {
         auto& map = *test1_result;
         expect(map.size() == 2) << "Map should have 2 elements";
         expect(map.at("0") == "alice") << "First element should be 'alice'";
         expect(map.at("1") == "bob") << "Second element should be 'bob'";
      }

      // Individual element access works (returns reference wrapper)
      auto elem0_result = glz::get<std::string>(json, "/test1/0");
      expect(elem0_result.has_value()) << "Should get map element";
      expect(elem0_result->get() == "alice") << "Element should be 'alice'";
   };

   "get_array_from_generic"_test = [] {
      std::string buffer = R"({"items": [1, 2, 3, 4, 5]})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // This should work now with the new overload
      auto result = glz::get<std::array<int, 5>>(json, "/items");
      expect(result.has_value()) << "Should get array<int, 5> value";
      if (result.has_value()) {
         auto& arr = *result;
         expect(arr[0] == 1) << "First element should be 1";
         expect(arr[4] == 5) << "Last element should be 5";
      }
   };

   "get_list_from_generic"_test = [] {
      std::string buffer = R"({"tags": ["tag1", "tag2", "tag3"]})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // This should work now with the new overload
      auto result = glz::get<std::list<std::string>>(json, "/tags");
      expect(result.has_value()) << "Should get list<string> value";
      if (result.has_value()) {
         auto& list = *result;
         expect(list.size() == 3) << "List should have 3 elements";
         auto it = list.begin();
         expect(*it == "tag1") << "First element should be 'tag1'";
      }
   };

   "get_nested_containers"_test = [] {
      std::string buffer = R"({
         "matrix": [[1, 2], [3, 4], [5, 6]],
         "table": {"row1": [10, 20], "row2": [30, 40]}
      })";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // Test vector of vectors
      auto matrix = glz::get<std::vector<std::vector<int>>>(json, "/matrix");
      expect(matrix.has_value()) << "Should get nested vector";
      if (matrix.has_value()) {
         expect(matrix->size() == 3) << "Matrix should have 3 rows";
         expect((*matrix)[0].size() == 2) << "First row should have 2 elements";
         expect((*matrix)[0][0] == 1) << "First element should be 1";
         expect((*matrix)[2][1] == 6) << "Last element should be 6";
      }

      // Test map of vectors
      auto table = glz::get<std::map<std::string, std::vector<int>>>(json, "/table");
      expect(table.has_value()) << "Should get map of vectors";
      if (table.has_value()) {
         expect(table->size() == 2) << "Table should have 2 rows";
         expect(table->at("row1").size() == 2) << "Row1 should have 2 elements";
         expect(table->at("row1")[0] == 10) << "First element of row1 should be 10";
         expect(table->at("row2")[1] == 40) << "Last element of row2 should be 40";
      }
   };

   "get_empty_containers"_test = [] {
      std::string buffer = R"({
         "empty_array": [],
         "empty_object": {}
      })";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // Test empty vector
      auto empty_vec = glz::get<std::vector<int>>(json, "/empty_array");
      expect(empty_vec.has_value()) << "Should get empty vector";
      if (empty_vec.has_value()) {
         expect(empty_vec->empty()) << "Vector should be empty";
      }

      // Test empty map
      auto empty_map = glz::get<std::map<std::string, int>>(json, "/empty_object");
      expect(empty_map.has_value()) << "Should get empty map";
      if (empty_map.has_value()) {
         expect(empty_map->empty()) << "Map should be empty";
      }
   };

   "get_integer_conversion"_test = [] {
      std::string buffer = R"({"numbers": [1.0, 2.5, 3.7, 4.9]})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // Test conversion from double to int
      auto numbers = glz::get<std::vector<int>>(json, "/numbers");
      expect(numbers.has_value()) << "Should get vector<int> from doubles";
      if (numbers.has_value()) {
         expect(numbers->size() == 4) << "Should have 4 elements";
         expect((*numbers)[0] == 1) << "First element should be 1";
         expect((*numbers)[1] == 2) << "Second element should be 2 (truncated)";
         expect((*numbers)[2] == 3) << "Third element should be 3 (truncated)";
         expect((*numbers)[3] == 4) << "Fourth element should be 4 (truncated)";
      }
   };

   "get_error_wrong_type"_test = [] {
      std::string buffer = R"({"value": "not a number", "data": {"nested": true}})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // Try to get string as vector
      auto wrong_vec = glz::get<std::vector<int>>(json, "/value");
      expect(!wrong_vec.has_value()) << "Should fail to get string as vector";

      // Try to get object as vector
      auto wrong_vec2 = glz::get<std::vector<int>>(json, "/data");
      expect(!wrong_vec2.has_value()) << "Should fail to get object as vector";

      // Try to get object as map with wrong value type
      auto wrong_map = glz::get<std::map<std::string, int>>(json, "/data");
      expect(!wrong_map.has_value()) << "Should fail to convert boolean to int";
   };

   "get_error_nonexistent_path"_test = [] {
      std::string buffer = R"({"data": [1, 2, 3]})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // Try to access non-existent path
      auto result = glz::get<std::vector<int>>(json, "/nonexistent");
      expect(!result.has_value()) << "Should fail to get non-existent path";

      // Try to access out of bounds array index
      auto result2 = glz::get<std::vector<int>>(json, "/data/10");
      expect(!result2.has_value()) << "Should fail to get out of bounds index";
   };

   "get_deque_container"_test = [] {
      std::string buffer = R"({"queue": [10, 20, 30, 40]})";
      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      auto result = glz::get<std::deque<int>>(json, "/queue");
      expect(result.has_value()) << "Should get deque<int> value";
      if (result.has_value()) {
         auto& dq = *result;
         expect(dq.size() == 4) << "Deque should have 4 elements";
         expect(dq.front() == 10) << "Front should be 10";
         expect(dq.back() == 40) << "Back should be 40";
      }
   };

   "convert_from_generic_lowlevel_api"_test = [] {
      // Test the lower-level convert_from_generic API directly
      glz::generic arr_json{};
      expect(!glz::read_json(arr_json, R"([1, 2, 3, 4, 5])"));

      std::vector<int> vec;
      auto ec = glz::convert_from_generic(vec, arr_json);
      expect(!ec) << "Should convert array to vector";
      expect(vec.size() == 5) << "Vector should have 5 elements";
      expect(vec[0] == 1 && vec[4] == 5) << "Elements should be correct";

      glz::generic obj_json{};
      expect(!glz::read_json(obj_json, R"({"a": 1, "b": 2})"));

      std::map<std::string, int> map;
      ec = glz::convert_from_generic(map, obj_json);
      expect(!ec) << "Should convert object to map";
      expect(map.size() == 2) << "Map should have 2 elements";
      expect(map["a"] == 1 && map["b"] == 2) << "Elements should be correct";

      // Test primitive conversion
      glz::generic num_json{};
      expect(!glz::read_json(num_json, "42.5"));

      int num;
      ec = glz::convert_from_generic(num, num_json);
      expect(!ec) << "Should convert number to int";
      expect(num == 42) << "Number should be 42";
   };

   "large_container_conversion"_test = [] {
      // Build a large array
      std::string buffer = "[";
      for (int i = 0; i < 1000; ++i) {
         if (i > 0) buffer += ",";
         buffer += std::to_string(i);
      }
      buffer += "]";

      glz::generic json{};
      expect(!glz::read_json(json, buffer));

      // Convert to vector - tests performance of direct traversal
      std::vector<int> result;
      auto ec = glz::convert_from_generic(result, json);
      expect(!ec) << "Should convert large array";
      expect(result.size() == 1000) << "Should have 1000 elements";
      expect(result[0] == 0) << "First element should be 0";
      expect(result[999] == 999) << "Last element should be 999";
   };
};

// Tests for optimized read_json from generic
suite optimized_read_json_tests = [] {
   "read_json_vector_optimized"_test = [] {
      glz::generic json{};
      expect(!glz::read_json(json, R"([1, 2, 3, 4, 5])"));

      // This should use the optimized direct-traversal path
      std::vector<int> vec;
      expect(!glz::read_json(vec, json));
      expect(vec.size() == 5) << "Vector should have 5 elements";
      expect(vec[0] == 1 && vec[4] == 5) << "Elements should be correct";
   };

   "read_json_map_optimized"_test = [] {
      glz::generic json{};
      expect(!glz::read_json(json, R"({"a": 10, "b": 20})"));

      // This should use the optimized direct-traversal path
      std::map<std::string, int> map;
      expect(!glz::read_json(map, json));
      expect(map.size() == 2) << "Map should have 2 elements";
      expect(map["a"] == 10 && map["b"] == 20) << "Elements should be correct";
   };

   "read_json_primitive_optimized"_test = [] {
      glz::generic num_json{};
      expect(!glz::read_json(num_json, "42.5"));

      int num;
      expect(!glz::read_json(num, num_json));
      expect(num == 42) << "Should convert double to int";

      glz::generic str_json{};
      expect(!glz::read_json(str_json, R"("hello world")"));

      std::string str;
      expect(!glz::read_json(str, str_json));
      expect(str == "hello world") << "Should get string value";
   };

   "read_json_expected_form_optimized"_test = [] {
      glz::generic json{};
      expect(!glz::read_json(json, R"([10, 20, 30])"));

      // Test the expected<T> form
      auto vec_result = glz::read_json<std::vector<int>>(json);
      expect(vec_result.has_value()) << "Should successfully convert";
      if (vec_result.has_value()) {
         expect(vec_result->size() == 3) << "Should have 3 elements";
         expect((*vec_result)[1] == 20) << "Middle element should be 20";
      }
   };

   "read_json_nested_containers_optimized"_test = [] {
      glz::generic json{};
      expect(!glz::read_json(json, R"([[1, 2], [3, 4], [5, 6]])"));

      std::vector<std::vector<int>> matrix;
      expect(!glz::read_json(matrix, json));
      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[0].size() == 2) << "Each row should have 2 elements";
      expect(matrix[2][1] == 6) << "Last element should be 6";
   };

   "read_json_struct_still_works"_test = [] {
      // Test that structs still work (using JSON round-trip)
      glz::generic json{};
      expect(!glz::read_json(json, R"({"name":"Alice","age":30,"hobbies":["reading","gaming"]})"));

      Person person;
      expect(!glz::read_json(person, json));
      expect(person.name == "Alice") << "Name should be correct";
      expect(person.age == 30) << "Age should be correct";
      expect(person.hobbies.size() == 2) << "Should have 2 hobbies";
   };

   "read_with_opts_optimized"_test = [] {
      glz::generic json{};
      expect(!glz::read_json(json, R"([1, 2, 3])"));

      // Test the templated read<Opts> function
      std::vector<int> vec;
      expect(!glz::read<glz::opts{}>(vec, json));
      expect(vec.size() == 3) << "Vector should have 3 elements";
      expect(vec[0] == 1) << "First element should be 1";
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
