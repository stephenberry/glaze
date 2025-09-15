// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze_exceptions.hpp"
#include "glaze/thread/async.hpp"
#include "glaze/thread/async_string.hpp"
#include "glaze/thread/shared_async_map.hpp"
#include "glaze/thread/shared_async_vector.hpp"
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
         R"({"type":["object"],"properties":{"arr":{"$ref":"#/$defs/std::array<uint64_t,3>"},"d":{"$ref":"#/$defs/double"},"hello":{"$ref":"#/$defs/std::string"},"i":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false,"$defs":{"double":{"type":["number"],"minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::array<uint64_t,3>":{"type":["array"],"items":{"$ref":"#/$defs/uint64_t"},"minItems":3,"maxItems":3},"std::string":{"type":["string"]},"uint64_t":{"type":["integer"],"minimum":0,"maximum":18446744073709551615}},"title":"my_struct"})")
         << schema;
   };

   "json_schema"_test = [] {
      std::string schema;
      glz::ex::write_json_schema<my_struct>(schema);
      expect(
         schema ==
         R"({"type":["object"],"properties":{"arr":{"$ref":"#/$defs/std::array<uint64_t,3>"},"d":{"$ref":"#/$defs/double"},"hello":{"$ref":"#/$defs/std::string"},"i":{"$ref":"#/$defs/int32_t"}},"additionalProperties":false,"$defs":{"double":{"type":["number"],"minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::array<uint64_t,3>":{"type":["array"],"items":{"$ref":"#/$defs/uint64_t"},"minItems":3,"maxItems":3},"std::string":{"type":["string"]},"uint64_t":{"type":["integer"],"minimum":0,"maximum":18446744073709551615}},"title":"my_struct"})")
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

suite shared_async_map_tests = [] {
   "shared_async_map"_test = [] {
      // Don't do this. This is merely a unit test, shared_async_map allocates a unique_ptr underneath
      glz::shared_async_map<std::string, std::unique_ptr<std::atomic<int>>> map;
      map.emplace("one", std::make_unique<std::atomic<int>>(1));
      map.emplace("two", std::make_unique<std::atomic<int>>(2));
      expect(*map.at("one").value() == 1);
      expect(*map.at("two").value() == 2);
      expect(map.size() == 2);

      for (const auto& [key, value] : map) {
         expect(key.size() == 3);
         expect(*value < 3);
      }

      for (auto&& [key, value] : map) {
         expect(key.size() == 3);
         *value = 3;
      }

      expect(*map.at("one").value() == 3);
      expect(*map.at("two").value() == 3);
   };

   "shared_async_map atomic"_test = [] {
      glz::shared_async_map<std::string, std::atomic<int>> map;
      map.emplace("one", 1);
      map.emplace("two", 2);
      expect(map.at("one").value() == 1);
      expect(map.at("two").value() == 2);
      expect(map.size() == 2);

      for (const auto& [key, value] : map) {
         expect(key.size() == 3);
         expect(value < 3);
      }

      for (auto&& [key, value] : map) {
         expect(key.size() == 3);
         value = 3;
      }

      expect(map.at("one").value() == 3);
      expect(map.at("two").value() == 3);

      map.at("one").value() = 1;

      for (auto it = map.begin(); it < map.end(); ++it) {
         std::cout << it->second << '\n';
      }
   };

   static_assert(glz::readable_map_t<glz::shared_async_map<std::string, std::atomic<int>>>);

   "shared_async_map write_json"_test = [] {
      glz::shared_async_map<std::string, std::atomic<int>> map;
      map["one"] = 1;
      map["two"] = 2;

      std::string buffer{};
      expect(not glz::write_json(map, buffer));
      expect(buffer == R"({"one":1,"two":2})") << buffer;

      map.clear();
      expect(not glz::read_json(map, buffer));
      expect(map.at("one").value() == 1);
      expect(map.at("two").value() == 2);
   };

   // Test serialization and deserialization of an empty shared_async_map
   "shared_async_map empty"_test = [] {
      glz::shared_async_map<std::string, std::atomic<int>> map;

      // Serialize the empty map
      std::string buffer{};
      expect(not glz::write_json(map, buffer));
      expect(buffer == R"({})") << buffer;

      // Clear and deserialize back
      map.clear();
      expect(not glz::read_json(map, buffer));
      expect(map.empty());
   };

   // Test handling of keys and values with special characters
   "shared_async_map special_characters"_test = [] {
      glz::shared_async_map<std::string, std::atomic<int>> map;
      map["key with spaces"] = 42;
      map["key_with_\"quotes\""] = 84;
      map["ключ"] = 168; // "key" in Russian

      std::string buffer{};
      expect(not glz::write_json(map, buffer));

      // Expected JSON with properly escaped characters
      std::string expected = R"({"key with spaces":42,"key_with_\"quotes\"":84,"ключ":168})";
      expect(buffer == expected) << buffer;

      // Deserialize and verify
      map.clear();
      expect(not glz::read_json(map, buffer));
      expect(map.at("key with spaces").value() == 42);
      expect(map.at("key_with_\"quotes\"").value() == 84);
      expect(map.at("ключ").value() == 168);
   };

   // Test serialization and deserialization of a large shared_async_map
   "shared_async_map large_map"_test = [] {
      glz::shared_async_map<int, std::atomic<int>> map;

      // Populate the map with 1000 entries
      for (int i = 0; i < 1000; ++i) {
         map[i] = i * i;
      }

      std::string buffer{};
      expect(not glz::write_json(map, buffer));

      // Simple check to ensure buffer is not empty
      expect(!buffer.empty());

      // Deserialize and verify a few entries
      map.clear();
      expect(not glz::read_json(map, buffer));
      expect(map.size() == 1000);
      expect(map.at(0).value() == 0);
      expect(map.at(999).value() == 999 * 999);
   };

   // Test deserialization with invalid JSON
   "shared_async_map invalid_json"_test = [] {
      glz::shared_async_map<std::string, std::atomic<int>> map;
      std::string invalid_buffer = R"({"one":1, "two": "invalid_value"})"; // "two" should be an integer

      expect(glz::read_json(map, invalid_buffer)); // Expecting an error (assuming 'read_json' returns true on failure)
   };

   // Test updating existing keys and adding new keys
   "shared_async_map update_and_add"_test = [] {
      glz::shared_async_map<std::string, std::atomic<int>> map;
      map["alpha"] = 10;
      map["beta"] = 20;

      std::string buffer{};
      expect(not glz::write_json(map, buffer));
      expect(buffer == R"({"alpha":10,"beta":20})") << buffer;

      // Update existing key and add a new key
      map["alpha"] = 30;
      map["gamma"] = 40;

      expect(not glz::write_json(map, buffer));
      expect(buffer == R"({"alpha":30,"beta":20,"gamma":40})") << buffer;

      // Deserialize and verify
      map.clear();
      expect(not glz::read_json(map, buffer));
      expect(map.at("alpha").value() == 30);
      expect(map.at("beta").value() == 20);
      expect(map.at("gamma").value() == 40);
   };

   // Test concurrent access to the shared_async_map
   "shared_async_map concurrent_access"_test = [] {
      glz::shared_async_map<int, std::atomic<int>> map;
      const int num_threads = 10;
      const int increments_per_thread = 1000;

      // Initialize map with keys
      for (int i = 0; i < num_threads; ++i) {
         map[i] = 0;
      }

      // Launch multiple threads to increment values concurrently
      std::vector<std::thread> threads;
      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&, i]() {
            for (int j = 0; j < increments_per_thread; ++j) {
               ++map[i].value();
            }
         });
      }

      // Wait for all threads to finish
      for (auto& th : threads) {
         th.join();
      }

      // Verify the results
      for (int i = 0; i < num_threads; ++i) {
         expect(map.at(i).value() == increments_per_thread) << "Key " << i;
      }
   };

   // Test removal of keys from the shared_async_map
   "shared_async_map remove_keys"_test = [] {
      glz::shared_async_map<std::string, std::atomic<int>> map;
      map["first"] = 100;
      map["second"] = 200;
      map["third"] = 300;

      // Remove a key
      map.erase("second");
      expect(map.size() == 2);
      expect(map.find("second") == map.end());

      // Serialize and verify
      std::string buffer{};
      expect(not glz::write_json(map, buffer));
      expect(buffer == R"({"first":100,"third":300})") << buffer;

      // Deserialize and verify
      map.clear();
      expect(not glz::read_json(map, buffer));
      expect(map.size() == 2);
      expect(map.at("first").value() == 100);
      expect(map.at("third").value() == 300);
   };
};

