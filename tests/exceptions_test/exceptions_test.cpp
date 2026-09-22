// Glaze Library
// For the license information refer to glaze.hpp

#include <limits>

#include "glaze/containers/ordered_small_map.hpp"
#include "glaze/glaze_exceptions.hpp"
#include "glaze/thread/async.hpp"
#include "glaze/thread/async_string.hpp"
#include "glaze/thread/threadpool.hpp"
#include "scratch_directory.hpp"
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

// Relative scratch paths in this file resolve inside a private directory rather than
// wherever the binary was launched from. This must precede the first suite: ut runs a
// suite from its constructor, during static initialization.
const glz_test::scratch_directory scratch{"exceptions_test"};

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
         R"({"type":"object","properties":{"arr":{"type":"array","items":{"$ref":"#/$defs/uint64_t"},"maxItems":3},"d":{"$ref":"#/$defs/double"},"hello":{"type":"string"},"i":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false,"$defs":{"double":{"type":"number","minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647},"uint64_t":{"type":"integer","minimum":0,"maximum":18446744073709551615}},"title":"my_struct"})")
         << schema;
   };

   "json_schema"_test = [] {
      std::string schema;
      glz::ex::write_json_schema<my_struct>(schema);
      expect(
         schema ==
         R"({"type":"object","properties":{"arr":{"type":"array","items":{"$ref":"#/$defs/uint64_t"},"maxItems":3},"d":{"$ref":"#/$defs/double"},"hello":{"type":"string"},"i":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false,"$defs":{"double":{"type":"number","minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"int32_t":{"type":"integer","minimum":-2147483648,"maximum":2147483647},"uint64_t":{"type":"integer","minimum":0,"maximum":18446744073709551615}},"title":"my_struct"})")
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

struct generic_read_struct
{
   int field{};
};

suite generic_read_tests = [] {
   "generic read valid"_test = [] {
      const auto generic = glz::ex::read_json<glz::generic_u64>(R"({"field":42})");
      const auto value = glz::ex::read_json<generic_read_struct>(generic);
      expect(value.field == 42);
   };

   "generic read into object formats exception"_test = [] {
      const auto generic = glz::ex::read_json<glz::generic_u64>(R"({"field":"invalid"})");
      generic_read_struct value{};
      try {
         glz::ex::read_json(value, generic);
         expect(false) << "expected glz::ex::read_json to throw";
      }
      catch (const std::runtime_error& error) {
         expect(std::string_view{error.what()} ==
                "read_json error: 1:10: parse_number_failure\n"
                "   {\"field\":\"invalid\"}\n"
                "            ^")
            << error.what();
      }
   };

   "generic read value formats exception"_test = [] {
      const auto generic = glz::ex::read_json<glz::generic_u64>(R"({"field":"invalid"})");
      try {
         [[maybe_unused]] auto value = glz::ex::read_json<generic_read_struct>(generic);
         expect(false) << "expected glz::ex::read_json to throw";
      }
      catch (const std::runtime_error& error) {
         expect(std::string_view{error.what()} ==
                "read_json error: 1:10: parse_number_failure\n"
                "   {\"field\":\"invalid\"}\n"
                "            ^")
            << error.what();
      }
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
      expect(throws([&] { glz::ex::read_file_json(s, "../nonexistent_file.json", std::string{}); }));
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

suite async_tests = [] {
   "non-void read and write operations"_test = [] {
      // Initialize with 10.
      glz::async<int> s{10};

      // Read with a lambda that returns a value.
      auto doubled = s.read([](const int& x) -> int { return x * 2; });
      expect(doubled == 20);

      // Write with a lambda that returns a value.
      auto new_value = s.write([](int& x) -> int {
         x += 5;
         return x;
      });
      expect(new_value == 15);

      // Confirm the new value via a read lambda (void-returning).
      s.read([](const int& x) { expect(x == 15); });
   };

   "void read operation"_test = [] {
      glz::async<int> s{20};
      bool flag = false;
      s.read([&flag](const int& x) {
         if (x == 20) flag = true;
      });
      expect(flag);
   };

   "void write operation"_test = [] {
      glz::async<int> s{100};
      s.write([](int& x) { x = 200; });
      s.read([](const int& x) { expect(x == 200); });
   };

   "copy constructor"_test = [] {
      glz::async<int> original{123};
      glz::async<int> copy = original;
      copy.read([](const int& x) { expect(x == 123); });
   };

   "move constructor"_test = [] {
      glz::async<std::string> original{"hello"};
      glz::async<std::string> moved = std::move(original);
      moved.read([](const std::string& s) { expect(s == "hello"); });
   };

   "copy assignment."_test = [] {
      glz::async<int> a{10}, b{20};
      a = b; // requires T to be copy-assignable
      a.read([](const int& x) { expect(x == 20); });
   };

   "move assignment."_test = [] {
      glz::async<std::string> a{"foo"}, b{"bar"};
      a = std::move(b); // requires T to be move-assignable
      a.read([](const std::string& s) { expect(s == "bar"); });
   };

   "concurrent access."_test = [] {
      glz::async<int> s{0};
      const int num_threads = 10;
      const int increments = 1000;
      std::vector<std::thread> threads;

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&] {
            for (int j = 0; j < increments; ++j) {
               s.write([](int& value) { ++value; });
            }
         });
      }

      for (auto& th : threads) {
         th.join();
      }

      // Verify that the value is the expected total.
      s.read([&](const int& value) { expect(value == num_threads * increments); });
   };
};

struct times
{
   uint64_t time;
   std::optional<uint64_t> time1;

   void read_time(uint64_t timeValue) { time = timeValue; }

   void read_time1(std::optional<uint64_t> time1Value) { time1 = time1Value; }
};

struct date
{
   times t;
};

template <>
struct glz::meta<times>
{
   using T = times;
   static constexpr auto value =
      object("time", glz::custom<&T::read_time, nullptr>, "time1", glz::custom<&T::read_time1, nullptr>);
};

template <>
struct glz::meta<date>
{
   using T = date;
   static constexpr auto value = object("date", &T::t);
};

suite custom_tests = [] {
   "glz::custom"_test = [] {
      constexpr std::string_view onlyTimeJson = R"({"date":{"time":1}})";

      date d{};

      try {
         glz::ex::read<glz::opts{.error_on_missing_keys = true}>(d, onlyTimeJson);
      }
      catch (const std::exception& error) {
         expect(false) << error.what() << '\n';
      }
   };
};

suite ordered_small_map_overflow_tests = [] {
   "ordered_small_map reserve overflow throws"_test = [] {
      constexpr auto max_u32_as_size = static_cast<size_t>((std::numeric_limits<uint32_t>::max)());
      if constexpr ((std::numeric_limits<size_t>::max)() > max_u32_as_size) {
         glz::ordered_small_map<int> map;
         const auto too_large = max_u32_as_size + size_t{1};
         expect(throws([&] { map.reserve(too_large); }));
      }
      else {
         // 32-bit size_t cannot represent > uint32_t, so this overflow path is unreachable.
         expect(true);
      }
   };
};

int main() { return 0; }
