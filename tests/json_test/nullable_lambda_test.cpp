// Tests that custom lambdas returning nullable types (unique_ptr, optional, etc.)
// can be properly skipped when they return null values

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

int main() {}
