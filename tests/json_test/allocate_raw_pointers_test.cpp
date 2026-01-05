// Glaze Library
// For the license information refer to glaze.hpp

// Comprehensive tests for the allocate_raw_pointers option
// Tests raw pointer allocation during deserialization across JSON, BEVE, CBOR, and MSGPACK formats

#include <map>
#include <unordered_map>
#include <vector>

#include "glaze/cbor.hpp" // CBOR not included in glaze.hpp by default
#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Test structs using pure reflection (no glz::meta needed)
struct simple_struct
{
   int x{};
   int y{};
   int z{};

   bool operator==(const simple_struct&) const = default;
};

struct nested_struct
{
   std::string name{};
   simple_struct* data{};

   bool operator==(const nested_struct& other) const
   {
      if (name != other.name) return false;
      if ((data == nullptr) != (other.data == nullptr)) return false;
      if (data && other.data && *data != *other.data) return false;
      return true;
   }
};

struct multi_pointer_struct
{
   int* int_ptr{};
   double* double_ptr{};
   std::string* string_ptr{};
};

// For deeply nested pointer test - must be at namespace scope for reflection
struct level2
{
   int value{};
};
struct level1
{
   level2* nested{};
};
struct level0
{
   level1* nested{};
};

// Custom options struct with allocate_raw_pointers enabled
struct alloc_opts : glz::opts
{
   bool allocate_raw_pointers = true;
};

struct alloc_opts_beve : glz::opts
{
   static constexpr uint32_t format = glz::BEVE;
   bool allocate_raw_pointers = true;
};

struct alloc_opts_cbor : glz::opts
{
   static constexpr uint32_t format = glz::CBOR;
   bool allocate_raw_pointers = true;
};

struct alloc_opts_msgpack : glz::opts
{
   static constexpr uint32_t format = glz::MSGPACK;
   bool allocate_raw_pointers = true;
};

// Helper to clean up allocated pointers
template <typename T>
void cleanup_vector(std::vector<T*>& vec)
{
   for (auto* p : vec) {
      delete p;
   }
   vec.clear();
}

template <typename K, typename V>
void cleanup_map(std::map<K, V*>& m)
{
   for (auto& [k, v] : m) {
      delete v;
   }
   m.clear();
}

template <typename K, typename V>
void cleanup_map(std::unordered_map<K, V*>& m)
{
   for (auto& [k, v] : m) {
      delete v;
   }
   m.clear();
}

// =============================================================================
// JSON Format Tests
// =============================================================================

