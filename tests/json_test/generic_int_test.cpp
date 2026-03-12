// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite generic_i64_tests = [] {
   "i64_parse_integer"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "9223372036854775807"; // max int64_t
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_int64());
      expect(!json.is_double());
      expect(json.get<int64_t>() == 9223372036854775807LL);
   };

   "i64_parse_negative_integer"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "-9223372036854775808"; // min int64_t
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_int64());
      expect(json.get<int64_t>() == -9223372036854775807LL - 1);
   };

   "i64_parse_small_integer"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "42";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_int64());
      expect(json.get<int64_t>() == 42);
      expect(json.as<int>() == 42);
   };

   "i64_double_parse_floating_point"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "3.14159";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(!json.is_int64());
      expect(json.is_double());
      expect(json.get<double>() == 3.14159);
   };

   "i64_double_parse_exponential"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "1.23e10";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_double());
      expect(json.get<double>() == 1.23e10);
   };

   "i64_in_object"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = R"({"id":9007199254740993,"value":3.14})";
      expect(glz::read_json(json, buffer) == glz::error_code::none);

      // id should be int64_t (beyond safe double range)
      expect(json["id"].is_int64());
      expect(json["id"].get<int64_t>() == 9007199254740993LL);

      // value should be double
      expect(json["value"].is_double());
      expect(json["value"].get<double>() == 3.14);
   };

   "i64_json_ptr_get_and_get_if"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = R"({"Example":{"enabled":true,"name":"test"}})";
      expect(glz::read_json(json, buffer) == glz::error_code::none);

      auto enabled = glz::get<bool>(json, "/Example/enabled");
      expect(enabled.has_value());
      expect(enabled->get() == true);

      auto name = glz::get<std::string>(json, "/Example/name");
      expect(name.has_value());
      expect(name->get() == "test");

      auto* name_if = glz::get_if<std::string>(json, "/Example/name");
      expect(name_if != nullptr);
      expect(*name_if == "test");
   };

   "i64_in_array"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "[1, 2, 3, 4, 5]";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_array());
      expect(json[0].is_int64());
      expect(json[0].get<int64_t>() == 1);
      expect(json[4].get<int64_t>() == 5);
   };

   "i64_as_conversion_from_int64"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "12345";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_int64());

      // Test as<T>() conversion
      expect(json.as<int>() == 12345);
      expect(json.as<long>() == 12345L);
      expect(json.as<int64_t>() == 12345LL);
      expect(json.as<double>() == 12345.0);
   };

   "i64_as_conversion_from_double"_test = [] {
      glz::generic_i64 json{};
      std::string buffer = "12345.67";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_double());

      // Test as<T>() conversion
      expect(json.as<int>() == 12345);
      expect(json.as<double>() == 12345.67);
   };

   "i64_as_number_helper"_test = [] {
      glz::generic_i64 int_json{};
      expect(glz::read_json(int_json, "42") == glz::error_code::none);
      expect(int_json.as_number() == 42.0);

      glz::generic_i64 double_json{};
      expect(glz::read_json(double_json, "3.14") == glz::error_code::none);
      expect(double_json.as_number() == 3.14);
   };

   "i64_assignment"_test = [] {
      glz::generic_i64 json{};
      json = 123456789LL;
      expect(json.is_int64());
      expect(json.get<int64_t>() == 123456789LL);

      // Test write
      auto result = json.dump();
      expect(result.has_value());
      expect(result.value() == "123456789");
   };

   "i64_assignment_double"_test = [] {
      glz::generic_i64 json{};
      json = 3.14159;
      expect(json.is_double());
      expect(json.get<double>() == 3.14159);
   };

   "i64_roundtrip"_test = [] {
      glz::generic_i64 json = 9007199254740993LL; // Beyond safe double integer range
      auto json_str = json.dump();
      expect(json_str.has_value());
      expect(json_str.value() == "9007199254740993");

      glz::generic_i64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);
      expect(json2.is_int64());
      expect(json2.get<int64_t>() == 9007199254740993LL);
   };

   "i64_roundtrip_mixed_types"_test = [] {
      glz::generic_i64 json = {{"int_value", 42}, {"double_value", 3.14}, {"big_int", 9007199254740993LL}};

      auto json_str = json.dump();
      expect(json_str.has_value());

      glz::generic_i64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);
      expect(json2["int_value"].is_int64());
      expect(json2["int_value"].get<int64_t>() == 42);
      expect(json2["double_value"].is_double());
      expect(json2["double_value"].get<double>() == 3.14);
      expect(json2["big_int"].is_int64());
      expect(json2["big_int"].get<int64_t>() == 9007199254740993LL);
   };

   "i64_convert_from_generic"_test = [] {
      glz::generic_i64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      int64_t val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42);
   };

   "i64_convert_from_generic_int"_test = [] {
      glz::generic_i64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      int val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42);
   };

   "i64_convert_from_generic_double"_test = [] {
      glz::generic_i64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      double val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42.0);
   };

   "i64_precision_test"_test = [] {
      // Test that large integers beyond double's safe range maintain precision
      const int64_t large_int = 9007199254740993LL; // 2^53 + 1, loses precision in double

      glz::generic_i64 json{};
      json = large_int;

      // Write to JSON
      auto json_str = json.dump();
      expect(json_str.has_value());

      // Read back
      glz::generic_i64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);

      // Verify precision is maintained
      expect(json2.get<int64_t>() == large_int);
   };

   "i64_negative_precision_test"_test = [] {
      const int64_t large_neg_int = -9007199254740993LL;

      glz::generic_i64 json{};
      json = large_neg_int;

      auto json_str = json.dump();
      expect(json_str.has_value());

      glz::generic_i64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);
      expect(json2.get<int64_t>() == large_neg_int);
   };

   "i64_zero_test"_test = [] {
      glz::generic_i64 json{};
      expect(glz::read_json(json, "0") == glz::error_code::none);
      expect(json.is_int64());
      expect(json.get<int64_t>() == 0);
   };

   "i64_is_int64_vs_is_double"_test = [] {
      glz::generic_i64 int_json{};
      expect(glz::read_json(int_json, "42") == glz::error_code::none);
      expect(int_json.is_int64());
      expect(!int_json.is_double());

      glz::generic_i64 double_json{};
      expect(glz::read_json(double_json, "42.5") == glz::error_code::none);
      expect(!double_json.is_int64());
      expect(double_json.is_double());
   };
};

