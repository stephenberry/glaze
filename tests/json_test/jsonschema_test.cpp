#include <boost/ut.hpp>
#include <cstdint>
#include <glaze/json.hpp>
#include <glaze/json/schema.hpp>
#include <string>

using namespace boost::ut;

struct schema_obj
{
   std::int64_t variable{2};
};

// static constexpr auto example_arr = std::array<std::string_view, 1>{ "\"this is an example\"" };

template <>
struct glz::json_schema<schema_obj>
{
   schema variable{
      .description = "this is a description",
      // .defaultValue = 42L, // todo it is not currently supported to read glz::schema::schema_any, for reference see
      // function variant_is_auto_deducible
      .deprecated = true,
      .readOnly = true,
      .writeOnly = true,
      .constant = "some constant value",
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
      .maxProperties = std::numeric_limits<std::uint64_t>::max(),
      // .required = , // read of std::span is not supported
      .minItems = 1UL,
      .maxItems = 2UL,
      .minContains = 1UL,
      .maxContains = 2UL,
      .uniqueItems = true,
      // .enumeration = , // read of std::span is not supported
      .extUnits = detail::ExtUnits{.unitAscii = "m^2", .unitUnicode = "m²"},
      .extAdvanced = true,
   };
   // schema examples{.examples = example_arr};
};

struct test_case
{
   std::string schema_str = glz::write_json_schema<schema_obj>();
   glz::expected<glz::detail::schematic, glz::parse_error> obj{glz::read_json<glz::detail::schematic>(schema_str)};
};

template <class T>
concept is_optional = glz::is_specialization_v<T, std::optional>;

template <auto Member, typename Value>
auto expect_property(test_case test, std::string_view key, Value value)
{
   auto schematic = test.obj;
   expect(fatal(schematic.has_value()));
   expect(schematic->properties->contains(key) >> fatal);
   glz::schema prop = schematic->properties->at(key);
   auto prop_value = glz::detail::get_member(prop, Member);
   expect(prop_value.has_value() >> fatal);
   using prop_value_t = std::decay_t<decltype(prop_value)>;
   if constexpr (is_optional<prop_value_t> && glz::is_variant<typename prop_value_t::value_type>) {
      expect(std::holds_alternative<Value>(prop_value.value()) >> fatal);
      expect(std::get<Value>(prop_value.value()) == value);
   }
   else if constexpr (is_optional<prop_value_t> && glz::detail::is_span<typename prop_value_t::value_type>) {
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
      test_case const test{};
      expect(test.obj.has_value()) << format_error(!test.obj.has_value() ? test.obj.error() : glz::parse_error{},
                                                   test.schema_str);
      expect(fatal(test.obj.has_value()));
   };
   "description"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::description>(test, "variable", std::string{"this is a description"});
   };
   "deprecated"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::deprecated>(test, "variable", true);
   };
   // skip / "examples"_test = [] {
   //    test_case const test{};
   //    expect_property<&glz::schema::examples>(test, "examples", std::span{ example_arr });
   // };
   "readOnly"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::readOnly>(test, "variable", true);
   };
   "writeOnly"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::writeOnly>(test, "variable", true);
   };
   "constant"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::constant>(test, "variable", std::string_view{"some constant value"});
   };
   "minLength"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::minLength>(test, "variable", std::uint64_t{1});
   };
   "maxLength"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::maxLength>(test, "variable", std::uint64_t{2});
   };
   "pattern"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::pattern>(test, "variable", std::string_view{"[a-z]+"});
   };
   "format"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::format>(test, "variable", glz::detail::defined_formats::hostname);
   };
   "minimum"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::minimum>(test, "variable", std::int64_t{1});
   };
   "maximum"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::maximum>(test, "variable", std::int64_t{2});
   };
   "exclusiveMinimum"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::exclusiveMinimum>(test, "variable", std::int64_t{1});
   };
   "exclusiveMaximum"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::exclusiveMaximum>(test, "variable", std::int64_t{2});
   };
   "multipleOf"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::multipleOf>(test, "variable", std::int64_t{3});
   };
   "minProperties"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::minProperties>(test, "variable", std::uint64_t{4});
   };
   "maxProperties"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::maxProperties>(test, "variable", std::numeric_limits<std::uint64_t>::max());
   };
   "minItems"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::minItems>(test, "variable", std::uint64_t{1});
   };
   "maxItems"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::maxItems>(test, "variable", std::uint64_t{2});
   };
   "minContains"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::minContains>(test, "variable", std::uint64_t{1});
   };
   "maxContains"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::maxContains>(test, "variable", std::uint64_t{2});
   };
   "uniqueItems"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::uniqueItems>(test, "variable", true);
   };
   "extUnits"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::extUnits>(test, "variable",
                                              glz::detail::ExtUnits{.unitAscii = "m^2", .unitUnicode = "m²"});
   };
   "extAdvanced"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::extAdvanced>(test, "variable", true);
   };
};

int main() { return boost::ut::cfg<>.run({.report_errors = true}); }
