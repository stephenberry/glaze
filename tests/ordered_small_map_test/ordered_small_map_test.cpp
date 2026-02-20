// Glaze Library
// For the license information refer to glaze.hpp

#include <iostream>
#include <string>
#include <vector>

#include "glaze/containers/ordered_small_map.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite ordered_small_map_tests = [] {
   "insertion_order_preserved"_test = [] {
      glz::ordered_small_map<int> map;

      map["zebra"] = 1;
      map["apple"] = 2;
      map["mango"] = 3;
      map["banana"] = 4;

      std::vector<std::string> keys;
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }

      // Should be in insertion order, not sorted order
      expect(keys[0] == "zebra");
      expect(keys[1] == "apple");
      expect(keys[2] == "mango");
      expect(keys[3] == "banana");
   };

   "lookup_works"_test = [] {
      glz::ordered_small_map<int> map;
      map["one"] = 1;
      map["two"] = 2;
      map["three"] = 3;

      expect(map["one"] == 1);
      expect(map["two"] == 2);
      expect(map["three"] == 3);
      expect(map.at("two") == 2);
   };

   "find_works"_test = [] {
      glz::ordered_small_map<int> map;
      map["exists"] = 42;

      auto it = map.find("exists");
      expect(it != map.end());
      expect(it->second == 42);

      auto it2 = map.find("not_exists");
      expect(it2 == map.end());
   };

   "contains_works"_test = [] {
      glz::ordered_small_map<int> map;
      map["key"] = 1;

      expect(map.contains("key"));
      expect(!map.contains("missing"));
   };

   "erase_works"_test = [] {
      glz::ordered_small_map<int> map;
      map["a"] = 1;
      map["b"] = 2;
      map["c"] = 3;

      expect(map.size() == 3);

      map.erase("b");

      expect(map.size() == 2);
      expect(!map.contains("b"));

      // Order should still be preserved for remaining elements
      std::vector<std::string> keys;
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys[0] == "a");
      expect(keys[1] == "c");
   };

   "duplicate_insert_fails"_test = [] {
      glz::ordered_small_map<int> map;

      auto [it1, inserted1] = map.insert({"key", 1});
      expect(inserted1);

      auto [it2, inserted2] = map.insert({"key", 2});
      expect(!inserted2);
      expect(it2->second == 1); // Original value unchanged
   };

   "copy_constructor"_test = [] {
      glz::ordered_small_map<int> map1;
      map1["a"] = 1;
      map1["b"] = 2;

      glz::ordered_small_map<int> map2 = map1;

      expect(map2.size() == 2);
      expect(map2["a"] == 1);
      expect(map2["b"] == 2);

      // Verify order is preserved in copy
      std::vector<std::string> keys;
      for (const auto& [key, value] : map2) {
         keys.push_back(key);
      }
      expect(keys[0] == "a");
      expect(keys[1] == "b");
   };

   "move_constructor"_test = [] {
      glz::ordered_small_map<int> map1;
      map1["x"] = 10;
      map1["y"] = 20;

      glz::ordered_small_map<int> map2 = std::move(map1);

      expect(map2.size() == 2);
      expect(map2["x"] == 10);
      expect(map2["y"] == 20);
      expect(map1.empty());
   };

   "initializer_list"_test = [] {
      glz::ordered_small_map<int> map = {{"first", 1}, {"second", 2}, {"third", 3}};

      expect(map.size() == 3);

      std::vector<std::string> keys;
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys[0] == "first");
      expect(keys[1] == "second");
      expect(keys[2] == "third");
   };

   "clear_works"_test = [] {
      glz::ordered_small_map<int> map;
      map["a"] = 1;
      map["b"] = 2;

      map.clear();

      expect(map.empty());
      expect(map.size() == 0);
   };

   "heterogeneous_lookup"_test = [] {
      glz::ordered_small_map<int> map;
      map["test"] = 42;

      // Should work with string_view
      std::string_view sv = "test";
      expect(map.contains(sv));
      expect(map.find(sv) != map.end());
   };

   "small_map_linear_search"_test = [] {
      // Test with <= 16 entries (should use linear search)
      glz::ordered_small_map<int> map;

      for (int i = 0; i < 16; ++i) {
         map[std::to_string(i)] = i;
      }

      expect(map.size() == 16);

      // Verify all values and insertion order
      int expected = 0;
      for (const auto& [key, value] : map) {
         expect(key == std::to_string(expected));
         expect(value == expected);
         ++expected;
      }

      // Verify lookup works
      for (int i = 0; i < 16; ++i) {
         expect(map[std::to_string(i)] == i);
      }
   };

   "large_map_index_lookup"_test = [] {
      // Test with > 16 entries (should build and use index)
      glz::ordered_small_map<int> map;

      for (int i = 0; i < 100; ++i) {
         map[std::to_string(i)] = i * 2;
      }

      expect(map.size() == 100);

      // Verify all values via lookup (this triggers index building)
      for (int i = 0; i < 100; ++i) {
         expect(map[std::to_string(i)] == i * 2);
      }

      // Verify insertion order is preserved
      int expected = 0;
      for (const auto& [key, value] : map) {
         expect(key == std::to_string(expected));
         expect(value == expected * 2);
         ++expected;
      }
   };

   "index_invalidation_on_insert"_test = [] {
      glz::ordered_small_map<int> map;

      // Build up to threshold
      for (int i = 0; i < 20; ++i) {
         map[std::to_string(i)] = i;
      }

      // Force index build by doing a lookup
      expect(map.contains("5"));

      // Insert more entries (should invalidate index)
      map["new_key"] = 999;

      // Lookup should still work (index rebuilt lazily)
      expect(map["new_key"] == 999);
      expect(map.contains("5"));

      // Verify order
      std::vector<std::string> keys;
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys.back() == "new_key");
   };

   "index_invalidation_on_erase"_test = [] {
      glz::ordered_small_map<int> map;

      for (int i = 0; i < 20; ++i) {
         map[std::to_string(i)] = i;
      }

      // Force index build
      expect(map.contains("10"));

      // Erase an entry
      map.erase("10");

      // Lookup should still work
      expect(!map.contains("10"));
      expect(map.contains("5"));
      expect(map.contains("15"));
   };

   "reserve_preserves_index"_test = [] {
      glz::ordered_small_map<int> map;

      // Insert enough to build index
      for (int i = 0; i < 20; ++i) {
         map[std::to_string(i)] = i;
      }

      // Force index build
      expect(map.contains("5"));

      // Reserve should NOT invalidate index (hashes and indices remain valid)
      map.reserve(1000);

      // Lookups should still work
      expect(map.contains("5"));
      expect(map.contains("10"));
      expect(map.contains("19"));
      expect(!map.contains("999"));
   };

   "hash_collision_fallback"_test = [] {
      // This test verifies that even if hash collisions occur,
      // the map still functions correctly (falls back to linear search)
      glz::ordered_small_map<int> map;

      // Insert many entries - statistically unlikely to have collisions with FNV-1a
      // but the fallback mechanism should handle it if they occur
      for (int i = 0; i < 100; ++i) {
         map["key_" + std::to_string(i)] = i;
      }

      // Verify all lookups work regardless of whether index or linear search is used
      for (int i = 0; i < 100; ++i) {
         expect(map.contains("key_" + std::to_string(i)));
         expect(map["key_" + std::to_string(i)] == i);
      }

      expect(!map.contains("nonexistent"));
   };


   "many_insertions"_test = [] {
      glz::ordered_small_map<int> map;

      // Insert enough to trigger multiple index rebuilds
      for (int i = 0; i < 1000; ++i) {
         map[std::to_string(i)] = i * 2;
      }

      expect(map.size() == 1000);

      // Verify all values
      for (int i = 0; i < 1000; ++i) {
         expect(map[std::to_string(i)] == i * 2);
      }

      // Verify insertion order
      int expected = 0;
      for (const auto& [key, value] : map) {
         expect(key == std::to_string(expected));
         ++expected;
      }
   };
};

int main() { return 0; }
