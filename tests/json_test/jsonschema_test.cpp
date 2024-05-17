#include <cstdint>
#include <string>

#include <boost/ut.hpp>
#include <glaze/json.hpp>
#include <glaze/json/schema.hpp>

using namespace boost::ut;

struct schema_obj
{
   std::int64_t min1{2};
   std::int64_t max2{5};
   std::string pattern{"[a-z]+"};
};

template <>
struct glz::json_schema<schema_obj>
{
   schema min1{.minimum = 1};
   schema max2{.maximum = 2};
   schema pattern{.pattern = "[a-z]+"};
};


suite schema_attributes = [] {
   "min1"_test = [] {
     auto schema_str = glz::write_json_schema<schema_obj>();
      auto schema_obj = glz::read_json<glz::detail::schematic>(schema_str);
      expect(schema_obj.has_value() >> fatal);

   };
};

int main()
{
   return boost::ut::cfg<>.run({.report_errors = true});
}
