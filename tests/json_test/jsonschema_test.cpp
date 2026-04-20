#include <algorithm>
#include <cstdint>
#include <glaze/core/feature_test.hpp>
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
   glz::expected<glz::schema, glz::error_ctx> obj{glz::read_json<glz::schema>(schema_str)};
};

template <class T>
concept has_variant_value = requires {
   typename T::value_type;
   requires glz::is_variant<typename T::value_type>;
} && requires(T t) {
   { t.has_value() } -> std::convertible_to<bool>;
   { t.value() } -> std::convertible_to<typename T::value_type>;
};

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
   if constexpr (has_variant_value<prop_value_t>) {
      expect[std::holds_alternative<Value>(prop_value.value())];
      expect(std::get<Value>(prop_value.value()) == value);
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

enum struct direction { up, down, left, right };

template <>
struct glz::meta<direction>
{
   using enum direction;
   static constexpr auto value = enumerate(up, down, left, right);
};

template <>
struct glz::json_schema<direction>
{
   schema up{.description = "Move upward"};
   schema down{.description = "Move downward"};
   schema left{.description = "Move left"};
   schema right{.description = "Move right"};
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

// This is a simplified version of the schema struct
// Since we cannot deduce the variant currently when reading when many number formats are available
struct schematic_substitute
{
   using schema_number = std::optional<std::variant<std::int64_t, std::uint64_t, double>>;
   std::optional<std::variant<std::string_view, std::vector<std::string_view>>> type{};
   std::optional<std::vector<schematic_substitute>> oneOf{};
   std::optional<std::string_view> title{};
   std::optional<std::string_view> description{};
   std::optional<std::variant<std::monostate, bool, std::int64_t, std::string_view>> constant{};
   schema_number minimum{};
   schema_number maximum{};
   std::optional<std::uint64_t> minItems{};
   std::optional<std::uint64_t> maxItems{};
   struct glaze
   {
      using T = schematic_substitute;
      static constexpr auto value = glz::object("type", &T::type, //
                                                "const", &T::constant, //
                                                "minimum", &T::minimum, //
                                                "maximum", &T::maximum, //
                                                "oneOf", &T::oneOf, //
                                                "title", &T::title, //
                                                "description", &T::description, //
                                                "minItems", &T::minItems, //
                                                "maxItems", &T::maxItems //
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
         expect(std::get<std::string_view>(*obj.type) == "integer");
      };
      test(one_number{});
      test(const_one_number{});
   };

   "Constexpr number is constant"_test = [] {
      std::string schema_str = glz::write_json_schema<const_one_number>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[obj.constant.has_value()];
      expect[std::holds_alternative<std::int64_t>(obj.constant.value())];
      expect[std::get<std::int64_t>(obj.constant.value()) == const_one_number::some_var];
   };

   "Constexpr enum is constant"_test = [] {
      std::string schema_str = glz::write_json_schema<const_one_enum>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.constant.has_value())];
      expect[(std::holds_alternative<std::string_view>(obj.constant.value()))];
      expect(std::get<std::string_view>(obj.constant.value()) == "green");
   };

   "number has minimum"_test = [] {
      auto test = []<typename number_t>(number_t) {
         std::string schema_str = glz::write_json_schema<number_t>().value_or("error");
         schematic_substitute obj{};
         auto err = read_json_ignore_unknown(obj, schema_str);
         expect(!err) << format_error(err, schema_str);
         expect[(obj.minimum.has_value())];
         expect[(std::holds_alternative<std::int64_t>(obj.minimum.value()))];
         expect(std::get<std::int64_t>(obj.minimum.value()) == std::numeric_limits<number_t>::lowest());
      };
      test(std::int64_t{});
      test(std::uint8_t{});
      // double todo parse_number_failure when reading "minimum":-1.7976931348623157E308
   };

   "always nullable type is constant null"_test = [] {
      std::string schema_str = glz::write_json_schema<std::monostate>().value_or("error");
      // reading null will of course leave the std::optional as empty, therefore check if
      // null is actually written in the schema
      expect(schema_str == R"({"type":"null","title":"std::monostate","const":null})");
   };

   "enum oneOf has title and constant"_test = [] {
      std::string schema_str = glz::write_json_schema<color>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.oneOf.has_value())];
      for (const auto& oneOf : obj.oneOf.value()) {
         expect[(oneOf.title.has_value())];
         expect[(oneOf.constant.has_value())];
         expect[(std::holds_alternative<std::string_view>(oneOf.constant.value()))];
         expect(std::get<std::string_view>(oneOf.constant.value()) == oneOf.title.value());
      }
   };

   "enum value description"_test = [] {
      std::string schema_str = glz::write_json_schema<direction>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.oneOf.has_value())];
      auto& entries = obj.oneOf.value();
      expect(entries.size() == 4);

      std::array<std::string_view, 4> expected_keys{"up", "down", "left", "right"};
      std::array<std::string_view, 4> expected_descs{"Move upward", "Move downward", "Move left", "Move right"};
      for (size_t i = 0; i < 4; ++i) {
         expect[(entries[i].constant.has_value())];
         expect(std::get<std::string_view>(entries[i].constant.value()) == expected_keys[i]);
         expect[(entries[i].title.has_value())];
         expect(entries[i].title.value() == expected_keys[i]);
         expect[(entries[i].description.has_value())];
         expect(entries[i].description.value() == expected_descs[i]);
      }
   };

   "fixed array has fixed size"_test = [] {
      std::string schema_str = glz::write_json_schema<std::array<std::int64_t, 42>>().value_or("error");
      schematic_substitute obj{};
      auto err = read_json_ignore_unknown(obj, schema_str);
      expect(!err) << format_error(err, schema_str);
      expect[(obj.type.has_value())];
      expect(std::get<std::string_view>(*obj.type) == "array");
      // check maxItems (minItems not set because Glaze allows partial reading)
      expect[(!obj.minItems.has_value())];
      expect[(obj.maxItems.has_value())];
      expect(obj.maxItems.value() == 42);
   };

   "required_key meta is correctly used"_test = [] {
      std::string schema_str = glz::write_json_schema<required_meta>().value_or("error");
      expect(
         schema_str ==
         R"({"type":"object","properties":{"a":{"$ref":"#/$defs/int32_t"},"b":{"$ref":"#/$defs/int32_t"},"reserved_1":{"$ref":"#/$defs/int32_t"},"reserved_2":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false,"$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"required":["a","b"],"title":"required_meta"})")
         << schema_str;
   };

   "Opts.error_on_missing_keys as fallback"_test = [] {
      using T = error_on_missing_keys_test;
      std::string schema_str_req =
         glz::write_json_schema<T, glz::opts{.error_on_missing_keys = true}>().value_or("error");
      std::string schema_str_nreq =
         glz::write_json_schema<T, glz::opts{.error_on_missing_keys = false}>().value_or("error");

      // Integer types use $ref to named definitions rather than being inlined.
      // std::optional<T> is canonicalized to anyOf:[ref-to-T, null] and never appears in $defs.
      expect(
         schema_str_req ==
         R"({"type":"object","properties":{"important":{"$ref":"#/$defs/int32_t"},"unimportant":{"anyOf":[{"$ref":"#/$defs/int32_t"},{"type":"null"}]}},"additionalProperties":false,"$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"required":["important"],"title":"error_on_missing_keys_test"})")
         << schema_str_req;

      expect(
         schema_str_nreq ==
         R"({"type":"object","properties":{"important":{"$ref":"#/$defs/int32_t"},"unimportant":{"anyOf":[{"$ref":"#/$defs/int32_t"},{"type":"null"}]}},"additionalProperties":false,"$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"title":"error_on_missing_keys_test"})")
         << schema_str_nreq;
   };

   // Demonstrates using error_on_missing_keys to mark all non-nullable fields as required in JSON schema
   "error_on_missing_keys marks all non-nullable fields as required"_test = [] {
      // Generate schema with all non-nullable fields marked as required
      auto schema = glz::write_json_schema<Person, glz::opts{.error_on_missing_keys = true}>().value_or("error");

      // Parse the schema to verify the required fields
      glz::schema obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema);
      expect(!err) << glz::format_error(err, schema);

      // Verify Person has required fields: name, age, address (but NOT nickname)
      expect(obj.required.has_value());
      auto& req = *obj.required;
      expect(std::find(req.begin(), req.end(), "name") != req.end());
      expect(std::find(req.begin(), req.end(), "age") != req.end());
      expect(std::find(req.begin(), req.end(), "address") != req.end());
      expect(std::find(req.begin(), req.end(), "nickname") == req.end()); // nullable, not required

      // Verify Address (inlined into properties) also has required fields: street, city (but NOT apartment)
      // Address is single-use so it gets inlined. Use generic parse to check nested structure.
      glz::generic_u64 doc{};
      auto err2 = glz::read_json(doc, schema);
      expect(!err2) << "Failed to parse schema as generic";
      auto& root = doc.get<glz::generic_u64::object_t>();
      auto& props = root.at("properties").get<glz::generic_u64::object_t>();
      auto& addr = props.at("address").get<glz::generic_u64::object_t>();
      auto addr_req_it = addr.find("required");
      expect(addr_req_it != addr.end()) << "inlined Address should have required";
      auto& addr_req_arr = addr_req_it->second.get<glz::generic_u64::array_t>();
      auto has_req = [&](const std::string& key) {
         return std::any_of(addr_req_arr.begin(), addr_req_arr.end(),
                            [&](const auto& v) { return v.template get<std::string>() == key; });
      };
      expect(has_req("street"));
      expect(has_req("city"));
      expect(!has_req("apartment")); // nullable, not required
   };
};

struct identifier
{
   std::string value;
};

template <>
struct glz::meta<identifier>
{
   static constexpr auto value{&identifier::value};
};

template <>
struct glz::json_schema<identifier>
{
   schema value{.description = "C++ identifier"};
};

struct cpp_class
{
   identifier name;
};

suite value_type_schema = [] {
   "value_type_json_schema"_test = [] {
      auto s = glz::write_json_schema<cpp_class>().value();
      auto obj = glz::read_json<glz::schema>(s);
      expect(obj.has_value()) << "Failed to parse schema";

      // identifier is single-use so it gets inlined into the property
      // The property should carry the description from json_schema<identifier>
      expect(obj->properties.has_value());
      auto it = obj->properties->find("name");
      expect(it != obj->properties->end());
      expect(it->second.description.has_value());
      expect(*it->second.description == "C++ identifier");
   };
};

struct cpp_class_variant
{
   std::variant<identifier, std::nullopt_t> name;
};

struct point3d
{
   double x{};
   double y{};
   int z{};
};

template <>
struct glz::meta<point3d>
{
   static constexpr std::string_view name = "point3d";
   static constexpr auto value = array(&point3d::x, &point3d::y, &point3d::z);
};

suite value_type_variant_schema = [] {
   "tuple schema uses prefixItems"_test = [] {
      using tuple_t = std::tuple<int, std::string, bool>;
      auto schema = glz::write_json_schema<tuple_t>().value();
      expect(
         schema ==
         R"({"type":"array","prefixItems":[{"type":"integer","minimum":-2147483648,"maximum":2147483647},{"type":"string"},{"type":"boolean"}],"items":false,"title":"std::tuple<int32_t,std::string,bool>","maxItems":3})")
         << schema;
   };

   "single element tuple"_test = [] {
      auto schema = glz::write_json_schema<std::tuple<int>>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema";
      expect(obj->prefixItems.has_value());
      expect(obj->prefixItems->size() == 1);
      expect(obj->items.has_value());
      expect(std::get<bool>(*obj->items) == false);
   };

   "glz::tuple schema"_test = [] {
      auto schema = glz::write_json_schema<glz::tuple<int, std::string>>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema";
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "array");
      expect(obj->prefixItems.has_value());
      expect(obj->prefixItems->size() == 2);
      expect(obj->items.has_value());
      expect(std::get<bool>(*obj->items) == false);
   };

   "nested tuple schema"_test = [] {
      using nested_t = std::tuple<int, std::tuple<double, bool>>;
      auto schema = glz::write_json_schema<nested_t>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema";
      expect(obj->prefixItems.has_value());
      expect(obj->prefixItems->size() == 2);
      // second element should itself be an array with prefixItems
      auto& inner = (*obj->prefixItems)[1];
      expect(inner.type.has_value());
      expect(std::get<std::string_view>(*inner.type) == "array");
      expect(inner.prefixItems.has_value());
      expect(inner.prefixItems->size() == 2);
   };

   "tuple with object type populates defs"_test = [] {
      using tuple_t = std::tuple<int, schema_obj>;
      auto schema = glz::write_json_schema<tuple_t>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema";
      expect(obj->prefixItems.has_value());
      expect(obj->prefixItems->size() == 2);
   };

   "homogeneous array integer items use ref"_test = [] {
      auto schema = glz::write_json_schema<std::vector<int>>().value();
      expect(
         schema ==
         R"({"type":"array","items":{"$ref":"#/$defs/int32_t"},"$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"title":"std::vector<int32_t>"})")
         << schema;
   };

   "vector<string> items inlined"_test = [] {
      auto schema = glz::write_json_schema<std::vector<std::string>>().value();
      expect(schema == R"({"type":"array","items":{"type":"string"},"title":"std::vector<std::string>"})") << schema;
   };

   "vector<bool> items inlined"_test = [] {
      auto schema = glz::write_json_schema<std::vector<bool>>().value();
      expect(schema == R"({"type":"array","items":{"type":"boolean"},"title":"std::vector<bool>"})") << schema;
   };

   "vector<double> items use ref"_test = [] {
      auto schema = glz::write_json_schema<std::vector<double>>().value();
      expect(
         schema ==
         R"({"type":"array","items":{"$ref":"#/$defs/double"},"$defs":{"double":{"type":"number","minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308}},"title":"std::vector<double>"})")
         << schema;
   };

   "map<string,string> additionalProperties inlined"_test = [] {
      auto schema = glz::write_json_schema<std::map<std::string, std::string>>().value();
      expect(
         schema ==
         R"({"type":"object","additionalProperties":{"type":"string"},"title":"std::map<std::string,std::string>"})")
         << schema;
   };

   "glaze_array_t schema"_test = [] {
      auto schema = glz::write_json_schema<point3d>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema";
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "array");
      // point3d has 3 members: double, double, int
      expect(obj->prefixItems.has_value());
      expect(obj->prefixItems->size() == 3);
      expect(obj->items.has_value());
      expect(std::get<bool>(*obj->items) == false);
   };

   "value_type_variant_json_schema"_test = [] {
      auto s = glz::write_json_schema<cpp_class_variant>().value();

      // The variant is single-use so it gets inlined into the property.
      // Parse as generic to verify the variant type array in the inlined property.
      glz::generic_u64 doc{};
      auto err = glz::read_json(doc, s);
      expect(!err) << "Failed to parse schema as generic";
      auto& root = doc.get<glz::generic_u64::object_t>();
      auto& props = root.at("properties").get<glz::generic_u64::object_t>();
      auto& name_prop = props.at("name").get<glz::generic_u64::object_t>();
      auto type_it = name_prop.find("type");
      expect(type_it != name_prop.end()) << "inlined variant should have type";
      auto& type_arr = type_it->second.get<glz::generic_u64::array_t>();
      auto has_type = [&](const std::string& t) {
         return std::any_of(type_arr.begin(), type_arr.end(),
                            [&](const auto& v) { return v.template get<std::string>() == t; });
      };
      expect(has_type("string")) << "missing string type";
      expect(has_type("null")) << "missing null type";
   };
};

suite vector_pair_schema_test = [] {
   "vector_pair_string_int_schema"_test = [] {
      // std::vector<std::pair<std::string, int>> serializes as a JSON object by default (concatenate=true)
      // The schema should reflect this by generating an object type, not an array type
      auto schema = glz::write_json_schema<std::vector<std::pair<std::string, int>>>().value_or("error");
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "failed to parse schema";
      if (!obj) return;
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "object") << "expected object type";
      expect(obj->additionalProperties.has_value()) << "expected additionalProperties";
   };

   "vector_pair_string_uint64_schema"_test = [] {
      auto schema = glz::write_json_schema<std::vector<std::pair<std::string, uint64_t>>>().value_or("error");
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "failed to parse schema";
      if (!obj) return;
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "object") << "expected object type";
   };
};