suite shared_async_vector_tests = [] {
   "shared_async_vector atomic"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;
      vec.emplace_back(1);
      vec.emplace_back(2);
      expect(vec.at(0).value() == 1);
      expect(vec.at(1).value() == 2);
      expect(vec.size() == 2);

      // Iterate over the vector with constterator
      for (const auto& value : vec) {
         expect(value < 3);
      }

      // Iterate over the vector with non-const iterator and modify values
      for (auto&& value : vec) {
         value = 3;
      }

      expect(vec.at(0).value() == 3);
      expect(vec.at(1).value() == 3);

      vec.at(0).value() = 1;

      for (auto it = vec.begin(); it != vec.end(); ++it) {
         std::cout << *it << '\n';
      }
   };

   "shared_async_vector write_json"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;
      vec.emplace_back(1);
      vec.emplace_back(2);

      std::string buffer{};
      expect(not glz::write_json(vec, buffer));
      expect(buffer == R"([1,2])") << buffer;

      vec.clear();
      expect(not glz::read_json(vec, buffer));
      expect(vec.at(0).value() == 1);
      expect(vec.at(1).value() == 2);
   };

   // Test serialization and deserialization of an empty shared_async_vector
   "shared_async_vector empty"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;

      // Serialize the empty vector
      std::string buffer{};
      expect(not glz::write_json(vec, buffer));
      expect(buffer == R"([])") << buffer;

      // Clear and deserialize back
      vec.clear();
      expect(not glz::read_json(vec, buffer));
      expect(vec.empty());
   };

   // Test handling of values with special characters (using strings)
   "shared_async_vector special_characters"_test = [] {
      glz::shared_async_vector<std::string> vec;
      vec.emplace_back("string with spaces");
      vec.emplace_back("string_with_\"quotes\"");
      vec.emplace_back("строка"); // "string" in Russian

      std::string buffer{};
      expect(not glz::write_json(vec, buffer));

      // Expected JSON with properly escaped characters
      std::string expected = R"(["string with spaces","string_with_\"quotes\"","строка"])";
      expect(buffer == expected) << buffer;

      // Deserialize and verify
      vec.clear();
      expect(not glz::read_json(vec, buffer));
      expect(vec.at(0).value() == "string with spaces");
      expect(vec.at(1).value() == "string_with_\"quotes\"");
      expect(vec.at(2).value() == "строка");
   };

   // Test serialization and deserialization of a large shared_async_vector
   "shared_async_vector large_vector"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;

      // Populate the vector with 1000 entries
      for (int i = 0; i < 1000; ++i) {
         vec.emplace_back(i * i);
      }

      std::string buffer{};
      expect(not glz::write_json(vec, buffer));

      // Simple check to ensure buffer is not empty
      expect(!buffer.empty());

      // Deserialize and verify a few entries
      vec.clear();
      expect(not glz::read_json(vec, buffer));
      expect(vec.size() == 1000);
      expect(vec.at(0).value() == 0);
      expect(vec.at(999).value() == 999 * 999);
   };

   // Test deserialization with invalid JSON
   "shared_async_vector invalid_json"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;
      std::string invalid_buffer = R"([1, "invalid_value", 3])"; // Second element should be an integer

      expect(glz::read_json(vec, invalid_buffer));
   };

   // Test updating existing elements and adding new elements
   "shared_async_vector update_and_add"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;
      vec.emplace_back(10);
      vec.emplace_back(20);

      std::string buffer{};
      expect(not glz::write_json(vec, buffer));
      expect(buffer == R"([10,20])") << buffer;

      // Update existing element and add a new element
      vec.at(0).value() = 30;
      vec.emplace_back(40);

      expect(not glz::write_json(vec, buffer));
      expect(buffer == R"([30,20,40])") << buffer;

      // Deserialize and verify
      vec.clear();
      expect(not glz::read_json(vec, buffer));
      expect(vec.at(0).value() == 30);
      expect(vec.at(1).value() == 20);
      expect(vec.at(2).value() == 40);
   };

   // Test concurrent access to the shared_async_vector
   "shared_async_vector concurrent_access"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;
      const int num_threads = 10;
      const int increments_per_thread = 1000;

      // Initialize vector with zeros
      for (int i = 0; i < num_threads; ++i) {
         vec.emplace_back(0);
      }

      // Launch multiple threads to increment values concurrently
      std::vector<std::thread> threads;
      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&, i]() {
            for (int j = 0; j < increments_per_thread; ++j) {
               ++vec.at(i).value();
            }
         });
      }

      // Wait for all threads to finish
      for (auto& th : threads) {
         th.join();
      }

      // Verify the results
      for (int i = 0; i < num_threads; ++i) {
         expect(vec.at(i).value() == increments_per_thread) << "Index " << i;
      }
   };

   // Test removal of elements from the shared_async_vector
   /*"shared_async_vector remove_elements"_test = [] {
      glz::shared_async_vector<std::atomic<int>> vec;
      vec.emplace_back(100); // Index 0
      vec.emplace_back(200); // Index 1
      vec.emplace_back(300); // Index 2

      // Remove the element at index 1
      vec.erase(vec.begin() + 1);
      expect(vec.size() == 2);

      // Serialize and verify
      std::string buffer{};
      expect(not glz::write_json(vec, buffer));
      expect(buffer == R"([100,300])") << buffer;

      // Deserialize and verify
      vec.clear();
      expect(not glz::read_json(vec, buffer));
      expect(vec.size() == 2);
      expect(vec.at(0).value() == 100);
      expect(vec.at(1).value() == 300);
   };*/
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

int main() { return 0; }
