#include <algorithm>
#include <cstdint>
#include <glaze/json.hpp>
#include <glaze/json/schema.hpp>
#include <string>
#include <ut/ut.hpp>

using namespace ut;

struct schema_obj
{
   std::int64_t variable{2};
};

template <>
struct glz::json_schema<schema_obj>
{
   schema variable{
      .description = "this is a description",
      // .defaultValue = 42L, // todo it is not currently supported to read glz::schema::schema_any, for reference see
      // function variant_is_auto_deducible
      .deprecated = true,
      .examples = std::vector<std::string_view>{R"("foo")", R"("bar")"},
      .readOnly = true,
      .writeOnly = true,
      // .constant = "some constant value", // todo it is not currently supported to read glz::schema::schema_any, for
      // reference see function variant_is_auto_deducible
      .minLength = 1L,
      .maxLength = 2L,
      .pattern = "[a-z]+",
      .format = detail::defined_formats::hostname,
      .minimum = 1L,
      .maximum = 2L,
      .exclusiveMinimum = 1L,
      .exclusiveMaximum = 2L,
      .multipleOf = 3L,
      .minProperties = 4UL,
      .maxProperties = (std::numeric_limits<std::uint64_t>::max)(),
      // .required = , // read of std::span is not supported
      .minItems = 1UL,
      .maxItems = 2UL,
      .minContains = 1UL,
      .maxContains = 2UL,
      .uniqueItems = true,
      // .enumeration = , // read of std::span is not supported
      .ExtUnits = detail::ExtUnits{.unitAscii = "m^2", .unitUnicode = "m²"},
      .ExtAdvanced = true,
   };
};

struct test_case
{
   std::string schema_str = glz::write_json_schema<schema_obj>().value_or("error");
   glz::expected<glz::detail::schematic, glz::error_ctx> obj{glz::read_json<glz::detail::schematic>(schema_str)};
};

template <class T>
concept is_optional = glz::is_specialization_v<T, std::optional>;

template <auto Member, typename Value>
auto expect_property(const test_case& test, std::string_view key, Value value)
{
   auto schematic = test.obj;
   expect[schematic.has_value()];
   expect[schematic->properties->contains(key)];
   const glz::schema& prop = schematic->properties->at(key);
   auto prop_value = glz::get_member(prop, Member);
   expect[prop_value.has_value()];
   using prop_value_t = std::decay_t<decltype(prop_value)>;
   if constexpr (is_optional<prop_value_t> && glz::is_variant<typename prop_value_t::value_type>) {
      expect[std::holds_alternative<Value>(prop_value.value())];
      expect(std::get<Value>(prop_value.value()) == value);
   }
   else if constexpr (is_optional<prop_value_t> && glz::is_span<typename prop_value_t::value_type>) {
      expect(fatal(prop_value.value().size() == value.size()));
      for (std::size_t i = 0; i < prop_value.value().size(); ++i) {
         expect(prop_value.value()[i] == value[i]);
      }
   }
   else {
      expect(prop_value.value() == value);
   }
}

suite schema_attributes = [] {
   "parsing"_test = [] {
      const test_case test{};
      expect(test.obj.has_value()) << format_error(!test.obj.has_value() ? test.obj.error() : glz::error_ctx{},
                                                   test.schema_str);
      expect[test.obj.has_value()];
   };
   "description"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::description>(test, "variable", std::string{"this is a description"});
   };
   "deprecated"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::deprecated>(test, "variable", true);
   };
   "readOnly"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::readOnly>(test, "variable", true);
   };
   "writeOnly"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::writeOnly>(test, "variable", true);
   };
   "minLength"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::minLength>(test, "variable", std::uint64_t{1});
   };
   "maxLength"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::maxLength>(test, "variable", std::uint64_t{2});
   };
   "pattern"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::pattern>(test, "variable", std::string_view{"[a-z]+"});
   };
   "format"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::format>(test, "variable", glz::detail::defined_formats::hostname);
   };
   "minimum"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::minimum>(test, "variable", std::int64_t{1});
   };
   "maximum"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::maximum>(test, "variable", std::int64_t{2});
   };
   "exclusiveMinimum"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::exclusiveMinimum>(test, "variable", std::int64_t{1});
   };
   "exclusiveMaximum"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::exclusiveMaximum>(test, "variable", std::int64_t{2});
   };
   "multipleOf"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::multipleOf>(test, "variable", std::int64_t{3});
   };
   "minProperties"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::minProperties>(test, "variable", std::uint64_t{4});
   };
   "maxProperties"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::maxProperties>(test, "variable", (std::numeric_limits<std::uint64_t>::max)());
   };
   "minItems"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::minItems>(test, "variable", std::uint64_t{1});
   };
   "maxItems"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::maxItems>(test, "variable", std::uint64_t{2});
   };
   "minContains"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::minContains>(test, "variable", std::uint64_t{1});
   };
   "maxContains"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::maxContains>(test, "variable", std::uint64_t{2});
   };
   "uniqueItems"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::uniqueItems>(test, "variable", true);
   };
   "extUnits"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::ExtUnits>(test, "variable",
                                              glz::detail::ExtUnits{.unitAscii = "m^2", .unitUnicode = "m²"});
   };
   "extAdvanced"_test = [] {
      const test_case test{};
      expect_property<&glz::schema::ExtAdvanced>(test, "variable", true);
   };
};

