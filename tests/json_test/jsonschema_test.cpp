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
   std::string pattern{"[a-z]+"};
};

template <>
struct glz::json_schema<schema_obj>
{
   schema desc{.description = "this is a description"};
   schema min1{.minimum = 1L};
   schema max2{.maximum = 2L};
   schema pattern{.pattern = "[a-z]+"};
};

template <auto Member>
auto expect_property(glz::expected<glz::detail::schematic, glz::parse_error> schematic, std::string_view key, auto value) {
   expect(schematic.has_value() >> fatal);
   expect(schematic->properties->contains(key) >> fatal);
   glz::schema prop = schematic->properties->at(key);
   auto prop_value = prop.*Member;
   expect(prop_value.has_value() >> fatal);
   using prop_value_t = std::decay_t<decltype(prop_value)>;
   if constexpr (std::same_as<prop_value_t, glz::schema::schema_number>) {
      using input_value_t = std::decay_t<decltype(value)>;
      expect(std::holds_alternative<input_value_t>(prop_value.value()) >> fatal);
      expect(std::get<input_value_t>(prop_value.value()) == value);
   }
   else {
      expect(prop_value.value() == value);
   }
}


suite schema_attributes = [] {
   struct test_case
   {
      std::string schema_str = glz::write_json_schema<schema_obj>();
      glz::expected<glz::detail::schematic, glz::parse_error> obj{glz::read_json<glz::detail::schematic>(schema_str)};
   };
   "description"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::description>(test.obj, "desc", "this is a description");
   };
   "min1"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::minimum>(test.obj, "min1", 1L);
   };
  "max2"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::maximum>(test.obj, "max2", 2L);
   };
   "pattern"_test = [] {
      test_case const test{};
      expect_property<&glz::schema::pattern>(test.obj, "pattern", std::string_view{"[a-z]+"});
  };
};

int main()
{
   return boost::ut::cfg<>.run({.report_errors = true});
}