// Issue #2459: meta value types should produce correct schema
struct holds_bool
{
   bool value;
};

template <>
struct glz::meta<holds_bool>
{
   static constexpr auto value = &holds_bool::value;
};

struct holds_int
{
   int value;
};

template <>
struct glz::meta<holds_int>
{
   static constexpr auto value = &holds_int::value;
};

struct holds_string
{
   std::string value;
};

template <>
struct glz::meta<holds_string>
{
   static constexpr auto value = &holds_string::value;
};

struct holds_optional
{
   std::optional<bool> value;
};

template <>
struct glz::meta<holds_optional>
{
   static constexpr auto value = &holds_optional::value;
};

// glaze_const_value_t: constexpr pointer to static const member
struct const_value_wrapper
{
   static constexpr std::uint64_t const_v{42};
   struct glaze
   {
      static constexpr auto value{&const_value_wrapper::const_v};
   };
};
static_assert(glz::glaze_const_value_t<const_value_wrapper>);

suite meta_value_schema_test = [] {
   "holds_bool schema"_test = [] {
      auto schema = glz::write_json_schema<holds_bool>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "failed to parse schema";
      if (!obj) return;
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "boolean") << schema;
   };

   "holds_int schema"_test = [] {
      auto schema = glz::write_json_schema<holds_int>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "failed to parse schema";
      if (!obj) return;
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "integer") << schema;
   };

   "holds_string schema"_test = [] {
      auto schema = glz::write_json_schema<holds_string>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "failed to parse schema";
      if (!obj) return;
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "string") << schema;
   };

   "holds_optional schema"_test = [] {
      auto schema = glz::write_json_schema<holds_optional>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "failed to parse schema";
      if (!obj) return;
      expect(obj->type.has_value());
      auto& types = std::get<std::vector<std::string_view>>(*obj->type);
      expect(std::find(types.begin(), types.end(), "boolean") != types.end()) << "missing boolean";
      expect(std::find(types.begin(), types.end(), "null") != types.end()) << "missing null";
   };

   "const_value_wrapper schema"_test = [] {
      auto schema = glz::write_json_schema<const_value_wrapper>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "failed to parse schema";
      if (!obj) return;
      expect(obj->type.has_value());
      expect(std::get<std::string_view>(*obj->type) == "integer") << schema;
   };
};

