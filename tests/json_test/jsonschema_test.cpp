#include <cstdint>
#include <string>

#include <boost/ut.hpp>
#include <glaze/json.hpp>
#include <glaze/json/schema.hpp>

using namespace boost::ut;

struct schema_obj
{
   std::string_view desc{"this variable has a description"};
   std::int64_t min1{2};
   std::int64_t max2{5};
   // todo it is not currently supported to read glz::schema::schema_any, for reference see function variant_is_auto_deducible
   // std::int64_t default_val{1337};
   std::string pattern{"[a-z]+"};
   bool is_deprecated{};
   std::string_view examples{"this is an example"};
};

static constexpr auto example_arr = std::array<std::string_view, 1>{ "this is an example" };

template <>
struct glz::json_schema<schema_obj>
{
   schema desc{.description = "this is a description"};
   schema min1{.minimum = 1L};
   schema max2{.maximum = 2L};
   // todo it is not currently supported to read glz::schema::schema_any, for reference see function variant_is_auto_deducible
   // schema default_val{.defaultValue = 42L};
   schema pattern{.pattern = "[a-z]+"};
   schema is_deprecated{.deprecated = true};
   schema examples{.examples = example_arr};
};

struct test_case
{
   std::string schema_str = glz::write_json_schema<schema_obj>();
   glz::expected<glz::detail::schematic, glz::parse_error> obj{glz::read_json<glz::detail::schematic>(schema_str)};
};

template <auto Member, typename Value>
auto expect_property(test_case test, std::string_view key, Value value) {
   auto schematic = test.obj;
   expect(fatal(schematic.has_value())) << "hello world";
   expect(schematic->properties->contains(key) >> fatal);
   glz::schema prop = schematic->properties->at(key);
   auto prop_value = glz::detail::get_member(prop, Member);
   expect(prop_value.has_value() >> fatal);
   using prop_value_t = std::decay_t<decltype(prop_value)>;
   if constexpr (std::same_as<prop_value_t, glz::schema::schema_number>) {
      expect(std::holds_alternative<Value>(prop_value.value()) >> fatal);
      expect(std::get<Value>(prop_value.value()) == value);
   }
   else if constexpr (glz::is_specialization_v<prop_value_t, std::optional> && glz::detail::is_span<typename prop_value_t::value_type>) {
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
   using std::literals::operator""s;
   "parsing"_test = [] {
      test_case const test{};
      expect(test.obj.has_value()) << format_error(!test.obj.has_value() ? test.obj.error() : glz::parse_error{}, test.schema_str);
      expect(fatal(test.obj.has_value()));
   };
   "description"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::description>(test, "desc", "this is a description"s);
   };
   "min1"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::minimum>(test, "min1", std::int64_t{1});
   };
  "max2"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::maximum>(test, "max2", std::int64_t{2});
   };
   "pattern"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::pattern>(test, "pattern", std::string_view{"[a-z]+"});
  };
   "deprecated"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::deprecated>(test, "is_deprecated", true);
   };
   "examples"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::examples>(test, "examples", std::span{ example_arr });
   };
};

int main()
{
   return boost::ut::cfg<>.run({.report_errors = true});
}
