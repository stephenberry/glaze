// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/glaze_exceptions.hpp"
#include "glaze/thread/async_map.hpp"
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

suite async_map_tests = [] {
   "async_map"_test = [] {
      // Don't do this. This is merely a unit test, async_map allocates a unique_ptr underneath
      glz::async_map<std::string, std::unique_ptr<std::atomic<int>>> map;
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

   "async_map atomic"_test = [] {
      glz::async_map<std::string, std::atomic<int>> map;
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

   static_assert(glz::detail::readable_map_t<glz::async_map<std::string, std::atomic<int>>>);

   "async_map write_json"_test = [] {
      glz::async_map<std::string, std::atomic<int>> map;
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

   // Test serialization and deserialization of an empty async_map
   "async_map empty"_test = [] {
      glz::async_map<std::string, std::atomic<int>> map;

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
   "async_map special_characters"_test = [] {
      glz::async_map<std::string, std::atomic<int>> map;
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

   // Test serialization and deserialization of a large async_map
   "async_map large_map"_test = [] {
      glz::async_map<int, std::atomic<int>> map;

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
   "async_map invalid_json"_test = [] {
      glz::async_map<std::string, std::atomic<int>> map;
      std::string invalid_buffer = R"({"one":1, "two": "invalid_value"})"; // "two" should be an integer

      expect(glz::read_json(map, invalid_buffer)); // Expecting an error (assuming 'read_json' returns true on failure)
   };

   // Test updating existing keys and adding new keys
   "async_map update_and_add"_test = [] {
      glz::async_map<std::string, std::atomic<int>> map;
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

   // Test concurrent access to the async_map
   "async_map concurrent_access"_test = [] {
      glz::async_map<int, std::atomic<int>> map;
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

   // Test removal of keys from the async_map
   "async_map remove_keys"_test = [] {
      glz::async_map<std::string, std::atomic<int>> map;
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
      
      auto other = map; // copy
      expect(other.size() == 2);
      expect(other.at("first").value() == 100);
      expect(other.at("third").value() == 300);
   };
};

int main() { return 0; }
