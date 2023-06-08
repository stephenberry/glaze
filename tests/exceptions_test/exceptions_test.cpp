// Glaze Library
// For the license information refer to glaze.hpp

#include "boost/ut.hpp"
#include "glaze/glaze_exceptions.hpp"

using namespace boost::ut;

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

template <>
struct glz::meta<my_struct>
{
   static constexpr std::string_view name = "my_struct";
   using T = my_struct;
   static constexpr auto value = object(
      "i", [](auto&& v) { return v.i; }, //
      "d", &T::d, //
      "hello", &T::hello, //
      "arr", &T::arr //
   );
};

suite starter = [] {
   "example"_test = [] {
      my_struct s{};
      std::string buffer{};
      glz::ex::write_json(s, buffer);
      expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
      expect(glz::prettify(buffer) == R"({
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [
      1,
      2,
      3
   ]
})");
   };
};

suite basic_types = [] {
   using namespace boost::ut;

   "double write"_test = [] {
      std::string buffer{};
      glz::ex::write_json(3.14, buffer);
      expect(buffer == "3.14") << buffer;
   };

   "double read valid"_test = [] {
      double num{};
      glz::ex::read_json(num, "3.14");
      expect(num == 3.14);
   };

   "int write"_test = [] {
      std::string buffer{};
      glz::ex::write_json(0, buffer);
      expect(buffer == "0");
   };

   "int read valid"_test = [] {
      int num{};
      glz::ex::read_json(num, "-1");
      expect(num == -1);
   };

   "bool write"_test = [] {
      std::string buffer{};
      glz::ex::write_json(true, buffer);
      expect(buffer == "true");
   };

   "bool read valid"_test = [] {
      bool val{};
      glz::ex::read_json(val, "true");
      expect(val == true);
   };

   "bool read invalid"_test = [] {
      expect(throws([] {
         bool val{};
         glz::ex::read_json(val, "tru");
      }));
   };
};


int main()
{
   // Explicitly run registered test suites and report errors
   // This prevents potential issues with thread local variables
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