struct one_number
{
   std::int64_t some_var{42};
};

template <>
struct glz::meta<one_number>
{
   static constexpr auto value = &one_number::some_var;
};

struct const_one_number
{
   static constexpr std::int64_t some_var{1337};
};

template <>
struct glz::meta<const_one_number>
{
   static constexpr auto value = &const_one_number::some_var;
};

enum struct color : std::uint8_t { red = 0, green, blue };

template <>
struct glz::meta<color>
{
   using enum color;
   static constexpr auto value = enumerate("red", red, //
                                           "green", green, //
                                           "blue", blue //
   );
};

struct const_one_enum
{
   static constexpr color some_var{color::green};
};

template <>
struct glz::meta<const_one_enum>
{
   static constexpr auto value = &const_one_enum::some_var;
};

// This is a simplified version of the schematic struct
// Since we cannot deduce the variant currently when reading when many number formats are available
struct schematic_substitute
{
   using schema_number = std::optional<std::variant<std::int64_t, std::uint64_t, double>>;
   std::optional<std::vector<std::string_view>> type{};
   std::optional<std::vector<schematic_substitute>> oneOf{};
   struct schema
   {
      std::optional<std::string_view> title{};
      std::optional<std::string_view> description{};
      std::optional<std::variant<std::monostate, bool, std::int64_t, std::string_view>> constant{};
      schema_number minimum{};
      schema_number maximum{};
      std::optional<std::uint64_t> minItems{};
      std::optional<std::uint64_t> maxItems{};
   };
   schema attributes{};
   struct glaze
   {
      using T = schematic_substitute;
      static constexpr auto value = glz::object(
         "type", &T::type, //
         "const", [](auto&& self) -> auto& { return self.attributes.constant; }, //
         "minimum", [](auto&& self) -> auto& { return self.attributes.minimum; }, //
         "maximum", [](auto&& self) -> auto& { return self.attributes.maximum; }, //
         "oneOf", &T::oneOf, //
         "title", [](auto&& self) -> auto& { return self.attributes.title; }, //
         "description", [](auto&& self) -> auto& { return self.attributes.description; }, //
         "minItems", [](auto&& self) -> auto& { return self.attributes.minItems; }, //
         "maxItems", [](auto&& self) -> auto& { return self.attributes.maxItems; } //
      );
   };
};

auto read_json_ignore_unknown(auto&& value, auto&& buffer)
{
   glz::context ctx{};
   return read<glz::opts{.error_on_unknown_keys = false}>(value, std::forward<decltype(buffer)>(buffer), ctx);
}

struct required_meta
{
   int a{};
   int reserved_1{};
   int reserved_2{};
   int b{};
};

template <>
struct glz::meta<required_meta>
{
   static constexpr bool requires_key(const std::string_view key, const bool is_nullable)
   {
      if (key.starts_with("reserved")) {
         return false;
      }
      return !is_nullable;
   }
};

struct error_on_missing_keys_test
{
   std::optional<int> unimportant{};
   int important{};
};

// Test types for demonstrating error_on_missing_keys with nested objects
struct Address
{
   std::string street{};
   std::string city{};
   std::optional<std::string> apartment{}; // nullable - will NOT be required
};

struct Person
{
   std::string name{};
   int age{};
   std::optional<std::string> nickname{}; // nullable - will NOT be required
   Address address{};
};