// Tests for generic_u64 (uint64_t → int64_t → double)
suite generic_u64_tests = [] {
   "u64_parse_large_unsigned"_test = [] {
      glz::generic_u64 json{};
      std::string buffer = "18446744073709551615"; // max uint64_t
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_uint64());
      expect(json.get<uint64_t>() == 18446744073709551615ULL);
   };

   "u64_parse_negative"_test = [] {
      glz::generic_u64 json{};
      std::string buffer = "-9223372036854775808"; // min int64_t
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(json.is_int64());
      expect(!json.is_uint64());
      expect(json.get<int64_t>() == -9223372036854775807LL - 1);
   };

   "u64_parse_small_positive"_test = [] {
      glz::generic_u64 json{};
      std::string buffer = "42";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      // Small positive integers should be stored as uint64_t
      expect(json.is_uint64());
      expect(json.get<uint64_t>() == 42);
   };

   "u64_parse_floating_point"_test = [] {
      glz::generic_u64 json{};
      std::string buffer = "3.14159";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json.is_number());
      expect(!json.is_uint64());
      expect(!json.is_int64());
      expect(json.is_double());
      expect(json.get<double>() == 3.14159);
   };

   "u64_in_object"_test = [] {
      glz::generic_u64 json{};
      std::string buffer = R"({"big_id":18446744073709551615,"neg":-100,"value":3.14})";
      expect(glz::read_json(json, buffer) == glz::error_code::none);

      // big_id should be uint64_t
      expect(json["big_id"].is_uint64());
      expect(json["big_id"].get<uint64_t>() == 18446744073709551615ULL);

      // neg should be int64_t
      expect(json["neg"].is_int64());
      expect(json["neg"].get<int64_t>() == -100);

      // value should be double
      expect(json["value"].is_double());
      expect(json["value"].get<double>() == 3.14);
   };

   "u64_json_ptr_get_and_get_if"_test = [] {
      glz::generic_u64 json{};
      std::string buffer = R"({"Example":{"enabled":true,"name":"test"}})";
      expect(glz::read_json(json, buffer) == glz::error_code::none);

      auto enabled = glz::get<bool>(json, "/Example/enabled");
      expect(enabled.has_value());
      expect(enabled->get() == true);

      auto name = glz::get<std::string>(json, "/Example/name");
      expect(name.has_value());
      expect(name->get() == "test");

      auto* name_if = glz::get_if<std::string>(json, "/Example/name");
      expect(name_if != nullptr);
      expect(*name_if == "test");
   };

   "u64_as_conversion"_test = [] {
      glz::generic_u64 json{};
      std::string buffer = "12345";
      expect(glz::read_json(json, buffer) == glz::error_code::none);

      // Test as<T>() conversion
      expect(json.as<int>() == 12345);
      expect(json.as<uint64_t>() == 12345ULL);
      expect(json.as<double>() == 12345.0);
   };

   "u64_as_number_helper"_test = [] {
      glz::generic_u64 uint_json{};
      expect(glz::read_json(uint_json, "42") == glz::error_code::none);
      expect(uint_json.as_number() == 42.0);

      glz::generic_u64 int_json{};
      expect(glz::read_json(int_json, "-42") == glz::error_code::none);
      expect(int_json.as_number() == -42.0);

      glz::generic_u64 double_json{};
      expect(glz::read_json(double_json, "3.14") == glz::error_code::none);
      expect(double_json.as_number() == 3.14);
   };

   "u64_assignment_unsigned"_test = [] {
      glz::generic_u64 json{};
      json = 18446744073709551615ULL;
      expect(json.is_uint64());
      expect(json.get<uint64_t>() == 18446744073709551615ULL);

      // Test write
      auto result = json.dump();
      expect(result.has_value());
      expect(result.value() == "18446744073709551615");
   };

   "u64_assignment_signed"_test = [] {
      glz::generic_u64 json{};
      json = -100LL;
      expect(json.is_int64());
      expect(json.get<int64_t>() == -100);
   };

   "u64_roundtrip"_test = [] {
      glz::generic_u64 json = 18446744073709551615ULL; // max uint64_t
      auto json_str = json.dump();
      expect(json_str.has_value());
      expect(json_str.value() == "18446744073709551615");

      glz::generic_u64 json2{};
      expect(glz::read_json(json2, json_str.value()) == glz::error_code::none);
      expect(json2.is_uint64());
      expect(json2.get<uint64_t>() == 18446744073709551615ULL);
   };

   "u64_convert_from_generic_uint64"_test = [] {
      glz::generic_u64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      uint64_t val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42ULL);
   };

   "u64_convert_from_generic_double"_test = [] {
      glz::generic_u64 json{};
      expect(glz::read_json(json, "42") == glz::error_code::none);

      double val;
      auto ec = glz::convert_from_generic(val, json);
      expect(!ec);
      expect(val == 42.0);
   };
};

int main() { return 0; }
