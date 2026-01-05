// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite lazy_json_tests = [] {
   "lazy_json_read_basic"_test = [] {
      std::string buffer = R"({"name":"John","age":30,"active":true,"balance":123.45})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value()) << "Failed to parse JSON";

      auto& doc = *result;
      expect(doc.is_object());
      expect(doc.root().size() == 4u);

      // Access values lazily
      expect(doc["name"].is_string());
      expect(doc["name"].get<std::string>().value() == "John");

      expect(doc["age"].is_number());
      expect(doc["age"].get<int64_t>().value() == 30);

      expect(doc["active"].is_boolean());
      expect(doc["active"].get<bool>().value() == true);

      expect(doc["balance"].is_number());
      expect(std::abs(doc["balance"].get<double>().value() - 123.45) < 0.001);
   };

   "lazy_json_read_array"_test = [] {
      std::string buffer = R"([1, 2, 3, "hello", true, null])";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 6u);

      expect(doc[0].get<int64_t>().value() == 1);
      expect(doc[1].get<int64_t>().value() == 2);
      expect(doc[2].get<int64_t>().value() == 3);
      expect(doc[3].get<std::string>().value() == "hello");
      expect(doc[4].get<bool>().value() == true);
      expect(doc[5].is_null());
   };

   "lazy_json_nested"_test = [] {
      std::string buffer = R"({"person":{"name":"Alice","friends":["Bob","Charlie"]},"count":2})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["person"]["name"].get<std::string>().value() == "Alice");
      expect(doc["person"]["friends"][0].get<std::string>().value() == "Bob");
      expect(doc["person"]["friends"][1].get<std::string>().value() == "Charlie");
      expect(doc["count"].get<int64_t>().value() == 2);
   };

   "lazy_json_escaped_string"_test = [] {
      std::string buffer = R"({"message":"Hello\nWorld","path":"C:\\Users\\test"})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["message"].get<std::string>().value() == "Hello\nWorld");
      expect(doc["path"].get<std::string>().value() == "C:\\Users\\test");
   };

   "lazy_json_unicode_escape"_test = [] {
      std::string buffer = R"({"emoji":"\u0048\u0065\u006c\u006c\u006f"})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["emoji"].get<std::string>().value() == "Hello");
   };

   "lazy_json_write"_test = [] {
      std::string buffer = R"({"x":1,"y":2})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      std::string output;
      auto ec = glz::write_json(result->root(), output);
      expect(ec == glz::error_code::none);
      expect(output == buffer) << output;
   };

   "lazy_json_contains"_test = [] {
      std::string buffer = R"({"a":1,"b":2})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.root().contains("a"));
      expect(doc.root().contains("b"));
      expect(!doc.root().contains("c"));
   };

   "lazy_json_empty_object"_test = [] {
      std::string buffer = R"({})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_object());
      expect(doc.root().empty());
      expect(doc.root().size() == 0u);
   };

   "lazy_json_empty_array"_test = [] {
      std::string buffer = R"([])";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().empty());
      expect(doc.root().size() == 0u);
   };

   "lazy_json_null"_test = [] {
      std::string buffer = R"(null)";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_null());
      expect(doc.root().empty());
   };

   "lazy_json_number_types"_test = [] {
      std::string buffer = R"({"int":42,"float":3.14,"negative":-100,"big":9007199254740993})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["int"].get<int32_t>().value() == 42);
      expect(doc["int"].get<uint32_t>().value() == 42u);
      expect(doc["int"].get<int64_t>().value() == 42);
      expect(doc["int"].get<double>().value() == 42.0);

      expect(std::abs(doc["float"].get<double>().value() - 3.14) < 0.001);
      expect(doc["negative"].get<int64_t>().value() == -100);

      // Big integers
      expect(doc["big"].get<int64_t>().value() == 9007199254740993);
   };

   "lazy_json_raw_string_view"_test = [] {
      std::string buffer = R"({"simple":"hello","escaped":"hello\\world"})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;

      // For simple strings without escapes, string_view works
      auto simple_sv = doc["simple"].get<std::string_view>();
      expect(simple_sv.has_value());
      expect(simple_sv.value() == "hello");

      // For escaped strings, string_view returns raw (with escapes)
      auto escaped_sv = doc["escaped"].get<std::string_view>();
      expect(escaped_sv.has_value());
      expect(escaped_sv.value() == "hello\\\\world"); // Raw contains double backslash

      // Use get<std::string>() for proper unescaping
      expect(doc["escaped"].get<std::string>().value() == "hello\\world");
   };

   "lazy_json_explicit_bool"_test = [] {
      std::string buffer = R"({"exists":true})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      // explicit bool conversion - true if not null
      expect(static_cast<bool>(*result));
      expect(static_cast<bool>((*result)["exists"]));

      std::string null_buffer = R"(null)";
      glz::lazy_buffer null_nodes;
      auto null_result = glz::read_lazy(null_buffer, null_nodes);
      expect(null_result.has_value());
      expect(!static_cast<bool>(*null_result)); // null is false
   };

   "lazy_json_wrong_type_error"_test = [] {
      std::string buffer = R"({"str":"hello","num":42})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;

      // Trying to get string as number should fail
      auto num_result = doc["str"].get<int64_t>();
      expect(!num_result.has_value());
      expect(num_result.error().ec == glz::error_code::get_wrong_type);

      // Trying to get number as string should fail
      auto str_result = doc["num"].get<std::string>();
      expect(!str_result.has_value());
      expect(str_result.error().ec == glz::error_code::get_wrong_type);
   };

   "lazy_json_float"_test = [] {
      std::string buffer = R"({"value":2.5})";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      auto float_result = doc["value"].get<float>();
      expect(float_result.has_value());
      expect(std::abs(float_result.value() - 2.5f) < 0.001f);
   };

   "lazy_json_large_array"_test = [] {
      std::string buffer = R"([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19])";
      glz::lazy_buffer nodes;
      auto result = glz::read_lazy(buffer, nodes);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.root().size() == 20u);
      for (size_t i = 0; i < 20; ++i) {
         expect(doc[i].get<int64_t>().value() == static_cast<int64_t>(i));
      }
   };

   "lazy_json_struct_size"_test = [] {
      // Sizes differ between 32-bit and 64-bit systems
      if constexpr (sizeof(void*) == 8) {
         // 64-bit: lazy_json is 24 bytes (data* 8 + key* 8 + key_length 4 + type 1 + padding 3)
         expect(sizeof(glz::lazy_json) == 24u) << "lazy_json should be 24 bytes on 64-bit, got " << sizeof(glz::lazy_json);
         // lazy_json_view is 40 bytes: doc* (8) + data* (8) + key_ (16) + error (4) + padding (4)
         expect(sizeof(glz::lazy_json_view<glz::opts{}>) == 40u) << "lazy_json_view should be 40 bytes on 64-bit, got " << sizeof(glz::lazy_json_view<glz::opts{}>);
         expect(sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>) == 40u) << "lazy_json_view should be 40 bytes on 64-bit, got " << sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>);
      }
      else {
         // 32-bit: lazy_json is 16 bytes (data* 4 + key* 4 + key_length 4 + type 1 + padding 3)
         expect(sizeof(glz::lazy_json) == 16u) << "lazy_json should be 16 bytes on 32-bit, got " << sizeof(glz::lazy_json);
         // lazy_json_view is 20 bytes: doc* (4) + data* (4) + key_ (8) + error (4)
         expect(sizeof(glz::lazy_json_view<glz::opts{}>) == 20u) << "lazy_json_view should be 20 bytes on 32-bit, got " << sizeof(glz::lazy_json_view<glz::opts{}>);
         expect(sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>) == 20u) << "lazy_json_view should be 20 bytes on 32-bit, got " << sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>);
      }
   };

   "lazy_json_buffer_reuse"_test = [] {
      glz::lazy_buffer nodes;
      nodes.reserve(10);

      // First parse
      {
         std::string buffer1 = R"({"x":1})";
         auto result1 = glz::read_lazy(buffer1, nodes);
         expect(result1.has_value());
         expect((*result1)["x"].get<int64_t>().value() == 1);
      }

      // Clear and reuse
      nodes.clear();

      // Second parse
      {
         std::string buffer2 = R"({"y":2})";
         auto result2 = glz::read_lazy(buffer2, nodes);
         expect(result2.has_value());
         expect((*result2)["y"].get<int64_t>().value() == 2);
      }
   };
};

int main() { return 0; }
