#include <deque>
#include <map>
#include <unordered_map>

#include "boost/ut.hpp"
#include "glaze/glaze.hpp"
#include "glaze/reflection/reflect.hpp"

using namespace boost::ut;

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

static_assert(!glz::detail::glaze_t<my_struct> && std::is_aggregate_v<std::remove_cvref_t<my_struct>>);
//static_assert(std::tuple_size_v<decltype(glz::detail::to_tuple(my_struct{}))> == 4);

suite reflection = [] {
   "reflect_write"_test = [] {
      std::string buffer{};
      my_struct obj{};
      glz::write_json(obj, buffer);
      expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})") << buffer;

      obj.i = {};
      obj.d = {};
      obj.hello = {};
      obj.arr = {};
      const auto ec = glz::read_json(obj, buffer);
      if (ec) {
         std::cout << glz::format_error(ec, buffer) << '\n';
      }
      expect(!ec);
      expect(obj.i == 287);
      expect(obj.d == 3.14);
      expect(obj.hello == "Hello World");
      expect(obj.arr == std::array<uint64_t, 3>{1, 2, 3});
   };
};

int main() { // Explicitly run registered test suites and report errors
   // This prevents potential issues with thread local variables
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result; }