#if 0
struct invalid_name
{
   int a{};
};
template <> struct glz::meta<invalid_name>
{
   static constexpr auto name = "invalid/name"; // slashes are not allowed in json schema
};
struct container_of_invalid_name
{
   invalid_name foo{};
};
suite compile_errors = [] {
   "static_assert slash in name"_test = [] {
      std::string schema_str = glz::write_json_schema<container_of_invalid_name>();
   };
};
#endif

suite schema_tests = [] {
   "typeof directly accessed integer should only be integer"_test = [] {
      auto test = []<typename T>(T) {
         std::string schema_str = glz::write_json_schema<T>().value_or("error");
         schematic_substitute obj{};
         auto err = read_json_ignore_unknown(obj, schema_str);
         expect(!err) << format_error(err, schema_str);
         expect(obj.type->size() == 1);
         expect(obj.type->at(0) == "integer");
      };
      test(one_number{});
      test(const_one_number{});
   };

   "Constexpr number is constant"_test = [] {
      std::string schema_str = glz::write_json_schema<const_one_number>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[obj.attributes.constant.has_value()];
      expect[std::holds_alternative<std::int64_t>(obj.attributes.constant.value())];
      expect[std::get<std::int64_t>(obj.attributes.constant.value()) == const_one_number::some_var];
   };

   "Constexpr enum is constant"_test = [] {
      std::string schema_str = glz::write_json_schema<const_one_enum>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.attributes.constant.has_value())];
      expect[(std::holds_alternative<std::string_view>(obj.attributes.constant.value()))];
      expect(std::get<std::string_view>(obj.attributes.constant.value()) == "green");
   };

   "number has minimum"_test = [] {
      auto test = []<typename number_t>(number_t) {
         std::string schema_str = glz::write_json_schema<number_t>().value_or("error");
         schematic_substitute obj{};
         auto err = read_json_ignore_unknown(obj, schema_str);
         expect(!err) << format_error(err, schema_str);
         expect[(obj.attributes.minimum.has_value())];
         expect[(std::holds_alternative<std::int64_t>(obj.attributes.minimum.value()))];
         expect(std::get<std::int64_t>(obj.attributes.minimum.value()) == std::numeric_limits<number_t>::lowest());
      };
      test(std::int64_t{});
      test(std::uint8_t{});
      // double todo parse_number_failure when reading "minimum":-1.7976931348623157E308
   };

   "always nullable type is constant null"_test = [] {
      std::string schema_str = glz::write_json_schema<std::monostate>().value_or("error");
      // reading null will of course leave the std::optional as empty, therefore check if
      // null is actually written in the schema
      expect(schema_str == R"({"type":["null"],"$defs":{},"title":"std::monostate","const":null})");
   };

   "enum oneOf has title and constant"_test = [] {
      std::string schema_str = glz::write_json_schema<color>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.oneOf.has_value())];
      for (const auto& oneOf : obj.oneOf.value()) {
         expect[(oneOf.attributes.title.has_value())];
         expect[(oneOf.attributes.constant.has_value())];
         expect[(std::holds_alternative<std::string_view>(oneOf.attributes.constant.value()))];
         expect(std::get<std::string_view>(oneOf.attributes.constant.value()) == oneOf.attributes.title.value());
      }
   };

   "enum description"_test = [] {
      std::string schema_str = glz::write_json_schema<color>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.oneOf.has_value())];
   };

   "fixed array has fixed size"_test = [] {
      std::string schema_str = glz::write_json_schema<std::array<std::int64_t, 42>>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.type.has_value())];
      expect(obj.type->size() == 1);
      expect(obj.type->at(0) == "array");
      // check minItems and maxItems
      expect[(obj.attributes.minItems.has_value())];
      expect(obj.attributes.minItems.value() == 42);
      expect[(obj.attributes.maxItems.has_value())];
      expect(obj.attributes.maxItems.value() == 42);
   };

   "required_key meta is correctly used"_test = [] {
      std::string schema_str = glz::write_json_schema<required_meta>().value_or("error");
      expect(
         schema_str ==
         R"({"type":["object"],"properties":{"a":{"$ref":"#/$defs/int32_t","default":0},"b":{"$ref":"#/$defs/int32_t","default":0},"reserved_1":{"$ref":"#/$defs/int32_t","default":0},"reserved_2":{"$ref":"#/$defs/int32_t","default":0}},"additionalProperties":false,"$defs":{"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647}},"required":["a","b"],"title":"required_meta"})");
   };

   "Opts.error_on_missing_keys as fallback"_test = [] {
      using T = error_on_missing_keys_test;
      std::string schema_str_req =
         glz::write_json_schema<T, glz::opts{.error_on_missing_keys = true}>().value_or("error");
      std::string schema_str_nreq =
         glz::write_json_schema<T, glz::opts{.error_on_missing_keys = false}>().value_or("error");

      expect(
         schema_str_req ==
         R"({"type":["object"],"properties":{"important":{"$ref":"#/$defs/int32_t","default":0},"unimportant":{"$ref":"#/$defs/std::optional<int32_t>"}},"additionalProperties":false,"$defs":{"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::optional<int32_t>":{"type":["integer","null"],"minimum":-2147483648,"maximum":2147483647}},"required":["important"],"title":"error_on_missing_keys_test"})");

      expect(
         schema_str_nreq ==
         R"({"type":["object"],"properties":{"important":{"$ref":"#/$defs/int32_t","default":0},"unimportant":{"$ref":"#/$defs/std::optional<int32_t>"}},"additionalProperties":false,"$defs":{"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::optional<int32_t>":{"type":["integer","null"],"minimum":-2147483648,"maximum":2147483647}},"title":"error_on_missing_keys_test"})");
   };

   // Demonstrates using error_on_missing_keys to mark all non-nullable fields as required in JSON schema
   "error_on_missing_keys marks all non-nullable fields as required"_test = [] {
      // Generate schema with all non-nullable fields marked as required
      auto schema = glz::write_json_schema<Person, glz::opts{.error_on_missing_keys = true}>().value_or("error");

      // Parse the schema to verify the required fields
      glz::detail::schematic obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema);
      expect(!err) << glz::format_error(err, schema);

      // Verify Person has required fields: name, age, address (but NOT nickname)
      expect(obj.required.has_value());
      auto& req = *obj.required;
      expect(std::find(req.begin(), req.end(), "name") != req.end());
      expect(std::find(req.begin(), req.end(), "age") != req.end());
      expect(std::find(req.begin(), req.end(), "address") != req.end());
      expect(std::find(req.begin(), req.end(), "nickname") == req.end()); // nullable, not required

      // Verify Address definition also has required fields: street, city (but NOT apartment)
      expect(obj.defs.has_value());
      auto address_it = obj.defs->find("Address");
      expect(address_it != obj.defs->end());
      expect(address_it->second.required.has_value());
      auto& addr_req = *address_it->second.required;
      expect(std::find(addr_req.begin(), addr_req.end(), "street") != addr_req.end());
      expect(std::find(addr_req.begin(), addr_req.end(), "city") != addr_req.end());
      expect(std::find(addr_req.begin(), addr_req.end(), "apartment") == addr_req.end()); // nullable, not required
   };
};

// Test structs for automatic default value extraction (Issue #1296)
struct auto_defaults
{
   bool flag{true};
   int32_t count{42};
   double ratio{3.14};
   uint64_t big{1000};
   int8_t small{-5};
};

struct mixed_defaults
{
   int with_default{100}; // primitives CAN be auto-extracted
   std::string no_schema_default{"hello"}; // strings not in schema_any, so can't be auto-extracted
   std::vector<int> container{1, 2, 3}; // complex types not in schema_any, so can't be auto-extracted
};

struct explicit_override
{
   int value{42};
};

template <>
struct glz::json_schema<explicit_override>
{
   schema value{
      .defaultValue = 99L, // explicit default should override the 42 from struct
   };
};

struct nested_defaults
{
   int outer{10};
   auto_defaults inner{}; // nested struct with defaults
};

suite auto_default_tests = [] {
   "auto_defaults extracts primitive defaults"_test = [] {
      std::string schema_str = glz::write_json_schema<auto_defaults>().value_or("error");

      // Parse and check defaults
      glz::detail::schematic obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema_str);
      expect(!err) << glz::format_error(err, schema_str);

      expect(obj.properties.has_value());
      auto& props = *obj.properties;

      // Check bool default
      expect(props.contains("flag"));
      expect(props.at("flag").defaultValue.has_value());
      expect(std::holds_alternative<bool>(props.at("flag").defaultValue.value()));
      expect(std::get<bool>(props.at("flag").defaultValue.value()) == true);

      // Check int32_t default
      expect(props.contains("count"));
      expect(props.at("count").defaultValue.has_value());
      expect(std::holds_alternative<int64_t>(props.at("count").defaultValue.value()));
      expect(std::get<int64_t>(props.at("count").defaultValue.value()) == 42);

      // Check double default
      expect(props.contains("ratio"));
      expect(props.at("ratio").defaultValue.has_value());
      expect(std::holds_alternative<double>(props.at("ratio").defaultValue.value()));
      expect(std::get<double>(props.at("ratio").defaultValue.value()) == 3.14);

      // Check uint64_t default (note: when read back from JSON, positive integers parse as int64_t)
      expect(props.contains("big"));
      expect(props.at("big").defaultValue.has_value());
      // JSON parsing reads positive integers as int64_t, not uint64_t
      expect(std::holds_alternative<int64_t>(props.at("big").defaultValue.value()));
      expect(std::get<int64_t>(props.at("big").defaultValue.value()) == 1000);

      // Check int8_t default (should be converted to int64_t)
      expect(props.contains("small"));
      expect(props.at("small").defaultValue.has_value());
      expect(std::holds_alternative<int64_t>(props.at("small").defaultValue.value()));
      expect(std::get<int64_t>(props.at("small").defaultValue.value()) == -5);
   };

   "mixed_defaults extracts primitive defaults even with non-trivial members"_test = [] {
      // mixed_defaults contains std::string and std::vector, but with C++20 transient constexpr allocation
      // we can still extract defaults for primitive members
      std::string schema_str = glz::write_json_schema<mixed_defaults>().value_or("error");

      glz::detail::schematic obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema_str);
      expect(!err) << glz::format_error(err, schema_str);

      expect(obj.properties.has_value());
      auto& props = *obj.properties;

      // Primitive types CAN have defaults extracted even when struct contains non-trivial types
      expect(props.contains("with_default"));
      expect(props.at("with_default").defaultValue.has_value());
      expect(std::holds_alternative<int64_t>(props.at("with_default").defaultValue.value()));
      expect(std::get<int64_t>(props.at("with_default").defaultValue.value()) == 100);

      // String defaults cannot be extracted (not supported by schema_any)
      expect(props.contains("no_schema_default"));
      expect(!props.at("no_schema_default").defaultValue.has_value());

      // Vector defaults cannot be extracted (not supported by schema_any)
      expect(props.contains("container"));
      expect(!props.at("container").defaultValue.has_value());
   };

   "explicit json_schema default overrides struct default"_test = [] {
      std::string schema_str = glz::write_json_schema<explicit_override>().value_or("error");

      glz::detail::schematic obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema_str);
      expect(!err) << glz::format_error(err, schema_str);

      expect(obj.properties.has_value());
      auto& props = *obj.properties;

      // Explicit schema default (99) should override struct default (42)
      expect(props.contains("value"));
      expect(props.at("value").defaultValue.has_value());
      expect(std::holds_alternative<int64_t>(props.at("value").defaultValue.value()));
      expect(std::get<int64_t>(props.at("value").defaultValue.value()) == 99);
   };

   "nested struct defaults work in $defs"_test = [] {
      std::string schema_str = glz::write_json_schema<nested_defaults>().value_or("error");

      glz::detail::schematic obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema_str);
      expect(!err) << glz::format_error(err, schema_str);

      expect(obj.properties.has_value());
      auto& props = *obj.properties;

      // outer field should have default
      expect(props.contains("outer"));
      expect(props.at("outer").defaultValue.has_value());
      expect(std::get<int64_t>(props.at("outer").defaultValue.value()) == 10);

      // inner field (complex type) should not have default on the property itself
      expect(props.contains("inner"));
      expect(!props.at("inner").defaultValue.has_value());

      // But the auto_defaults $def should have defaults for its members
      expect(obj.defs.has_value());
      auto it = obj.defs->find("auto_defaults");
      expect(it != obj.defs->end());
      expect(it->second.properties.has_value());
      auto& inner_props = *it->second.properties;
      expect(inner_props.contains("count"));
      expect(inner_props.at("count").defaultValue.has_value());
      expect(std::get<int64_t>(inner_props.at("count").defaultValue.value()) == 42);
   };
};

int main() { return 0; }
