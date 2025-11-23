// Tests for skip_null_members_on_read option
// This option allows reading JSON with null values without requiring std::optional

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct simple_struct
{
   std::string name = "default";
   int age = 0;
   double score = 0.0;
};

template <>
struct glz::meta<simple_struct>
{
   using T = simple_struct;
   static constexpr auto value = glz::object(&T::name, &T::age, &T::score);
};

// Custom options with skip_null_members_on_read enabled
struct opts_skip_null : glz::opts
{
   bool skip_null_members_on_read = true;
};

suite skip_null_on_read_basic_tests = [] {
   "skip null string field"_test = [] {
      constexpr opts_skip_null opts{};
      simple_struct obj{};
      obj.name = "original";
      obj.age = 25;
      obj.score = 100.0;

      std::string json = R"({"name":null,"age":30,"score":95.5})";
      auto ec = glz::read<opts>(obj, json);

      // Should succeed - null is skipped
      expect(!ec) << "Error: " << glz::format_error(ec, json);

      // name should retain its original value since null was skipped
      expect(obj.name == "original");
      expect(obj.age == 30);
      expect(obj.score == 95.5);
   };

   "skip null int field"_test = [] {
      constexpr opts_skip_null opts{};
      simple_struct obj{};
      obj.name = "test";
      obj.age = 25;
      obj.score = 100.0;

      std::string json = R"({"name":"John","age":null,"score":95.5})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      expect(obj.name == "John");
      expect(obj.age == 25); // Retains original value
      expect(obj.score == 95.5);
   };

   "skip null double field"_test = [] {
      constexpr opts_skip_null opts{};
      simple_struct obj{};
      obj.name = "test";
      obj.age = 25;
      obj.score = 100.0;

      std::string json = R"({"name":"Jane","age":30,"score":null})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      expect(obj.name == "Jane");
      expect(obj.age == 30);
      expect(obj.score == 100.0); // Retains original value
   };

   "skip multiple null fields"_test = [] {
      constexpr opts_skip_null opts{};
      simple_struct obj{};
      obj.name = "original";
      obj.age = 25;
      obj.score = 100.0;

      std::string json = R"({"name":null,"age":null,"score":null})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      // All fields should retain their original values
      expect(obj.name == "original");
      expect(obj.age == 25);
      expect(obj.score == 100.0);
   };

   "mixed null and non-null fields"_test = [] {
      constexpr opts_skip_null opts{};
      simple_struct obj{};
      obj.name = "original";
      obj.age = 25;
      obj.score = 100.0;

      std::string json = R"({"name":"updated","age":null,"score":75.5})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      expect(obj.name == "updated");
      expect(obj.age == 25); // Null was skipped
      expect(obj.score == 75.5);
   };

   "default behavior - option disabled"_test = [] {
      // Default opts has skip_null_members_on_read = false
      simple_struct obj{};
      obj.name = "original";
      obj.age = 25;
      obj.score = 100.0;

      std::string json = R"({"name":null,"age":30,"score":95.5})";
      auto ec = glz::read_json(obj, json);

      // Should error because string doesn't accept null without being optional
      expect(bool(ec)) << "Expected error when reading null into non-nullable type";
   };
};

// Test with optional fields to ensure they still work correctly
struct struct_with_optional
{
   std::string required_name = "default";
   std::optional<int> optional_age = std::nullopt;
   std::optional<std::string> optional_nickname = std::nullopt;
};

template <>
struct glz::meta<struct_with_optional>
{
   using T = struct_with_optional;
   static constexpr auto value = glz::object(&T::required_name, &T::optional_age, &T::optional_nickname);
};

suite skip_null_on_read_with_optional_tests = [] {
   "optional fields with null - option enabled"_test = [] {
      constexpr opts_skip_null opts{};
      struct_with_optional obj{};
      obj.required_name = "original";
      obj.optional_age = 25;
      obj.optional_nickname = "Nick";

      std::string json = R"({"required_name":null,"optional_age":null,"optional_nickname":null})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      // With skip_null_members_on_read, even optional fields should skip null
      expect(obj.required_name == "original");
      expect(obj.optional_age == 25);
      expect(obj.optional_nickname == "Nick");
   };

   "optional fields with null - option disabled"_test = [] {
      struct_with_optional obj{};
      obj.optional_age = 25;
      obj.optional_nickname = "Nick";

      std::string json = R"({"required_name":"test","optional_age":null,"optional_nickname":null})";
      auto ec = glz::read_json(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      // Without skip_null_members_on_read, optional fields are reset on null
      expect(obj.required_name == "test");
      expect(!obj.optional_age.has_value());
      expect(!obj.optional_nickname.has_value());
   };
};

// Test with nested objects
struct nested_inner
{
   int value = 0;
};

template <>
struct glz::meta<nested_inner>
{
   using T = nested_inner;
   static constexpr auto value = glz::object(&T::value);
};

struct nested_outer
{
   std::string name = "default";
   nested_inner inner;
};

template <>
struct glz::meta<nested_outer>
{
   using T = nested_outer;
   static constexpr auto value = glz::object(&T::name, &T::inner);
};

suite skip_null_on_read_nested_tests = [] {
   "skip null in nested object field"_test = [] {
      constexpr opts_skip_null opts{};
      nested_outer obj{};
      obj.name = "test";
      obj.inner.value = 42;

      std::string json = R"({"name":"updated","inner":{"value":null}})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      expect(obj.name == "updated");
      expect(obj.inner.value == 42); // Null was skipped
   };
};

// Test with empty string vs null
suite skip_null_on_read_edge_cases = [] {
   "empty string is not null"_test = [] {
      constexpr opts_skip_null opts{};
      simple_struct obj{};
      obj.name = "original";

      std::string json = R"({"name":"","age":30,"score":95.5})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      expect(obj.name == ""); // Empty string is valid, not null
      expect(obj.age == 30);
      expect(obj.score == 95.5);
   };

   "whitespace around null"_test = [] {
      constexpr opts_skip_null opts{};
      simple_struct obj{};
      obj.name = "original";
      obj.age = 25;

      std::string json = R"({"name": null , "age":30,"score":95.5})";
      auto ec = glz::read<opts>(obj, json);

      expect(!ec) << "Error: " << glz::format_error(ec, json);
      expect(obj.name == "original"); // Null was skipped
      expect(obj.age == 30);
   };
};

int main() {}