// Issue #2467: void should produce null type, not all types
suite void_schema_test = [] {
   "void schema is null"_test = [] {
      auto schema = glz::write_json_schema<void>().value();
      expect(schema == R"({"type":"null","title":"void"})") << schema;
   };
};

// Issue #2475: variant oneOf should use $ref for complex types instead of duplicating definitions
struct obj_a
{
   int x{};
   double y{};
};

struct obj_b
{
   std::string name{};
};

// Separate types for the tagged variant test so the meta specialization
// doesn't pollute the untagged std::variant<obj_a, obj_b> type.
struct tagged_a
{
   int x{};
};

struct tagged_b
{
   std::string name{};
};

using tagged_obj_variant = std::variant<tagged_a, tagged_b>;
template <>
struct glz::meta<tagged_obj_variant>
{
   static constexpr std::string_view tag = "kind";
};

// Wrapper that uses the same object type both as a property and as a variant alternative
struct shared_type_wrapper
{
   obj_a direct{};
   std::variant<obj_a, obj_b> choice{};
};

suite variant_ref_schema_tests = [] {
   "untagged variant with object alternatives inlined when single-use"_test = [] {
      using var_t = std::variant<obj_a, obj_b>;
      auto schema = glz::write_json_schema<var_t>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema: " << schema;
      if (!obj) return;

      // Single-use alternatives are inlined directly in oneOf
      expect(obj->oneOf.has_value());
      expect(obj->oneOf->size() == 2u);
      for (auto& entry : *obj->oneOf) {
         expect(!entry.ref.has_value()) << "single-use oneOf entry should be inlined";
         expect(entry.type.has_value()) << "inlined oneOf entry should have type";
         expect(entry.properties.has_value()) << "inlined oneOf entry should have properties";
      }
   };

   "untagged variant with array alternative inlined when single-use"_test = [] {
      using var_t = std::variant<std::vector<int>, std::string>;
      auto schema = glz::write_json_schema<var_t>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema: " << schema;
      if (!obj) return;

      expect(obj->oneOf.has_value());
      expect(obj->oneOf->size() == 2u);

      // Both alternatives are single-use and inlined
      auto& vec_entry = (*obj->oneOf)[0];
      expect(!vec_entry.ref.has_value()) << "single-use array alternative should be inlined";
      expect(vec_entry.type.has_value());

      auto& str_entry = (*obj->oneOf)[1];
      expect(!str_entry.ref.has_value()) << "string alternative should be inline";
      expect(str_entry.type.has_value());
   };

   "untagged variant with map alternative inlined when single-use"_test = [] {
      using var_t = std::variant<std::map<std::string, int>, double>;
      auto schema = glz::write_json_schema<var_t>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema: " << schema;
      if (!obj) return;

      expect(obj->oneOf.has_value());
      expect(obj->oneOf->size() == 2u);

      // Both alternatives are single-use and inlined
      auto& map_entry = (*obj->oneOf)[0];
      expect(!map_entry.ref.has_value()) << "single-use map alternative should be inlined";
      expect(map_entry.type.has_value());

      auto& dbl_entry = (*obj->oneOf)[1];
      expect(!dbl_entry.ref.has_value()) << "double alternative should be inline";
   };

   "mixed variant: all single-use types inlined"_test = [] {
      using var_t = std::variant<obj_a, int, std::string, std::vector<double>>;
      auto schema = glz::write_json_schema<var_t>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema: " << schema;
      if (!obj) return;

      expect(obj->oneOf.has_value());
      expect(obj->oneOf->size() == 4u);

      // All alternatives are single-use and inlined
      for (auto& entry : *obj->oneOf) {
         expect(!entry.ref.has_value()) << "single-use alternative should be inlined";
         expect(entry.type.has_value()) << "inlined alternative should have type";
      }
   };

#if !defined(_MSC_VER)
   "tagged variant object alternatives remain inline"_test = [] {
      auto schema = glz::write_json_schema<tagged_obj_variant>().value();
      auto obj = glz::read_json<glz::schema>(schema);
      expect(obj.has_value()) << "Failed to parse schema: " << schema;
      if (!obj) return;

      expect(obj->oneOf.has_value());
      expect(obj->oneOf->size() == 2u);

      // Tagged object alternatives should NOT use $ref — they need inline expansion
      // so the tag discriminator property can be part of the properties map
      for (auto& entry : *obj->oneOf) {
         expect(!entry.ref.has_value()) << "tagged object alternative should not use $ref: " << schema;
         expect(entry.type.has_value()) << "tagged object alternative should have inline type";
         expect(entry.properties.has_value()) << "tagged object alternative should have inline properties";
         // Verify the tag property is present
         expect(entry.properties->count("kind") == 1u) << "tagged alternative should have discriminator property";
      }
   };
#endif

   "shared type between property and variant uses single $defs entry"_test = [] {
      auto schema = glz::write_json_schema<shared_type_wrapper>().value();
      // Use error_on_unknown_keys=false because inlined variant introduces keys (oneOf) not in schema type
      glz::schema obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema);
      expect(!err) << "Failed to parse schema: " << schema;

      // obj_a appears in $defs because it is referenced from both the property and the variant (count=2)
      expect(obj.defs.has_value());
      auto it = obj.defs->find(glz::name_v<obj_a>);
      expect(it != obj.defs->end()) << "obj_a should be in $defs";

      // The direct property should reference obj_a via $ref
      expect(obj.properties.has_value());
      auto prop_it = obj.properties->find("direct");
      expect(prop_it != obj.properties->end());
      expect(prop_it->second.ref.has_value());
      expect(*prop_it->second.ref == std::string("#/$defs/") + std::string(glz::name_v<obj_a>));
   };
};

