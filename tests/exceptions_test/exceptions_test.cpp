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

   static_assert(glz::detail::readable_map_t<glz::shared_async_map<std::string, std::atomic<int>>>);

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

suite async_string_tests = [] {
   "async_string default constructor"_test = [] {
      glz::async_string s;
      expect(s.empty());
      expect(s.size() == 0);
      expect(s.length() == 0);
   };

   "async_string param constructors"_test = [] {
      glz::async_string s1("Hello");
      expect(s1.size() == 5) << "s1.size()";
      expect(s1 == "Hello");

      std::string st = "World";
      glz::async_string s2(st);
      expect(s2 == "World");

      std::string_view sv("View me");
      glz::async_string s3(sv);
      expect(s3 == "View me");

      // Move construct
      glz::async_string s4(std::move(s2));
      expect(s4 == "World");
      expect(s2.empty()); // Moved-from string should be empty
   };

   "async_string copy constructor"_test = [] {
      glz::async_string original("Copy me");
      glz::async_string copy(original);
      expect(copy == "Copy me");
      expect(copy == original);
   };

   "async_string move constructor"_test = [] {
      glz::async_string original("Move me");
      glz::async_string moved(std::move(original));
      expect(moved == "Move me");
      expect(original.empty());
   };

   "async_string copy assignment"_test = [] {
      glz::async_string s1("First");
      glz::async_string s2("Second");
      s1 = s2;
      expect(s1 == s2);
      expect(s1 == "Second");
   };

   "async_string move assignment"_test = [] {
      glz::async_string s1("First");
      glz::async_string s2("Second");
      s1 = std::move(s2);
      expect(s1 == "Second");
      expect(s2.empty());
   };

   "async_string assignment from various types"_test = [] {
      glz::async_string s;
      s = "Hello again";
      expect(s == "Hello again");
      expect(s.size() == 11);

      std::string st = "Another test";
      s = st;
      expect(s == "Another test");
      expect(s.size() == 12);

      std::string_view sv("Testing 123");
      s = sv;
      expect(s == "Testing 123");
      expect(s.size() == 11);
   };

   "async_string read/write proxy"_test = [] {
      glz::async_string s("initial");
      {
         auto writer = s.write();
         writer->append(" data");
      }
      expect(s == "initial data");

      {
         auto reader = s.read();
         expect(*reader == "initial data");
         expect(reader->size() == 12);
      }
   };

   "async_string modifiers"_test = [] {
      glz::async_string s("Hello");
      s.push_back('!');
      expect(s == "Hello!");
      expect(s.size() == 6);

      s.pop_back();
      expect(s == "Hello");
      expect(s.size() == 5);

      s.clear();
      expect(s.empty());
      expect(s.size() == 0);
   };

   "async_string append and operator+="_test = [] {
      glz::async_string s("Hello");
      s.append(", ").append("World");
      expect(s == "Hello, World");
      expect(s.size() == 12);

      s += "!!!";
      expect(s == "Hello, World!!!");
      expect(s.size() == 15);

      s += '?';
      expect(s == "Hello, World!!!?");
      expect(s.size() == 16);
   };

   "async_string element access"_test = [] {
      glz::async_string s("Test");
      expect(s.at(0) == 'T');
      expect(s[1] == 'e');
      expect(s.front() == 'T');
      expect(s.back() == 't');

      // Check out_of_range
      expect(throws([&] {
         (void)s.at(10); // out_of_range
      }));
   };

   "async_string compare"_test = [] {
      glz::async_string s1("abc");
      glz::async_string s2("abcd");
      expect(s1.compare(s2) < 0);
      expect(s2.compare(s1) > 0);

      expect(s1 < s2);
      expect(s1 != s2);
      expect(not(s1 == s2));
   };

   "async_string relational ops"_test = [] {
      glz::async_string s1("abc");
      glz::async_string s2("abc");
      expect(s1 == s2);
      expect(not(s1 < s2));
      expect(s1 >= s2);
      expect(s1 <= s2);
   };

   "async_string swap"_test = [] {
      glz::async_string s1("Hello");
      glz::async_string s2("World");
      swap(s1, s2);
      expect(s1 == "World");
      expect(s2 == "Hello");
   };

   // Demonstrate Glaze JSON serialization/deserialization
   "async_string write_json / read_json"_test = [] {
      glz::async_string s("Serialize me!");
      std::string buffer{};

      // write_json returns a status code: false means success, true means error
      expect(not glz::write_json(s, buffer)) << "Failed to serialize async_string.";
      // The JSON for a single string is just a quoted string
      expect(buffer == R"("Serialize me!")") << buffer;

      glz::async_string t;
      expect(not glz::read_json(t, buffer)) << "Failed to deserialize async_string.";
      expect(*t.read() == "Serialize me!");
   };

   // Test an empty string's serialization
   "async_string empty serialization"_test = [] {
      glz::async_string s;
      std::string buffer{};

      expect(not glz::write_json(s, buffer));
      // An empty string in JSON
      expect(buffer == R"("")") << buffer;

      glz::async_string t("placeholder");
      expect(not glz::read_json(t, buffer));
      expect(t.empty());
   };

   "async_string starts_with"_test = [] {
      glz::async_string s("Hello, World!");

      // Positive cases
      expect(s.starts_with("Hello"));
      expect(s.starts_with(std::string("Hello")));
      expect(s.starts_with(std::string_view("Hello")));

      // Negative cases
      expect(not s.starts_with("World"));
      expect(not s.starts_with("hello")); // Case-sensitive
      expect(not s.starts_with("Hello, World! And more"));

      // Edge cases
      glz::async_string empty;
      expect(empty.starts_with("")); // An empty string starts with an empty string
      expect(not empty.starts_with("Non-empty"));

      expect(s.starts_with("")); // Any string starts with an empty string
   };

   "async_string ends_with"_test = [] {
      glz::async_string s("Hello, World!");

      // Positive cases
      expect(s.ends_with("World!"));
      expect(s.ends_with(std::string("World!")));
      expect(s.ends_with(std::string_view("World!")));

      // Negative cases
      expect(not s.ends_with("Hello"));
      expect(not s.ends_with("world!")); // Case-sensitive
      expect(not s.ends_with("...World!"));

      // Edge cases
      glz::async_string empty;
      expect(empty.ends_with("")); // An empty string ends with an empty string
      expect(not empty.ends_with("Non-empty"));

      expect(s.ends_with("")); // Any string ends with an empty string
   };

   "async_string substr"_test = [] {
      glz::async_string s("Hello, World!");

      // Basic substrings
      auto sub1 = s.substr(0, 5);
      expect(sub1 == "Hello");
      expect(sub1.size() == 5);

      auto sub2 = s.substr(7, 5);
      expect(sub2 == "World");
      expect(sub2.size() == 5);

      // Substring to the end
      auto sub3 = s.substr(7);
      expect(sub3 == "World!");
      expect(sub3.size() == 6);

      // Full string
      auto sub4 = s.substr(0, s.size());
      expect(sub4 == s);

      // Empty substring
      auto sub5 = s.substr(5, 0);
      expect(sub5.empty());
      expect(sub5.size() == 0);

      // Edge cases
      glz::async_string empty;
      auto sub_empty = empty.substr(0, 1);
      expect(sub_empty.empty());

      // Out of range positions
      expect(throws([&] {
         s.substr(100, 5); // Start position out of range
      }));

      expect(not throws([&] { s.substr(5, 100); }));

      // Start position at the end of the string
      auto sub_end = s.substr(s.size(), 0);
      expect(sub_end.empty());

      // Start position just before the end
      auto sub_last = s.substr(s.size() - 1, 1);
      expect(sub_last == "!");
      expect(sub_last.size() == 1);
   };

#ifdef __cpp_lib_format
   "async_string std::format single argument"_test = [] {
      glz::async_string name("Alice");
      std::string formatted = std::format("Hello, {}!", name);
      expect(formatted == "Hello, Alice!");
   };

   "async_string std::format multiple arguments"_test = [] {
      glz::async_string name("Bob");
      glz::async_string city("New York");
      std::string formatted = std::format("{} is from {}.", name, city);
      expect(formatted == "Bob is from New York.");
   };

   "async_string std::format with empty strings"_test = [] {
      glz::async_string empty{};

      // Formatting with an empty glz::async_string as an argument
      std::string formatted_empty_arg = std::format("Hello, {}!", empty);
      expect(formatted_empty_arg == "Hello, !");
   };

   "async_string std::format numeric and other types"_test = [] {
      glz::async_string name("Diana");
      int age = 30;
      double height = 5.6;

      std::string formatted = std::format("{} is {} years old and {} feet tall.", name, age, height);
      expect(formatted == "Diana is 30 years old and 5.6 feet tall.");
   };
#endif

   "async_string concurrent reads"_test = [] {
      std::string long_string(1024, 'A');
      glz::async_string s(long_string);
      std::vector<std::thread> threads;
      std::mutex mutex;
      std::vector<std::string> results(10);

      for (int i = 0; i < 10; ++i) {
         threads.emplace_back([&, i]() {
            auto reader = s.read();
            std::lock_guard<std::mutex> lock(mutex);
            results[i] = *reader;
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      for (const auto& result : results) {
         expect(result == long_string);
      }
   };

   "async_string concurrent writes with single char"_test = [] {
      glz::async_string s;
      std::vector<std::thread> threads;
      int num_threads = 10;
      std::string expected_result;
      for (int i = 0; i < num_threads; ++i) {
         expected_result += std::string(256, char('a' + i));
      }
      std::string sorted_expected_result = expected_result;
      std::sort(sorted_expected_result.begin(), sorted_expected_result.end());

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&, char_to_append = char('a' + i)]() {
            for (int j = 0; j < 256; ++j) {
               s.push_back(char_to_append);
            }
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      std::string actual_result = *s.read();
      std::string sorted_actual_result = actual_result;
      std::sort(sorted_actual_result.begin(), sorted_actual_result.end());
      expect(sorted_actual_result == sorted_expected_result);
   };

   "async_string concurrent writes with append"_test = [] {
      glz::async_string s;
      std::vector<std::thread> threads;
      int num_threads = 10;
      std::vector<std::string> to_append;
      std::string expected_result;
      for (int i = 0; i < num_threads; ++i) {
         std::string append_str(512, '0' + i);
         to_append.push_back(append_str);
         expected_result += append_str;
      }
      std::sort(expected_result.begin(), expected_result.end());

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&, str_to_append = to_append[i]]() { s.append(str_to_append); });
      }

      for (auto& t : threads) {
         t.join();
      }

      std::string actual_result = *s.read();
      std::sort(actual_result.begin(), actual_result.end());
      expect(actual_result == expected_result);
   };

   "async_string concurrent reads and writes"_test = [] {
      std::string initial_string(512, 'I');
      glz::async_string s(initial_string);
      std::vector<std::thread> threads;
      int num_threads = 10;
      std::string expected_final_string = initial_string;
      std::vector<std::string> appends;
      for (int i = 0; i < num_threads; ++i) {
         std::string append_str(256, '0' + i);
         appends.push_back(append_str);
         expected_final_string += append_str;
      }
      std::string sorted_appends_expected;
      for (size_t i = initial_string.length(); i < expected_final_string.length(); ++i) {
         sorted_appends_expected += expected_final_string[i];
      }
      std::sort(sorted_appends_expected.begin(), sorted_appends_expected.end());
      expected_final_string = initial_string + sorted_appends_expected;

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&, id = i]() {
            // Writer thread
            if (id == 0) {
               s.append(appends[id]);
            }
            else {
               // Reader threads
               // Just access the data, the important part is no crashes
               (void)s.size();
            }
         });
      }
      for (int i = 1; i < num_threads; ++i) {
         threads.emplace_back([&, id = i]() {
            // Writer threads (after the initial writer)
            s.append(appends[id]);
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      std::string actual_result = *s.read();
      std::string sorted_appends_actual = actual_result.substr(initial_string.length());
      std::sort(sorted_appends_actual.begin(), sorted_appends_actual.end());
      expect(initial_string + sorted_appends_actual == expected_final_string);
   };

   "async_string multiple concurrent write proxies"_test = [] {
      glz::async_string s;
      std::vector<std::thread> threads;
      int num_threads = 5;
      std::string expected_append;
      std::vector<std::string> to_append;
      for (int i = 0; i < num_threads; ++i) {
         std::string append_str(512, '0' + i);
         to_append.push_back(append_str);
         expected_append += append_str;
      }
      std::sort(expected_append.begin(), expected_append.end());

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&, str_to_append = to_append[i]]() {
            auto writer = s.write();
            writer->append(str_to_append);
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      std::string actual_result = *s.read();
      std::sort(actual_result.begin(), actual_result.end());
      expect(actual_result == expected_append);
   };

   "async_string concurrent read and modify"_test = [] {
      std::string initial_value(1024, 'X');
      glz::async_string s(initial_value);
      std::vector<std::thread> threads;
      int num_threads = 10;
      std::mutex m;
      std::vector<std::string> observed_values;

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&, id = i]() {
            if (id % 2 == 0) {
               // Reader thread
               auto reader = s.read();
               {
                  std::lock_guard<std::mutex> lock(m);
                  observed_values.push_back(*reader);
               }
            }
            else {
               // Modifier thread
               auto writer = s.write();
               for (int j = 0; j < 128; ++j) {
                  *writer += "a";
               }
            }
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      // It's hard to predict the exact sequence of observed values, but we can check for consistency.
      // All observed values should at least start with the initial value.
      for (const auto& val : observed_values) {
         expect(val.rfind(initial_value, 0) == 0);
      }

      // The final string should have been modified by the modifier threads.
      expect(*s.read().operator->() != initial_value);
      expect(s.read()->length() > initial_value.length());
   };
};

suite sync_tests = [] {
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

int main() { return 0; }