suite json_allocate_raw_pointers_tests = [] {
   "json_single_pointer_allocation"_test = [] {
      simple_struct* ptr = nullptr;
      std::string json = R"({"x":1,"y":2,"z":3})";

      auto ec = glz::read<alloc_opts{}>(ptr, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr != nullptr);
      expect(ptr->x == 1);
      expect(ptr->y == 2);
      expect(ptr->z == 3);

      delete ptr;
   };

   "json_single_pointer_without_option_fails"_test = [] {
      simple_struct* ptr = nullptr;
      std::string json = R"({"x":1,"y":2,"z":3})";

      auto ec = glz::read_json(ptr, json);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "json_vector_of_pointers"_test = [] {
      std::vector<simple_struct*> vec;
      std::string json = R"([{"x":1,"y":2,"z":3},{"x":4,"y":5,"z":6},{"x":7,"y":8,"z":9}])";

      auto ec = glz::read<alloc_opts{}>(vec, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(vec.size() == 3);
      expect(vec[0]->x == 1);
      expect(vec[1]->x == 4);
      expect(vec[2]->x == 7);

      cleanup_vector(vec);
   };

   "json_vector_of_pointers_without_option_fails"_test = [] {
      std::vector<simple_struct*> vec;
      std::string json = R"([{"x":1,"y":2,"z":3}])";

      auto ec = glz::read_json(vec, json);
      expect(ec == glz::error_code::invalid_nullable_read);
      // Note: vec may have been resized before the error, but elements should be null
   };

   "json_map_with_pointer_values"_test = [] {
      std::map<std::string, simple_struct*> m;
      std::string json = R"({"first":{"x":1,"y":2,"z":3},"second":{"x":4,"y":5,"z":6}})";

      auto ec = glz::read<alloc_opts{}>(m, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(m.size() == 2);
      expect(m["first"]->x == 1);
      expect(m["second"]->x == 4);

      cleanup_map(m);
   };

   "json_unordered_map_with_pointer_values"_test = [] {
      std::unordered_map<std::string, simple_struct*> m;
      std::string json = R"({"alpha":{"x":10,"y":20,"z":30}})";

      auto ec = glz::read<alloc_opts{}>(m, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(m.size() == 1);
      expect(m["alpha"]->x == 10);

      cleanup_map(m);
   };

   "json_nested_pointer_struct"_test = [] {
      nested_struct obj;
      std::string json = R"({"name":"test","data":{"x":100,"y":200,"z":300}})";

      auto ec = glz::read<alloc_opts{}>(obj, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(obj.name == "test");
      expect(obj.data != nullptr);
      expect(obj.data->x == 100);
      expect(obj.data->y == 200);
      expect(obj.data->z == 300);

      delete obj.data;
   };

   "json_multi_pointer_struct"_test = [] {
      multi_pointer_struct obj;
      std::string json = R"({"int_ptr":42,"double_ptr":3.14,"string_ptr":"hello"})";

      auto ec = glz::read<alloc_opts{}>(obj, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(obj.int_ptr != nullptr);
      expect(obj.double_ptr != nullptr);
      expect(obj.string_ptr != nullptr);
      expect(*obj.int_ptr == 42);
      expect(*obj.double_ptr == 3.14);
      expect(*obj.string_ptr == "hello");

      delete obj.int_ptr;
      delete obj.double_ptr;
      delete obj.string_ptr;
   };

   "json_null_value_does_not_allocate"_test = [] {
      simple_struct* ptr = nullptr;
      std::string json = "null";

      auto ec = glz::read<alloc_opts{}>(ptr, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr == nullptr); // Should remain null
   };

   "json_preallocated_pointer_works_without_option"_test = [] {
      simple_struct value{};
      simple_struct* ptr = &value;
      std::string json = R"({"x":42,"y":43,"z":44})";

      auto ec = glz::read_json(ptr, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(value.x == 42);
      expect(value.y == 43);
      expect(value.z == 44);
   };

   "json_roundtrip_vector_of_pointers"_test = [] {
      // Write
      std::vector<simple_struct*> original;
      original.push_back(new simple_struct{1, 2, 3});
      original.push_back(new simple_struct{4, 5, 6});

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      cleanup_vector(original);

      // Read back
      std::vector<simple_struct*> result;
      auto rec = glz::read<alloc_opts{}>(result, json);
      expect(rec == glz::error_code::none) << glz::format_error(rec, json);
      expect(result.size() == 2);
      expect(result[0]->x == 1);
      expect(result[1]->x == 4);

      cleanup_vector(result);
   };

   "json_primitive_pointer"_test = [] {
      int* ptr = nullptr;
      std::string json = "42";

      auto ec = glz::read<alloc_opts{}>(ptr, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr != nullptr);
      expect(*ptr == 42);

      delete ptr;
   };

   "json_string_pointer"_test = [] {
      std::string* ptr = nullptr;
      std::string json = R"("hello world")";

      auto ec = glz::read<alloc_opts{}>(ptr, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr != nullptr);
      expect(*ptr == "hello world");

      delete ptr;
   };

   "json_vector_of_int_pointers"_test = [] {
      std::vector<int*> vec;
      std::string json = R"([1,2,3,4,5])";

      auto ec = glz::read<alloc_opts{}>(vec, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(vec.size() == 5);
      expect(*vec[0] == 1);
      expect(*vec[4] == 5);

      for (auto* p : vec) delete p;
   };

   "json_double_pointer"_test = [] {
      double* ptr = nullptr;
      std::string json = "3.14159";

      auto ec = glz::read<alloc_opts{}>(ptr, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr != nullptr);
      expect(*ptr == 3.14159);

      delete ptr;
   };
};

// =============================================================================
// BEVE Format Tests
// =============================================================================

suite beve_allocate_raw_pointers_tests = [] {
   "beve_single_pointer_allocation"_test = [] {
      // Write a value first
      simple_struct original{10, 20, 30};
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      // Read into null pointer with allocation
      simple_struct* ptr = nullptr;
      auto ec = glz::read<alloc_opts_beve{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(ptr->x == 10);
      expect(ptr->y == 20);
      expect(ptr->z == 30);

      delete ptr;
   };

   "beve_single_pointer_without_option_fails"_test = [] {
      simple_struct original{10, 20, 30};
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      simple_struct* ptr = nullptr;
      auto ec = glz::read_beve(ptr, buffer);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "beve_vector_of_pointers"_test = [] {
      // Write vector of values
      std::vector<simple_struct> original{{1, 2, 3}, {4, 5, 6}};
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      // Read into vector of pointers
      std::vector<simple_struct*> result;
      auto ec = glz::read<alloc_opts_beve{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.size() == 2);
      expect(result[0]->x == 1);
      expect(result[1]->x == 4);

      cleanup_vector(result);
   };

   "beve_map_with_pointer_values"_test = [] {
      std::map<std::string, simple_struct> original{{"a", {1, 2, 3}}, {"b", {4, 5, 6}}};
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      std::map<std::string, simple_struct*> result;
      auto ec = glz::read<alloc_opts_beve{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.size() == 2);
      expect(result["a"]->x == 1);
      expect(result["b"]->x == 4);

      cleanup_map(result);
   };

   "beve_roundtrip_vector_of_pointers"_test = [] {
      std::vector<simple_struct*> original;
      original.push_back(new simple_struct{100, 200, 300});
      original.push_back(new simple_struct{400, 500, 600});

      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      cleanup_vector(original);

      std::vector<simple_struct*> result;
      auto ec = glz::read<alloc_opts_beve{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.size() == 2);
      expect(result[0]->x == 100);
      expect(result[1]->x == 400);

      cleanup_vector(result);
   };

   "beve_primitive_pointer"_test = [] {
      double original = 3.14159;
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      double* ptr = nullptr;
      auto ec = glz::read<alloc_opts_beve{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 3.14159);

      delete ptr;
   };

   "beve_nested_struct_pointer"_test = [] {
      simple_struct inner{7, 8, 9};
      nested_struct original{"nested_test", &inner};

      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      nested_struct result;
      auto ec = glz::read<alloc_opts_beve{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.name == "nested_test");
      expect(result.data != nullptr);
      expect(result.data->x == 7);

      delete result.data;
   };

   "beve_int_pointer"_test = [] {
      int original = 12345;
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      auto ec = glz::read<alloc_opts_beve{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 12345);

      delete ptr;
   };

   "beve_string_pointer"_test = [] {
      std::string original = "beve test string";
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      std::string* ptr = nullptr;
      auto ec = glz::read<alloc_opts_beve{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == "beve test string");

      delete ptr;
   };
};

// =============================================================================
// CBOR Format Tests
// =============================================================================

suite cbor_allocate_raw_pointers_tests = [] {
   "cbor_int_pointer"_test = [] {
      int original = 12345;
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      auto ec = glz::read<alloc_opts_cbor{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 12345);

      delete ptr;
   };

   "cbor_int_pointer_without_option_fails"_test = [] {
      int original = 12345;
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      auto ec = glz::read_cbor(ptr, buffer);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "cbor_double_pointer"_test = [] {
      double original = 3.14159;
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      double* ptr = nullptr;
      auto ec = glz::read<alloc_opts_cbor{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 3.14159);

      delete ptr;
   };

   "cbor_string_pointer"_test = [] {
      std::string original = "cbor test string";
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      std::string* ptr = nullptr;
      auto ec = glz::read<alloc_opts_cbor{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == "cbor test string");

      delete ptr;
   };

   "cbor_vector_of_int_pointers"_test = [] {
      // Write as vector of pointers to get proper CBOR array format
      std::vector<int*> original;
      original.push_back(new int{1});
      original.push_back(new int{2});
      original.push_back(new int{3});

      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      for (auto* p : original) delete p;

      std::vector<int*> result;
      auto ec = glz::read<alloc_opts_cbor{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.size() == 3);
      expect(*result[0] == 1);
      expect(*result[2] == 3);

      for (auto* p : result) delete p;
   };

   "cbor_bool_pointer"_test = [] {
      bool original = true;
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      bool* ptr = nullptr;
      auto ec = glz::read<alloc_opts_cbor{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == true);

      delete ptr;
   };

   "cbor_map_of_int_pointers"_test = [] {
      std::map<std::string, int> original{{"a", 1}, {"b", 2}};
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      std::map<std::string, int*> result;
      auto ec = glz::read<alloc_opts_cbor{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.size() == 2);
      expect(*result["a"] == 1);
      expect(*result["b"] == 2);

      for (auto& [k, v] : result) delete v;
   };
};

// =============================================================================
// MSGPACK Format Tests
// =============================================================================

suite msgpack_allocate_raw_pointers_tests = [] {
   "msgpack_int_pointer"_test = [] {
      int original = 54321;
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      auto ec = glz::read<alloc_opts_msgpack{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 54321);

      delete ptr;
   };

   "msgpack_int_pointer_without_option_fails"_test = [] {
      int original = 54321;
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      auto ec = glz::read_msgpack(ptr, buffer);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "msgpack_double_pointer"_test = [] {
      double original = 2.71828;
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      double* ptr = nullptr;
      auto ec = glz::read<alloc_opts_msgpack{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 2.71828);

      delete ptr;
   };

   "msgpack_string_pointer"_test = [] {
      std::string original = "msgpack test string";
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      std::string* ptr = nullptr;
      auto ec = glz::read<alloc_opts_msgpack{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == "msgpack test string");

      delete ptr;
   };

   "msgpack_vector_of_int_pointers"_test = [] {
      // Write as vector of pointers to get proper format
      std::vector<int*> original;
      original.push_back(new int{10});
      original.push_back(new int{20});
      original.push_back(new int{30});

      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      for (auto* p : original) delete p;

      std::vector<int*> result;
      auto ec = glz::read<alloc_opts_msgpack{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.size() == 3);
      expect(*result[0] == 10);
      expect(*result[2] == 30);

      for (auto* p : result) delete p;
   };

   "msgpack_bool_pointer"_test = [] {
      bool original = true;
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      bool* ptr = nullptr;
      auto ec = glz::read<alloc_opts_msgpack{}>(ptr, buffer);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == true);

      delete ptr;
   };

   "msgpack_map_of_int_pointers"_test = [] {
      std::map<std::string, int> original{{"x", 100}, {"y", 200}};
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      std::map<std::string, int*> result;
      auto ec = glz::read<alloc_opts_msgpack{}>(result, buffer);
      expect(ec == glz::error_code::none);
      expect(result.size() == 2);
      expect(*result["x"] == 100);
      expect(*result["y"] == 200);

      for (auto& [k, v] : result) delete v;
   };
};

// =============================================================================
// Runtime allocate_raw_pointers via Custom Context Tests
// =============================================================================

// Custom context with runtime allocate_raw_pointers control
struct secure_context : glz::context
{
   bool allocate_raw_pointers = false;
};

suite runtime_allocate_raw_pointers_tests = [] {
   "runtime_json_allocation_enabled"_test = [] {
      simple_struct* ptr = nullptr;
      std::string json = R"({"x":1,"y":2,"z":3})";

      secure_context ctx;
      ctx.allocate_raw_pointers = true; // Enable at runtime

      auto ec = glz::read<glz::opts{}>(ptr, json, ctx);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr != nullptr);
      expect(ptr->x == 1);
      expect(ptr->y == 2);
      expect(ptr->z == 3);

      delete ptr;
   };

   "runtime_json_allocation_disabled"_test = [] {
      simple_struct* ptr = nullptr;
      std::string json = R"({"x":1,"y":2,"z":3})";

      secure_context ctx;
      ctx.allocate_raw_pointers = false; // Disable at runtime

      auto ec = glz::read<glz::opts{}>(ptr, json, ctx);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "runtime_json_vector_of_pointers_enabled"_test = [] {
      std::vector<simple_struct*> vec;
      std::string json = R"([{"x":1,"y":2,"z":3},{"x":4,"y":5,"z":6}])";

      secure_context ctx;
      ctx.allocate_raw_pointers = true;

      auto ec = glz::read<glz::opts{}>(vec, json, ctx);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(vec.size() == 2);
      expect(vec[0]->x == 1);
      expect(vec[1]->x == 4);

      cleanup_vector(vec);
   };

   "runtime_json_trust_level_pattern"_test = [] {
      // Simulates the use case from the GitHub issue - different trust levels
      auto deserialize_with_trust = [](const std::string& json, bool is_trusted) -> simple_struct* {
         simple_struct* ptr = nullptr;
         secure_context ctx;
         ctx.allocate_raw_pointers = is_trusted;

         auto ec = glz::read<glz::opts{}>(ptr, json, ctx);
         if (ec) {
            return nullptr;
         }
         return ptr;
      };

      std::string json = R"({"x":42,"y":43,"z":44})";

      // Trusted source - should allocate
      auto* trusted_result = deserialize_with_trust(json, true);
      expect(trusted_result != nullptr);
      expect(trusted_result->x == 42);
      delete trusted_result;

      // Untrusted source - should fail
      auto* untrusted_result = deserialize_with_trust(json, false);
      expect(untrusted_result == nullptr);
   };

   "runtime_beve_allocation_enabled"_test = [] {
      simple_struct original{10, 20, 30};
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      simple_struct* ptr = nullptr;
      secure_context ctx;
      ctx.allocate_raw_pointers = true;

      auto ec = glz::read<glz::opts{.format = glz::BEVE}>(ptr, buffer, ctx);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(ptr->x == 10);

      delete ptr;
   };

   "runtime_beve_allocation_disabled"_test = [] {
      simple_struct original{10, 20, 30};
      std::string buffer;
      auto wec = glz::write_beve(original, buffer);
      expect(!wec);

      simple_struct* ptr = nullptr;
      secure_context ctx;
      ctx.allocate_raw_pointers = false;

      auto ec = glz::read<glz::opts{.format = glz::BEVE}>(ptr, buffer, ctx);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "runtime_cbor_allocation_enabled"_test = [] {
      int original = 12345;
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      secure_context ctx;
      ctx.allocate_raw_pointers = true;

      auto ec = glz::read<glz::opts{.format = glz::CBOR}>(ptr, buffer, ctx);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 12345);

      delete ptr;
   };

   "runtime_cbor_allocation_disabled"_test = [] {
      int original = 12345;
      std::string buffer;
      auto wec = glz::write_cbor(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      secure_context ctx;
      ctx.allocate_raw_pointers = false;

      auto ec = glz::read<glz::opts{.format = glz::CBOR}>(ptr, buffer, ctx);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "runtime_msgpack_allocation_enabled"_test = [] {
      int original = 54321;
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      secure_context ctx;
      ctx.allocate_raw_pointers = true;

      auto ec = glz::read<glz::opts{.format = glz::MSGPACK}>(ptr, buffer, ctx);
      expect(ec == glz::error_code::none);
      expect(ptr != nullptr);
      expect(*ptr == 54321);

      delete ptr;
   };

   "runtime_msgpack_allocation_disabled"_test = [] {
      int original = 54321;
      std::string buffer;
      auto wec = glz::write_msgpack(original, buffer);
      expect(!wec);

      int* ptr = nullptr;
      secure_context ctx;
      ctx.allocate_raw_pointers = false;

      auto ec = glz::read<glz::opts{.format = glz::MSGPACK}>(ptr, buffer, ctx);
      expect(ec == glz::error_code::invalid_nullable_read);
      expect(ptr == nullptr);
   };

   "runtime_compile_time_option_takes_precedence"_test = [] {
      // When compile-time option is set to true, runtime context is not checked
      simple_struct* ptr = nullptr;
      std::string json = R"({"x":1,"y":2,"z":3})";

      secure_context ctx;
      ctx.allocate_raw_pointers = false; // Runtime says no

      // But compile-time option says yes - should still allocate
      auto ec = glz::read<alloc_opts{}>(ptr, json, ctx);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr != nullptr);

      delete ptr;
   };

   "runtime_nested_pointers"_test = [] {
      level0 obj;
      std::string json = R"({"nested":{"nested":{"value":42}}})";

      secure_context ctx;
      ctx.allocate_raw_pointers = true;

      auto ec = glz::read<glz::opts{}>(obj, json, ctx);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(obj.nested != nullptr);
      expect(obj.nested->nested != nullptr);
      expect(obj.nested->nested->value == 42);

      delete obj.nested->nested;
      delete obj.nested;
   };

   "runtime_map_with_pointer_values"_test = [] {
      std::map<std::string, simple_struct*> m;
      std::string json = R"({"first":{"x":1,"y":2,"z":3},"second":{"x":4,"y":5,"z":6}})";

      secure_context ctx;
      ctx.allocate_raw_pointers = true;

      auto ec = glz::read<glz::opts{}>(m, json, ctx);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(m.size() == 2);
      expect(m["first"]->x == 1);
      expect(m["second"]->x == 4);

      cleanup_map(m);
   };
};

// =============================================================================
// Edge Cases and Advanced Tests
// =============================================================================

suite allocate_raw_pointers_edge_cases = [] {
   "empty_vector_of_pointers"_test = [] {
      std::vector<simple_struct*> vec;
      std::string json = R"([])";

      auto ec = glz::read<alloc_opts{}>(vec, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(vec.empty());
   };

   "empty_map_with_pointer_values"_test = [] {
      std::map<std::string, simple_struct*> m;
      std::string json = R"({})";

      auto ec = glz::read<alloc_opts{}>(m, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(m.empty());
   };

   "mixed_null_and_values_in_struct"_test = [] {
      multi_pointer_struct obj;
      // Only provide int_ptr, others will be null in JSON if skipped
      std::string json = R"({"int_ptr":99})";

      auto ec = glz::read<alloc_opts{}>(obj, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(obj.int_ptr != nullptr);
      expect(*obj.int_ptr == 99);
      // Other pointers remain null because they weren't in JSON
      expect(obj.double_ptr == nullptr);
      expect(obj.string_ptr == nullptr);

      delete obj.int_ptr;
   };

   "explicit_null_in_json"_test = [] {
      multi_pointer_struct obj;
      std::string json = R"({"int_ptr":42,"double_ptr":null,"string_ptr":"test"})";

      auto ec = glz::read<alloc_opts{}>(obj, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(obj.int_ptr != nullptr);
      expect(*obj.int_ptr == 42);
      expect(obj.double_ptr == nullptr); // Explicitly null
      expect(obj.string_ptr != nullptr);
      expect(*obj.string_ptr == "test");

      delete obj.int_ptr;
      delete obj.string_ptr;
   };

   "deeply_nested_pointers"_test = [] {
      level0 obj;
      std::string json = R"({"nested":{"nested":{"value":42}}})";

      auto ec = glz::read<alloc_opts{}>(obj, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(obj.nested != nullptr);
      expect(obj.nested->nested != nullptr);
      expect(obj.nested->nested->value == 42);

      delete obj.nested->nested;
      delete obj.nested;
   };

   "vector_append_with_pointers"_test = [] {
      struct append_opts : glz::opts
      {
         bool allocate_raw_pointers = true;
         bool append_arrays = true;
      };

      std::vector<simple_struct*> vec;
      vec.push_back(new simple_struct{0, 0, 0}); // Pre-existing element

      std::string json = R"([{"x":1,"y":2,"z":3}])";
      auto ec = glz::read<append_opts{}>(vec, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(vec.size() == 2);
      expect(vec[0]->x == 0); // Original
      expect(vec[1]->x == 1); // Appended

      cleanup_vector(vec);
   };

   "large_vector_of_pointers"_test = [] {
      // Create JSON with many elements
      std::string json = "[";
      for (int i = 0; i < 100; ++i) {
         if (i > 0) json += ",";
         json += R"({"x":)" + std::to_string(i) + R"(,"y":0,"z":0})";
      }
      json += "]";

      std::vector<simple_struct*> vec;
      auto ec = glz::read<alloc_opts{}>(vec, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(vec.size() == 100);
      expect(vec[0]->x == 0);
      expect(vec[50]->x == 50);
      expect(vec[99]->x == 99);

      cleanup_vector(vec);
   };

   "bool_pointer"_test = [] {
      bool* ptr = nullptr;
      std::string json = "true";

      auto ec = glz::read<alloc_opts{}>(ptr, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(ptr != nullptr);
      expect(*ptr == true);

      delete ptr;
   };

   "vector_of_string_pointers"_test = [] {
      std::vector<std::string*> vec;
      std::string json = R"(["hello","world","test"])";

      auto ec = glz::read<alloc_opts{}>(vec, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(vec.size() == 3);
      expect(*vec[0] == "hello");
      expect(*vec[1] == "world");
      expect(*vec[2] == "test");

      for (auto* p : vec) delete p;
   };

   "map_of_int_pointers"_test = [] {
      std::map<std::string, int*> m;
      std::string json = R"({"a":1,"b":2,"c":3})";

      auto ec = glz::read<alloc_opts{}>(m, json);
      expect(ec == glz::error_code::none) << glz::format_error(ec, json);
      expect(m.size() == 3);
      expect(*m["a"] == 1);
      expect(*m["b"] == 2);
      expect(*m["c"] == 3);

      for (auto& [k, v] : m) delete v;
   };
};

int main() { return 0; }
