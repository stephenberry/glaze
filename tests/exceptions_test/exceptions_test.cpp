// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/glaze_exceptions.hpp"
#include "glaze/thread/threadpool.hpp"
#include "ut/ut.hpp"

using namespace ut;

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
      expect(glz::prettify_json(buffer) == R"({
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

   "json_schema"_test = [] {
      const std::string schema = glz::ex::write_json_schema<my_struct>();
      expect(
         schema ==
         R"({"type":["object"],"properties":{"arr":{"$ref":"#/$defs/std::array<uint64_t,3>"},"d":{"$ref":"#/$defs/double"},"hello":{"$ref":"#/$defs/std::string"},"i":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false,"$defs":{"double":{"type":["number"],"minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::array<uint64_t,3>":{"type":["array"],"items":{"$ref":"#/$defs/uint64_t"},"minItems":3,"maxItems":3},"std::string":{"type":["string"]},"uint64_t":{"type":["integer"],"minimum":0,"maximum":18446744073709551615}}})")
         << schema;
   };

   "json_schema"_test = [] {
      std::string schema;
      glz::ex::write_json_schema<my_struct>(schema);
      expect(
         schema ==
         R"({"type":["object"],"properties":{"arr":{"$ref":"#/$defs/std::array<uint64_t,3>"},"d":{"$ref":"#/$defs/double"},"hello":{"$ref":"#/$defs/std::string"},"i":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false,"$defs":{"double":{"type":["number"],"minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::array<uint64_t,3>":{"type":["array"],"items":{"$ref":"#/$defs/uint64_t"},"minItems":3,"maxItems":3},"std::string":{"type":["string"]},"uint64_t":{"type":["integer"],"minimum":0,"maximum":18446744073709551615}}})")
         << schema;
   };
};

suite basic_types = [] {
   using namespace ut;

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

   "bool write"_test = [] {
      std::string buffer = glz::ex::write_json(true);
      expect(buffer == "true");
   };

   "bool write"_test = [] {
      std::string buffer{};
      glz::ex::write<glz::opts{}>(true, buffer);
      expect(buffer == "true");
   };

   "bool write"_test = [] {
      std::string buffer = glz::ex::write<glz::opts{}>(true);
      expect(buffer == "true");
   };

   "bool read valid"_test = [] {
      bool val{};
      glz::ex::read_json(val, "true");
      expect(val == true);
   };

   "bool read valid"_test = [] {
      bool val = glz::ex::read_json<bool>("true");
      expect(val == true);
   };

   "bool read valid"_test = [] {
      bool val{};
      glz::ex::read<glz::opts{}>(val, "true");
      expect(val == true);
   };

   "bool read invalid"_test = [] {
      expect(throws([] {
         bool val{};
         glz::ex::read_json(val, "tru");
      }));
   };

   "bool read invalid"_test = [] {
      expect(throws([] { [[maybe_unused]] bool val = glz::ex::read_json<bool>("tru"); }));
   };
};

struct file_struct
{
   std::string name;
   std::string label;

   struct glaze
   {
      using T = file_struct;
      static constexpr auto value = glz::object("name", &T::name, "label", &T::label);
   };
};

suite read_file_test = [] {
   "read_file valid"_test = [] {
      std::string filename = "../file.json";
      {
         std::ofstream out(filename);
         expect(bool(out));
         if (out) {
            out << R"({
     "name": "my",
     "label": "label"
   })";
         }
      }

      file_struct s;
      std::string buffer{};
      glz::ex::read_file_json(s, filename, buffer);
   };

   "read_file invalid"_test = [] {
      file_struct s;
      expect(throws([&] { glz::ex::read_file_json(s, "../nonexsistant_file.json", std::string{}); }));
   };
};

suite thread_pool = [] {
   "thread pool throw"_test = [] {
      glz::pool pool{1};

      std::atomic<int> x = 0;

      expect(throws([&] {
         auto future = pool.emplace_back([&] {
            ++x;
            throw std::runtime_error("aha!");
         });
         pool.wait();
         future.get();
      }));
   };
};

int main() { return 0; }