// Issue #2489: nested std::optional should be idempotent for bool and string
struct nested_optional_obj
{
   std::optional<std::string> b1;
   std::optional<std::optional<std::string>> b2;
   std::optional<std::optional<bool>> b3;
   std::optional<std::optional<std::optional<std::string>>> b4;
};

suite nested_optional_inline_test = [] {
   "nested optional string and bool inlined"_test = [] {
      auto schema = glz::write_json_schema<nested_optional_obj>().value();
      expect(
         schema ==
         R"({"type":"object","properties":{"b1":{"type":["string","null"]},"b2":{"type":["string","null"]},"b3":{"type":["boolean","null"]},"b4":{"type":["string","null"]}},"additionalProperties":false,"title":"nested_optional_obj"})")
         << schema;
   };
};

// Issue #2498: std::optional<T> should never be referenced in $defs.
// Instead, T goes into $defs and the use-site references T via anyOf + null.
namespace issue_2498
{
   struct A
   {};
   struct B
   {
      std::optional<A> aOpt{};
   };
   struct Named
   {
      int x{};
      struct glaze
      {
         static constexpr std::string_view name = "somename";
      };
   };
   struct HoldsNamed
   {
      std::optional<Named> n{};
   };
   struct HoldsOptionalVector
   {
      std::optional<std::vector<A>> v{};
   };
   struct HoldsVectorOfOptional
   {
      std::vector<std::optional<A>> v{};
   };
   struct HoldsMapOfOptional
   {
      std::map<std::string, std::optional<A>> m{};
   };
   struct MultiUse
   {
      A required_a{};
      std::optional<A> optional_a{};
   };
}

