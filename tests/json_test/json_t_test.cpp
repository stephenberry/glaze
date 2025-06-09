// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite generic_json_tests = [] {
   "generic_json_write"_test = [] {
      glz::json_t json = {{"pi", 3.141},
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
      glz::json_t json{};
      std::string buffer = R"([5,"Hello World",{"pi":3.14},null])";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json[0].get<double>() == 5.0);
      expect(json[1].get<std::string>() == "Hello World");
      expect(json[2]["pi"].get<double>() == 3.14);
      expect(json[3].holds<glz::json_t::null_t>());
   };

   "generic_json_roundtrip"_test = [] {
      glz::json_t json{};
      std::string buffer = R"([5,"Hello World",{"pi":3.14},null])";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(glz::write_json(json) == buffer);
      expect(json.dump().value() == buffer);
   };

   "generic_json_const"_test = [] {
      auto foo = [](const glz::json_t& json) { return json["s"].get<std::string>(); };
      glz::json_t json = {{"s", "hello world"}};
      expect(foo(json) == "hello world");
      expect(json.dump().value() == R"({"s":"hello world"})");

      std::map<std::string, std::string> obj{};
      expect(not glz::read_json(obj, json));
      expect(obj.contains("s"));
      expect(obj.at("s") == "hello world");
   };

   "generic_json_int"_test = [] {
      glz::json_t json = {{"i", 1}};
      expect(json["i"].get<double>() == 1);
      expect(json.dump().value() == R"({"i":1})");
   };

   "generic_json_as"_test = [] {
      glz::json_t json = {{"pi", 3.141},
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
      static const glz::json_t messageSchema = {{"type", "struct"},
                                                {"fields",
                                                 {
                                                    {{"field", "branch"}, {"type", "string"}},
                                                 }}};
      std::string buffer{};
      expect(not glz::write_json(messageSchema, buffer));
      expect(buffer == R"({"fields":[{"field":"branch","type":"string"}],"type":"struct"})") << buffer;
   };

   "json_t_contains"_test = [] {
      auto json = glz::read_json<glz::json_t>(R"({"foo":"bar"})");
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
      glz::json_t json{};
      expect(glz::read_json(json, buffer) == glz::error_code::parse_number_failure);
   };

   "json_t copy construction"_test = [] {
      std::string s{};
      expect(not glz::write_json(glz::json_t(*(glz::read_json<glz::json_t>("{}"))), s));
      expect(s == "{}") << s;
   };

   "json_t is_object"_test = [] {
      glz::json_t json{};
      expect(not glz::read_json(json, "{}"));
      expect(json.is_object());
      expect(glz::is_object(json));
      expect(json.empty());
      expect(json.size() == 0);
      expect(json.get_object().size() == 0);
   };

   "json_t is_object"_test = [] {
      glz::json_t json{};
      expect(not glz::read_json(json, R"({"age":"22","name":"Noah"})"));
      expect(json.is_object());
      expect(glz::is_object(json));
      expect(not json.empty());
      expect(json.size() == 2);
      expect(json.get_object().size() == 2);
   };

   "json_t is_array"_test = [] {
      glz::json_t json{};
      expect(not glz::read_json(json, "[]"));
      expect(json.is_array());
      expect(glz::is_array(json));
      expect(json.empty());
      expect(json.size() == 0);
      expect(json.get_array().size() == 0);
   };

   "json_t is_array"_test = [] {
      glz::json_t json{};
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

   "json_t is_string"_test = [] {
      glz::json_t json{};
      expect(not glz::read_json(json, R"("")"));
      expect(json.is_string());
      expect(glz::is_string(json));
      expect(json.empty());
      expect(json.size() == 0);
      expect(json.get_string() == "");
   };

   "json_t is_string"_test = [] {
      glz::json_t json{};
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

   "json_t is_number"_test = [] {
      glz::json_t json{};
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

   "json_t is_boolean"_test = [] {
      glz::json_t json{};
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

   "json_t is_null"_test = [] {
      glz::json_t json{};
      expect(not glz::read_json(json, "null"));
      expect(json.is_null());
      expect(glz::is_null(json));
      expect(json.empty());
      expect(json.size() == 0);
   };

   "json_t garbage input"_test = [] {
      glz::json_t json{};
      expect(glz::read_json(json, "\x22\x5c\x75\xff\x22"));
   };

   "json_t string_view"_test = [] {
      glz::json_t json = std::string_view{"Hello"};
      expect(glz::write_json(json).value() == R"("Hello")");
      json = std::string_view{"World"};
      expect(glz::write_json(json).value() == R"("World")");
   };

   "json_t int"_test = [] {
      glz::json_t json = 55;
      expect(glz::write_json(json).value() == "55");
      json = 44;
      expect(glz::write_json(json).value() == "44");
   };

   "json_t const char*"_test = [] {
      glz::json_t j{};
      j["some key"] = "some value";
      expect(j.dump() == R"({"some key":"some value"})");
   };
};

int main() { return 0; }
