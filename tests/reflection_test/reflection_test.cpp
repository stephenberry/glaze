#include <deque>
#include <map>
#include <unordered_map>

#include "boost/ut.hpp"
#include "glaze/glaze.hpp"
#include "glaze/reflection/reflect.hpp"

using namespace boost::ut;

struct my_struct
{
   int i{};
   double d{};
   std::string hello{};
   std::array<uint64_t, 3> arr{};
};

static_assert(!glz::detail::glaze_t<my_struct> && std::is_aggregate_v<std::remove_cvref_t<my_struct>>);
//static_assert(std::tuple_size_v<decltype(glz::detail::to_tuple(my_struct{}))> == 4);

suite reflection = [] {
   "reflect_write"_test = [] {
      std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
      my_struct obj{};
      expect(!glz::read_json(obj, buffer));

      expect(obj.i == 287);
      expect(obj.d == 3.14);
      expect(obj.hello == "Hello World");
      expect(obj.arr == std::array<uint64_t, 3>{1, 2, 3});

      buffer.clear();
      glz::write_json(obj, buffer);

      expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
   };
};

int main() { // Explicitly run registered test suites and report errors
   // This prevents potential issues with thread local variables
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result; }
