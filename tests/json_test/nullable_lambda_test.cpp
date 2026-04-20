// Tests that custom lambdas returning nullable types (unique_ptr, optional, etc.)
// can be properly skipped when they return null values

#include "glaze/beve/read.hpp"
#include "glaze/beve/write.hpp"
#include "glaze/cbor/read.hpp"
#include "glaze/cbor/write.hpp"
#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct my_struct
{
   int i = 287;
   double d = 3.14;
};

template <>
struct glz::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto write_test = [](auto& s) -> std::unique_ptr<std::string> {
      if (287 != s.i)
         return std::make_unique<std::string>("expected: not 287");
      else
         return std::unique_ptr<std::string>();
   };
   static constexpr auto value = glz::object("i", &T::i, "d", &T::d, "test", write_test);
};

suite nullable_lambda_unique_ptr_tests = [] {
   "lambda returns null - should skip field"_test = [] {
      my_struct obj{};
      obj.i = 287; // This will cause lambda to return null
      obj.d = 3.14;

      std::string buffer;

      // Default behavior should skip null members
      expect(not glz::write_json(obj, buffer));

      // The "test" field should be omitted entirely when lambda returns null
      expect(buffer == R"({"i":287,"d":3.14})") << "Got: " << buffer;
   };

   "lambda returns value - should write field"_test = [] {
      my_struct obj{};
      obj.i = 100; // This will cause lambda to return a string
      obj.d = 3.14;

      std::string buffer;
      expect(not glz::write_json(obj, buffer));

      // The "test" field should be present with the error message
      expect(buffer == R"({"i":100,"d":3.14,"test":"expected: not 287"})") << "Got: " << buffer;
   };

   "lambda with skip_null_members false"_test = [] {
      constexpr auto opts = glz::opts{.skip_null_members = false};
      my_struct obj{};
      obj.i = 287; // Lambda returns null
      obj.d = 3.14;

      std::string buffer;
      expect(not glz::write<opts>(obj, buffer));

      // When skip_null_members is false, null should be written explicitly
      expect(buffer == R"({"i":287,"d":3.14,"test":null})") << "Got: " << buffer;
   };
};

// Test with optional return type
struct my_struct_optional
{
   int value = 42;
};

template <>
struct glz::meta<my_struct_optional>
{
   using T = my_struct_optional;
   static constexpr auto get_status = [](auto& s) -> std::optional<std::string> {
      if (s.value > 50)
         return "high";
      else
         return std::nullopt;
   };
   static constexpr auto value = glz::object("value", &T::value, "status", get_status);
};

suite nullable_lambda_optional_tests = [] {
   "lambda returns nullopt - should skip field"_test = [] {
      my_struct_optional obj{};
      obj.value = 42; // Lambda returns nullopt

      std::string buffer;
      expect(not glz::write_json(obj, buffer));

      // The "status" field should be omitted
      expect(buffer == R"({"value":42})") << "Got: " << buffer;
   };

   "lambda returns optional value - should write field"_test = [] {
      my_struct_optional obj{};
      obj.value = 100; // Lambda returns "high"

      std::string buffer;
      expect(not glz::write_json(obj, buffer));

      // The "status" field should be present
      expect(buffer == R"({"value":100,"status":"high"})") << "Got: " << buffer;
   };
};

// Test with glz::custom and optional return type (GitHub issue #2386)
struct my_struct_custom_optional
{
   int value = 42;
};

template <>
struct glz::meta<my_struct_custom_optional>
{
   using T = my_struct_custom_optional;
   static constexpr auto set_status = [](T& s, std::optional<std::string> val) {
      if (val.has_value()) {
         s.value = 50;
      }
      else {
         s.value = 0;
      }
   };
   static constexpr auto get_status = [](auto& s) -> std::optional<std::string> {
      if (s.value > 50) {
         return "high";
      }
      else {
         return std::nullopt;
      }
   };
   static constexpr auto value = glz::object("value", &T::value, "status", glz::custom<set_status, get_status>);
};

suite custom_nullable_optional_tests = [] {
   "custom getter returns nullopt - should skip field"_test = [] {
      my_struct_custom_optional obj{};
      obj.value = 42; // getter returns nullopt

      std::string buffer;
      expect(not glz::write_json(obj, buffer));

      // The "status" field should be omitted
      expect(buffer == R"({"value":42})") << "Got: " << buffer;
   };

   "custom getter returns optional value - should write field"_test = [] {
      my_struct_custom_optional obj{};
      obj.value = 100; // getter returns "high"

      std::string buffer;
      expect(not glz::write_json(obj, buffer));

      // The "status" field should be present
      expect(buffer == R"({"value":100,"status":"high"})") << "Got: " << buffer;
   };

   "json write then read round-trip with value"_test = [] {
      my_struct_custom_optional src{};
      src.value = 100; // getter returns "high"

      std::string buffer;
      expect(not glz::write_json(src, buffer));

      my_struct_custom_optional dst{};
      expect(not glz::read_json(dst, buffer));

      // setter receives "high" -> sets value to 50
      expect(dst.value == 50) << "Got: " << dst.value;
   };

   "json read with field absent - should not error"_test = [] {
      my_struct_custom_optional dst{};
      dst.value = 99;

      // JSON with "status" omitted — the setter should not be called
      expect(not glz::read_json(dst, R"({"value":42})"));
      expect(dst.value == 42) << "Got: " << dst.value;
   };

   "custom getter with skip_null_members false"_test = [] {
      constexpr auto opts = glz::opts{.skip_null_members = false};
      my_struct_custom_optional obj{};
      obj.value = 42; // getter returns nullopt

      std::string buffer;
      expect(not glz::write<opts>(obj, buffer));

      // When skip_null_members is false, null should be written explicitly
      expect(buffer == R"({"value":42,"status":null})") << "Got: " << buffer;
   };
};

// CBOR round-trip tests for custom nullable getter
suite custom_nullable_cbor_tests = [] {
   "cbor custom getter nullopt - round trip"_test = [] {
      my_struct_custom_optional src{};
      src.value = 42; // getter returns nullopt

      std::string buffer;
      expect(not glz::write_cbor(src, buffer));

      my_struct_custom_optional dst{};
      dst.value = 99;
      expect(not glz::read_cbor(dst, buffer));

      // value should survive round-trip; status was omitted (nullopt)
      expect(dst.value == 42);
   };

   "cbor custom getter with value - round trip"_test = [] {
      my_struct_custom_optional src{};
      src.value = 100; // getter returns "high"

      std::string buffer;
      expect(not glz::write_cbor(src, buffer));

      my_struct_custom_optional dst{};
      expect(not glz::read_cbor(dst, buffer));

      // setter receives "high" -> sets value to 50
      expect(dst.value == 50);
   };
};

// BEVE round-trip tests for custom nullable getter
suite custom_nullable_beve_tests = [] {
   "beve custom getter nullopt - round trip"_test = [] {
      my_struct_custom_optional src{};
      src.value = 42; // getter returns nullopt

      std::string buffer;
      expect(not glz::write_beve(src, buffer));

      my_struct_custom_optional dst{};
      dst.value = 99;
      expect(not glz::read_beve(dst, buffer));

      expect(dst.value == 42);
   };

   "beve custom getter with value - round trip"_test = [] {
      my_struct_custom_optional src{};
      src.value = 100; // getter returns "high"

      std::string buffer;
      expect(not glz::write_beve(src, buffer));

      my_struct_custom_optional dst{};
      expect(not glz::read_beve(dst, buffer));

      expect(dst.value == 50);
   };
};

int main() {}