// P2996 type-name compiler portability: Bloomberg clang-p2996 returns unqualified names
// from std::meta::display_string_of (e.g., "B"), while GCC 16's reflection returns fully
// qualified names (e.g., "issue_2498::B"). Traditional (non-P2996) reflection also returns
// qualified names. Until qualified_name_of is available in Bloomberg clang-p2996
// (see include/glaze/reflection/get_name.hpp), normalize both sides of the schema
// comparison by stripping the known namespace prefix before comparing. This way a single
// expected string matches all three reflection backends.
static std::string normalize_issue_2498_ns(std::string s)
{
   static constexpr std::string_view prefix = "issue_2498::";
   for (auto pos = s.find(prefix); pos != std::string::npos; pos = s.find(prefix, pos)) {
      s.erase(pos, prefix.size());
   }
   return s;
}

suite optional_never_referenced_test = [] {
   auto eq = [](const std::string& schema, std::string_view expected) {
      return normalize_issue_2498_ns(schema) == normalize_issue_2498_ns(std::string{expected});
   };

   "optional of struct expands inline via anyOf"_test = [eq] {
      auto schema = glz::write_json_schema<issue_2498::B>().value();
      expect(eq(
         schema,
         R"({"type":"object","properties":{"aOpt":{"anyOf":[{"type":"object","properties":{},"additionalProperties":false},{"type":"null"}]}},"additionalProperties":false,"title":"issue_2498::B"})"))
         << schema;
   };

   "optional of named struct still omits std::optional from $defs"_test = [eq] {
      auto schema = glz::write_json_schema<issue_2498::HoldsNamed>().value();
      expect(eq(
         schema,
         R"({"type":"object","properties":{"n":{"anyOf":[{"type":"object","properties":{"x":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false},{"type":"null"}]}},"additionalProperties":false,"$defs":{"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647}},"title":"issue_2498::HoldsNamed"})"))
         << schema;
   };

   "optional wrapping a vector canonicalizes to anyOf"_test = [eq] {
      auto schema = glz::write_json_schema<issue_2498::HoldsOptionalVector>().value();
      expect(eq(
         schema,
         R"({"type":"object","properties":{"v":{"anyOf":[{"type":"array","items":{"type":"object","properties":{},"additionalProperties":false}},{"type":"null"}]}},"additionalProperties":false,"title":"issue_2498::HoldsOptionalVector"})"))
         << schema;
   };

   "array items of nullable type emit anyOf at the item level"_test = [eq] {
      auto schema = glz::write_json_schema<issue_2498::HoldsVectorOfOptional>().value();
      expect(eq(
         schema,
         R"({"type":"object","properties":{"v":{"type":"array","items":{"anyOf":[{"type":"object","properties":{},"additionalProperties":false},{"type":"null"}]}}},"additionalProperties":false,"title":"issue_2498::HoldsVectorOfOptional"})"))
         << schema;
   };

   "map values of nullable type emit anyOf at the value level"_test = [eq] {
      auto schema = glz::write_json_schema<issue_2498::HoldsMapOfOptional>().value();
      expect(eq(
         schema,
         R"({"type":"object","properties":{"m":{"type":"object","additionalProperties":{"anyOf":[{"type":"object","properties":{},"additionalProperties":false},{"type":"null"}]}}},"additionalProperties":false,"title":"issue_2498::HoldsMapOfOptional"})"))
         << schema;
   };

   "multi-use inner type stays in $defs and is referenced via anyOf"_test = [eq] {
      auto schema = glz::write_json_schema<issue_2498::MultiUse>().value();
      expect(eq(
         schema,
         R"({"type":"object","properties":{"optional_a":{"anyOf":[{"$ref":"#/$defs/issue_2498::A"},{"type":"null"}]},"required_a":{"$ref":"#/$defs/issue_2498::A"}},"additionalProperties":false,"$defs":{"issue_2498::A":{"type":"object","properties":{},"additionalProperties":false}},"title":"issue_2498::MultiUse"})"))
         << schema;
   };
};

struct round_trip_inner
{
   int a{};
   std::string b{};
};

struct round_trip_val
{
   double x{};
};

// Verify round-trip correctness for schemas with shared_ptr<schema> items/additionalProperties
suite schema_round_trip_test = [] {
   "array schema round-trips through read/write"_test = [] {
      // Generate a schema with complex items (shared_ptr<schema> path)
      auto schema_str = glz::write_json_schema<std::vector<round_trip_inner>>().value();

      // Read it back
      glz::schema obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema_str);
      expect(!err) << glz::format_error(err, schema_str);

      // Verify structural fields survived the round-trip
      expect(obj.type.has_value());
      expect(std::get<std::string_view>(*obj.type) == "array");
      expect(obj.items.has_value());
      auto* item_ptr = std::get_if<std::shared_ptr<glz::schema>>(&*obj.items);
      expect(item_ptr != nullptr) << "items should be a schema, not bool";
      expect(*item_ptr != nullptr) << "items shared_ptr should not be null";
      expect((*item_ptr)->type.has_value());
      expect(std::get<std::string_view>(*(*item_ptr)->type) == "object");

      // Re-serialize and compare
      std::string re_serialized{};
      auto ec = glz::write<glz::opts{.error_on_unknown_keys = false}>(obj, re_serialized);
      expect(!ec);
      expect(re_serialized == schema_str)
         << "round-trip mismatch:\n  original: " << schema_str << "\n  re-serialized: " << re_serialized;
   };

   "map schema round-trips through read/write"_test = [] {
      auto schema_str = glz::write_json_schema<std::map<std::string, round_trip_val>>().value();

      glz::schema obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema_str);
      expect(!err) << glz::format_error(err, schema_str);

      expect(obj.type.has_value());
      expect(std::get<std::string_view>(*obj.type) == "object");
      expect(obj.additionalProperties.has_value());
      auto* ap_ptr = std::get_if<std::shared_ptr<glz::schema>>(&*obj.additionalProperties);
      expect(ap_ptr != nullptr) << "additionalProperties should be a schema, not bool";
      expect(*ap_ptr != nullptr);

      std::string re_serialized{};
      auto ec = glz::write<glz::opts{.error_on_unknown_keys = false}>(obj, re_serialized);
      expect(!ec);
      expect(re_serialized == schema_str)
         << "round-trip mismatch:\n  original: " << schema_str << "\n  re-serialized: " << re_serialized;
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

// Opt in to primitive-member default extraction for these tests.
struct auto_defaults_opts : glz::opts
{
   bool schema_auto_defaults = true;
};

suite auto_default_tests = [] {
   "default opts emit no auto-extracted defaults"_test = [] {
      // With the default options, primitive member defaults are not extracted.
      std::string schema_str = glz::write_json_schema<auto_defaults>().value_or("error");
      glz::schema obj{};
      auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, schema_str);
      expect(!err) << glz::format_error(err, schema_str);
      expect(obj.properties.has_value());
      auto& props = *obj.properties;
      expect(props.contains("flag"));
      expect(!props.at("flag").defaultValue.has_value()) << schema_str;
      expect(!props.at("count").defaultValue.has_value()) << schema_str;
      expect(!props.at("ratio").defaultValue.has_value()) << schema_str;
   };

   "auto_defaults extracts primitive defaults"_test = [] {
      std::string schema_str = glz::write_json_schema<auto_defaults, auto_defaults_opts{}>().value_or("error");

      // Parse and check defaults
      glz::schema obj{};
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

#if GLZ_HAS_CONSTEXPR_STRING
   "mixed_defaults extracts primitive defaults even with non-trivial members"_test = [] {
      // mixed_defaults contains std::string and std::vector, but with C++20 transient constexpr allocation
      // we can still extract defaults for primitive members
      std::string schema_str = glz::write_json_schema<mixed_defaults, auto_defaults_opts{}>().value_or("error");

      glz::schema obj{};
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
#endif

   "explicit json_schema default overrides struct default"_test = [] {
      std::string schema_str = glz::write_json_schema<explicit_override, auto_defaults_opts{}>().value_or("error");

      glz::schema obj{};
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

   "nested struct defaults work in inlined definition"_test = [] {
      std::string schema_str = glz::write_json_schema<nested_defaults, auto_defaults_opts{}>().value_or("error");

      glz::schema obj{};
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

      // auto_defaults is single-use, so it gets inlined into the inner property.
      // Verify the inlined struct has defaults for its members.
      expect(props.at("inner").properties.has_value());
      auto& inner_props = *props.at("inner").properties;
      expect(inner_props.contains("count"));
      expect(inner_props.at("count").defaultValue.has_value());
      expect(std::get<int64_t>(inner_props.at("count").defaultValue.value()) == 42);
   };
};

int main() { return 0; }
