// Glaze Library
// For the license information refer to glaze.hpp
//
// glz::pmr::generic is an allocator-aware type where the entire tree
// uses a single memory resource. Use parentheses (val) or assignment = val
// for single values.

#include <array>
#include <memory_resource>

#include "glaze/json/generic_pmr.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite pmr_generic_basic_tests = [] {
   "basic_construction"_test = [] {
      glz::pmr::generic_f64 json;
      expect(json.is_null());
   };

   "construction_with_resource"_test = [] {
      std::array<std::byte, 1024> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);
      expect(json.is_null());
      expect(json.resource() == &mbr);
   };

   "assign_double"_test = [] {
      glz::pmr::generic_f64 json;
      json = 42.0;
      expect(json.is_number());
      expect(json.get<double>() == 42.0);
   };

   "assign_int"_test = [] {
      glz::pmr::generic_f64 json;
      json = 42;
      expect(json.is_number());
      expect(json.get<double>() == 42.0);
   };

   "assign_bool"_test = [] {
      glz::pmr::generic_f64 json;
      json = true;
      expect(json.is_boolean());
      expect(json.get<bool>() == true);
   };

   "assign_string"_test = [] {
      glz::pmr::generic_f64 json;
      json = "hello";
      expect(json.is_string());
      expect(json.get_string() == "hello");
   };

   "assign_nullptr"_test = [] {
      glz::pmr::generic_f64 json;
      json = 42;
      json = nullptr;
      expect(json.is_null());
   };

   "object_subscript"_test = [] {
      glz::pmr::generic_f64 json;
      json["key"] = 123;
      expect(json.is_object());
      expect(json["key"].get<double>() == 123.0);
   };

   "object_nested_subscript"_test = [] {
      glz::pmr::generic_f64 json;
      json["user"]["name"] = "Alice";
      expect(json.is_object());
      expect(json["user"].is_object());
      expect(json["user"]["name"].get_string() == "Alice");
   };

   "array_push_back"_test = [] {
      std::array<std::byte, 4096> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);
      json.push_back(glz::pmr::generic_f64(1, &mbr));
      json.push_back(glz::pmr::generic_f64(2, &mbr));

      expect(json.is_array());
      expect(json.size() == 2);
      expect(json[0].get<double>() == 1.0);
      expect(json[1].get<double>() == 2.0);
   };

   "array_emplace_back"_test = [] {
      std::array<std::byte, 4096> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);
      json.emplace_back(10);
      json.emplace_back(20);

      expect(json.is_array());
      expect(json.size() == 2);
      expect(json[0].get<double>() == 10.0);
      expect(json[1].get<double>() == 20.0);
   };

   "contains"_test = [] {
      glz::pmr::generic_f64 json;
      json["existing_key"] = "value";

      expect(json.contains("existing_key"));
      expect(!json.contains("nonexistent_key"));
   };

   "empty_and_size"_test = [] {
      glz::pmr::generic_f64 json;
      expect(json.empty());

      json["key"] = "value";
      expect(!json.empty());
      expect(json.size() == 1);
   };

   "clear"_test = [] {
      glz::pmr::generic_f64 json;
      json["a"] = 1;
      json["b"] = 2;
      expect(json.size() == 2);

      json.clear();
      expect(json.empty());
   };

   "reset"_test = [] {
      glz::pmr::generic_f64 json;
      json["key"] = "value";
      expect(json.is_object());

      json.reset();
      expect(json.is_null());
   };
};

