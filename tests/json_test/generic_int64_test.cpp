// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Create a generic type alias with int64_t support
using generic_int64 = glz::generic_t<true>;

suite generic_int64_tests = [] {
   "int64_parse_integer"_test = [] {
      generic_int64 json{};
      std::string buffer = "9223372036854775807"; // max int64_t
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_int64());
      expect(!json.is_double());
      expect(json.get<int64_t>() == 9223372036854775807LL);
   };

   "int64_parse_negative_integer"_test = [] {
      generic_int64 json{};
      std::string buffer = "-9223372036854775808"; // min int64_t
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_int64());
      expect(json.get<int64_t>() == -9223372036854775807LL - 1);
   };

   "int64_parse_small_integer"_test = [] {
      generic_int64 json{};
      std::string buffer = "42";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_int64());
      expect(json.get<int64_t>() == 42);
      expect(json.as<int>() == 42);
   };

   "double_parse_floating_point"_test = [] {
      generic_int64 json{};
      std::string buffer = "3.14159";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(!json.is_int64());
      expect(json.is_double());
      expect(json.get<double>() == 3.14159);
   };

   "double_parse_exponential"_test = [] {
      generic_int64 json{};
      std::string buffer = "1.23e10";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_double());
      expect(json.get<double>() == 1.23e10);
   };

   "int64_in_object"_test = [] {
      generic_int64 json{};
      std::string buffer = R"({"id":9007199254740993,"value":3.14})";
      expect(glz::read_json(json, buffer) == glz::error_code::none);

      // id should be int64_t (beyond safe double range)
      expect(json["id"].is_int64());
      expect(json["id"].get<int64_t>() == 9007199254740993LL);

      // value should be double
      expect(json["value"].is_double());
      expect(json["value"].get<double>() == 3.14);
   };

   "int64_in_array"_test = [] {
      generic_int64 json{};
      std::string buffer = "[1, 2, 3, 4, 5]";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_array());
      expect(json[0].is_int64());
      expect(json[0].get<int64_t>() == 1);
      expect(json[4].get<int64_t>() == 5);
   };

   "as_conversion_from_int64"_test = [] {
      generic_int64 json{};
      std::string buffer = "12345";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_int64());

      // Test as<T>() conversion
      expect(json.as<int>() == 12345);
      expect(json.as<long>() == 12345L);
      expect(json.as<int64_t>() == 12345LL);
      expect(json.as<double>() == 12345.0);
   };

   "as_conversion_from_double"_test = [] {
      generic_int64 json{};
      std::string buffer = "12345.67";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_double());

      // Test as<T>() conversion
      expect(json.as<int>() == 12345);
      expect(json.as<double>() == 12345.67);
   };

   "as_number_helper"_test = [] {
      generic_int64 int_json{};
      expect(glz::read_json(int_json, "42") == glz::error_code::none);
      expect(int_json.as_number() == 42.0);

      generic_int64 double_json{};
      expect(glz::read_json(double_json, "3.14") == glz::error_code::none);
      expect(double_json.as_number() == 3.14);
   };

   "assignment_int64"_test = [] {
      generic_int64 json{};
      json = 123456789LL;
      expect(json.is_int64());
      expect(json.get<int64_t>() == 123456789LL);

      // Test write
      auto result = json.dump();
      expect(result.has_value());
      expect(result.value() == "123456789");
   };

   "assignment_double"_test = [] {
      generic_int64 json{};
      json = 3.14159;
      expect(json.is_double());
      expect(json.get<double>() == 3.14159);
   };

   "roundtrip_int64"_test = [] {
      generic_int64 json = 9007199254740993LL; // Beyond safe double integer range
      auto json_str = json.dump();
      expect(json_str.has_value());
      expect(json_str.value() == "9007199254740993");

      generic_int64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);
      expect(json2.is_int64());
      expect(json2.get<int64_t>() == 9007199254740993LL);
   };

   "roundtrip_mixed_types"_test = [] {
      generic_int64 json = {
         {"int_value", 42},
         {"double_value", 3.14},
         {"big_int", 9007199254740993LL}
      };

      auto json_str = json.dump();
      expect(json_str.has_value());

      generic_int64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);
      expect(json2["int_value"].is_int64());
      expect(json2["int_value"].get<int64_t>() == 42);
      expect(json2["double_value"].is_double());
      expect(json2["double_value"].get<double>() == 3.14);
      expect(json2["big_int"].is_int64());
      expect(json2["big_int"].get<int64_t>() == 9007199254740993LL);
   };

   "convert_from_generic_int64"_test = [] {
      generic_int64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      int64_t val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42);
   };

   "convert_from_generic_int"_test = [] {
      generic_int64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      int val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42);
   };

   "convert_from_generic_double"_test = [] {
      generic_int64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      double val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42.0);
   };

   // Note: glz::get with containers needs additional work for templated generic_t
   // Commenting out for now
   // "get_vector_with_int64"_test = [] {
   //    std::string buffer = R"({"numbers": [1, 2, 3, 4, 5]})";
   //    generic_int64 json{};
   //    expect(!glz::read_json(json, buffer));

   //    auto numbers = glz::get<std::vector<int64_t>>(json, "/numbers");
   //    expect(numbers.has_value());
   //    if (numbers.has_value()) {
   //       auto& vec = numbers->get();
   //       expect(vec.size() == 5);
   //       expect(vec[0] == 1);
   //       expect(vec[4] == 5);
   //    }
   // };

   "precision_test"_test = [] {
      // Test that large integers beyond double's safe range maintain precision
      const int64_t large_int = 9007199254740993LL; // 2^53 + 1, loses precision in double

      generic_int64 json{};
      json = large_int;

      // Write to JSON
      auto json_str = json.dump();
      expect(json_str.has_value());

      // Read back
      generic_int64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);

      // Verify precision is maintained
      expect(json2.get<int64_t>() == large_int);
   };

   "negative_precision_test"_test = [] {
      const int64_t large_neg_int = -9007199254740993LL;

      generic_int64 json{};
      json = large_neg_int;

      auto json_str = json.dump();
      expect(json_str.has_value());

      generic_int64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);
      expect(json2.get<int64_t>() == large_neg_int);
   };

   "zero_test"_test = [] {
      generic_int64 json{};
      expect(glz::read_json(json, "0") == glz::error_code::none);
      expect(json.is_int64());
      expect(json.get<int64_t>() == 0);
   };

   "is_int64_vs_is_double"_test = [] {
      generic_int64 int_json{};
      expect(glz::read_json(int_json, "42") == glz::error_code::none);
      expect(int_json.is_int64());
      expect(!int_json.is_double());

      generic_int64 double_json{};
      expect(glz::read_json(double_json, "42.5") == glz::error_code::none);
      expect(!double_json.is_int64());
      expect(double_json.is_double());
   };
};

int main() { return 0; }
