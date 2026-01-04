// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite lazy_json_tests = [] {
   "lazy_json_read_basic"_test = [] {
      std::string buffer = R"({"name":"John","age":30,"active":true,"balance":123.45})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value()) << "Failed to parse JSON";

      auto& lazy = *result;
      expect(lazy.is_object());
      expect(lazy.size() == 4u);

      // Access values lazily
      expect(lazy["name"].is_string());
      expect(lazy["name"].get<std::string>().value() == "John");

      expect(lazy["age"].is_number());
      expect(lazy["age"].get<int64_t>().value() == 30);

      expect(lazy["active"].is_boolean());
      expect(lazy["active"].get<bool>().value() == true);

      expect(lazy["balance"].is_number());
      expect(std::abs(lazy["balance"].get<double>().value() - 123.45) < 0.001);
   };

   "lazy_json_read_array"_test = [] {
      std::string buffer = R"([1, 2, 3, "hello", true, null])";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy.is_array());
      expect(lazy.size() == 6u);

      expect(lazy[0].get<int64_t>().value() == 1);
      expect(lazy[1].get<int64_t>().value() == 2);
      expect(lazy[2].get<int64_t>().value() == 3);
      expect(lazy[3].get<std::string>().value() == "hello");
      expect(lazy[4].get<bool>().value() == true);
      expect(lazy[5].is_null());
   };

   "lazy_json_nested"_test = [] {
      std::string buffer = R"({"person":{"name":"Alice","friends":["Bob","Charlie"]},"count":2})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy["person"]["name"].get<std::string>().value() == "Alice");
      expect(lazy["person"]["friends"][0].get<std::string>().value() == "Bob");
      expect(lazy["person"]["friends"][1].get<std::string>().value() == "Charlie");
      expect(lazy["count"].get<int64_t>().value() == 2);
   };

   "lazy_json_escaped_string"_test = [] {
      std::string buffer = R"({"message":"Hello\nWorld","path":"C:\\Users\\test"})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy["message"].get<std::string>().value() == "Hello\nWorld");
      expect(lazy["path"].get<std::string>().value() == "C:\\Users\\test");
   };

   "lazy_json_unicode_escape"_test = [] {
      std::string buffer = R"({"emoji":"\u0048\u0065\u006c\u006c\u006f"})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy["emoji"].get<std::string>().value() == "Hello");
   };

   "lazy_json_write"_test = [] {
      std::string buffer = R"({"x":1,"y":2})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      std::string output;
      auto ec = glz::write_json(*result, output);
      expect(ec == glz::error_code::none);
      expect(output == buffer) << output;
   };

   "lazy_json_contains"_test = [] {
      std::string buffer = R"({"a":1,"b":2})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy.contains("a"));
      expect(lazy.contains("b"));
      expect(!lazy.contains("c"));
   };

   "lazy_json_empty_object"_test = [] {
      std::string buffer = R"({})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy.is_object());
      expect(lazy.empty());
      expect(lazy.size() == 0u);
   };

   "lazy_json_empty_array"_test = [] {
      std::string buffer = R"([])";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy.is_array());
      expect(lazy.empty());
      expect(lazy.size() == 0u);
   };

   "lazy_json_null"_test = [] {
      std::string buffer = R"(null)";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy.is_null());
      expect(lazy.empty());
   };

   "lazy_json_number_types"_test = [] {
      std::string buffer = R"({"int":42,"float":3.14,"negative":-100,"big":9007199254740993})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy["int"].get<int32_t>().value() == 42);
      expect(lazy["int"].get<uint32_t>().value() == 42u);
      expect(lazy["int"].get<int64_t>().value() == 42);
      expect(lazy["int"].get<double>().value() == 42.0);

      expect(std::abs(lazy["float"].get<double>().value() - 3.14) < 0.001);
      expect(lazy["negative"].get<int64_t>().value() == -100);

      // Big integers
      expect(lazy["big"].get<int64_t>().value() == 9007199254740993);
   };

   "lazy_json_raw_string_view"_test = [] {
      std::string buffer = R"({"simple":"hello","escaped":"hello\\world"})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;

      // For simple strings without escapes, string_view works
      auto simple_sv = lazy["simple"].get<std::string_view>();
      expect(simple_sv.has_value());
      expect(simple_sv.value() == "hello");

      // For escaped strings, string_view returns raw (with escapes)
      auto escaped_sv = lazy["escaped"].get<std::string_view>();
      expect(escaped_sv.has_value());
      expect(escaped_sv.value() == "hello\\\\world"); // Raw contains double backslash

      // Use get<std::string>() for proper unescaping
      expect(lazy["escaped"].get<std::string>().value() == "hello\\world");
   };

   "lazy_json_dump"_test = [] {
      std::string buffer = R"({"key":"value"})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      // dump() returns the original JSON string_view
      expect(result->dump() == buffer);
   };

   "lazy_json_explicit_bool"_test = [] {
      std::string buffer = R"({"exists":true})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      // explicit bool conversion - true if not null
      expect(static_cast<bool>(*result));
      expect(static_cast<bool>((*result)["exists"]));

      std::string null_buffer = R"(null)";
      auto null_result = glz::read_lazy(null_buffer);
      expect(null_result.has_value());
      expect(!static_cast<bool>(*null_result)); // null is false
   };

   "lazy_json_wrong_type_error"_test = [] {
      std::string buffer = R"({"str":"hello","num":42})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;

      // Trying to get string as number should fail
      auto num_result = lazy["str"].get<int64_t>();
      expect(!num_result.has_value());
      expect(num_result.error().ec == glz::error_code::get_wrong_type);

      // Trying to get number as string should fail
      auto str_result = lazy["num"].get<std::string>();
      expect(!str_result.has_value());
      expect(str_result.error().ec == glz::error_code::get_wrong_type);
   };

   "lazy_json_float"_test = [] {
      std::string buffer = R"({"value":2.5})";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      auto float_result = lazy["value"].get<float>();
      expect(float_result.has_value());
      expect(std::abs(float_result.value() - 2.5f) < 0.001f);
   };

   "lazy_json_large_array"_test = [] {
      std::string buffer = R"([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19])";
      auto result = glz::read_lazy(buffer);
      expect(result.has_value());

      auto& lazy = *result;
      expect(lazy.size() == 20u);
      for (size_t i = 0; i < 20; ++i) {
         expect(lazy[i].get<int64_t>().value() == static_cast<int64_t>(i));
      }
   };
};

int main() { return 0; }