suite pmr_generic_stack_buffer_tests = [] {
   "single_resource_entire_tree"_test = [] {
      std::array<std::byte, 16384> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);

      // All internal allocations use the same resource
      json["name"] = "Alice";
      json["age"] = 30;
      json["scores"].push_back(glz::pmr::generic_f64(95, &mbr));
      json["scores"].push_back(glz::pmr::generic_f64(87, &mbr));
      json["address"]["city"] = "NYC";
      json["address"]["zip"] = "10001";

      expect(json.is_object());
      expect(json["name"].get_string() == "Alice");
      expect(json["age"].get<double>() == 30.0);
      expect(json["scores"].is_array());
      expect(json["scores"].size() == 2);
      expect(json["address"]["city"].get_string() == "NYC");
   };

   "resource_propagation"_test = [] {
      std::array<std::byte, 8192> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);

      // Verify resource is propagated
      expect(json.resource() == &mbr);

      // operator[] creates children with same resource
      json["child"]["grandchild"] = "value";

      expect(json.is_object());
      expect(json["child"].is_object());
      expect(json["child"]["grandchild"].get_string() == "value");
   };

   "string_allocation"_test = [] {
      std::array<std::byte, 8192> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);

      // Long strings that definitely need heap allocation
      json["short"] = "hi";
      json["long"] = "This is a much longer string that exceeds SSO buffer size";

      expect(json["short"].get_string() == "hi");
      expect(json["long"].get_string() == "This is a much longer string that exceeds SSO buffer size");
   };

   "copy_with_allocator"_test = [] {
      std::array<std::byte, 8192> buffer1{};
      std::array<std::byte, 8192> buffer2{};
      std::pmr::monotonic_buffer_resource mbr1(buffer1.data(), buffer1.size());
      std::pmr::monotonic_buffer_resource mbr2(buffer2.data(), buffer2.size());

      glz::pmr::generic_f64 json1(&mbr1);
      json1["key"] = "value";
      json1["nested"]["data"] = 42;

      // Copy to different allocator
      glz::pmr::generic_f64 json2(json1, glz::pmr::generic_f64::allocator_type(&mbr2));

      expect(json2.resource() == &mbr2);
      expect(json2["key"].get_string() == "value");
      expect(json2["nested"]["data"].get<double>() == 42.0);
   };

   "deeply_nested"_test = [] {
      std::array<std::byte, 32768> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);

      // Create deeply nested structure - all using same resource
      auto* current = &json;
      for (int i = 0; i < 10; ++i) {
         (*current)["level"] = i;
         current = &((*current)["child"]);
      }
      *current = "leaf";

      // Verify structure
      expect(json["level"].get<double>() == 0.0);
      expect(json["child"]["level"].get<double>() == 1.0);
      expect(json["child"]["child"]["level"].get<double>() == 2.0);
   };

   "mixed_content"_test = [] {
      std::array<std::byte, 16384> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_f64 json(&mbr);

      json["null_val"] = nullptr;
      json["bool_val"] = true;
      json["int_val"] = 42;
      json["double_val"] = 3.14159;
      json["string_val"] = "hello";
      json["array_val"].push_back(glz::pmr::generic_f64(1, &mbr));
      json["array_val"].push_back(glz::pmr::generic_f64(2, &mbr));
      json["object_val"]["nested"] = "value";

      expect(json["null_val"].is_null());
      expect(json["bool_val"].get<bool>() == true);
      expect(json["int_val"].get<double>() == 42.0);
      expect(json["double_val"].get<double>() == 3.14159);
      expect(json["string_val"].get_string() == "hello");
      expect(json["array_val"].size() == 2);
      expect(json["object_val"]["nested"].get_string() == "value");
   };

   "buffer_reuse"_test = [] {
      std::array<std::byte, 4096> buffer{};

      // First use of the buffer
      {
         std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());
         glz::pmr::generic_f64 json(&mbr);
         json["data"] = "first";
         expect(json["data"].get_string() == "first");
      }

      // Reuse the same buffer for a new allocation
      {
         std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());
         glz::pmr::generic_f64 json(&mbr);
         json["data"] = "second";
         expect(json["data"].get_string() == "second");
      }
   };

   "with_pool_resource"_test = [] {
      std::array<std::byte, 8192> buffer{};
      std::pmr::monotonic_buffer_resource upstream(buffer.data(), buffer.size());
      std::pmr::unsynchronized_pool_resource pool(&upstream);

      glz::pmr::generic_f64 json(&pool);

      for (int i = 0; i < 10; ++i) {
         json.emplace_back(static_cast<double>(i));
      }

      expect(json.size() == 10);
      expect(json[9].get<double>() == 9.0);
   };

   "small_buffer_with_fallback"_test = [] {
      // Small stack buffer with heap fallback for overflow
      std::array<std::byte, 256> small_buffer{};
      std::pmr::monotonic_buffer_resource mbr(small_buffer.data(), small_buffer.size(),
                                               std::pmr::get_default_resource());

      glz::pmr::generic_f64 json(&mbr);

      // Add enough data to potentially overflow the small buffer
      for (int i = 0; i < 20; ++i) {
         json.emplace_back(static_cast<double>(i));
      }

      expect(json.size() == 20);
      expect(json[0].get<double>() == 0.0);
      expect(json[19].get<double>() == 19.0);
   };
};

suite pmr_generic_i64_u64_tests = [] {
   "i64_mode"_test = [] {
      std::array<std::byte, 4096> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_i64 json(&mbr);
      json["big_int"] = 9223372036854775807LL;

      expect(json["big_int"].holds<int64_t>());
      expect(json["big_int"].get<int64_t>() == 9223372036854775807LL);
   };

   "u64_mode"_test = [] {
      std::array<std::byte, 4096> buffer{};
      std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());

      glz::pmr::generic_u64 json(&mbr);

      // Test unsigned int assignment
      json["small"] = 1000000u;
      expect(json["small"].holds<uint64_t>());
      expect(json["small"].get<uint64_t>() == 1000000u);

      // Test unsigned long long with large value
      json["big"] = 18446744073709551615ULL; // UINT64_MAX
      expect(json["big"].holds<uint64_t>());
      expect(json["big"].get<uint64_t>() == 18446744073709551615ULL);
   };

};

int main() { return 0; }
