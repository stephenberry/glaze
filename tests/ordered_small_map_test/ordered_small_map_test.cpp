// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/containers/ordered_small_map.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ut/ut.hpp"

using namespace ut;

#if __cpp_exceptions
namespace
{
   struct throwing_value
   {
      int value = 0;

      static inline int alive = 0;
      static inline int copy_attempts = 0;
      static inline int move_attempts = 0;
      static inline int throw_on_copy = -1;
      static inline int throw_on_move = -1;

      throwing_value() { ++alive; }
      explicit throwing_value(int v) : value(v) { ++alive; }

      throwing_value(const throwing_value& other) : value(other.value)
      {
         if (throw_on_copy >= 0 && copy_attempts++ == throw_on_copy) {
            throw std::runtime_error("throwing_value copy");
         }
         ++alive;
      }

      throwing_value(throwing_value&& other) noexcept(false) : value(other.value)
      {
         if (throw_on_move >= 0 && move_attempts++ == throw_on_move) {
            throw std::runtime_error("throwing_value move");
         }
         other.value = -1;
         ++alive;
      }

      throwing_value& operator=(const throwing_value& other)
      {
         value = other.value;
         return *this;
      }

      throwing_value& operator=(throwing_value&& other) noexcept(false)
      {
         if (this != &other) {
            if (throw_on_move >= 0 && move_attempts++ == throw_on_move) {
               throw std::runtime_error("throwing_value move assign");
            }
            other.value = -1;
         }
         return *this;
      }

      ~throwing_value() { --alive; }

      static void reset_throw_controls()
      {
         copy_attempts = 0;
         move_attempts = 0;
         throw_on_copy = -1;
         throw_on_move = -1;
      }
   };
}
#endif

namespace
{
   struct tracked_try_emplace_value
   {
      int value = 0;

      static inline int direct_ctor = 0;
      static inline int copy_ctor = 0;
      static inline int move_ctor = 0;

      tracked_try_emplace_value() = delete;
      explicit tracked_try_emplace_value(int v) : value(v) { ++direct_ctor; }

      tracked_try_emplace_value(const tracked_try_emplace_value& other) : value(other.value) { ++copy_ctor; }
      tracked_try_emplace_value(tracked_try_emplace_value&& other) noexcept : value(other.value)
      {
         ++move_ctor;
         other.value = -1;
      }

      tracked_try_emplace_value& operator=(const tracked_try_emplace_value&) = default;
      tracked_try_emplace_value& operator=(tracked_try_emplace_value&&) noexcept = default;

      static void reset_counts()
      {
         direct_ctor = 0;
         copy_ctor = 0;
         move_ctor = 0;
      }
   };
}

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

   "insert_or_assign_works"_test = [] {
      glz::ordered_small_map<int> map;

      auto [it1, inserted1] = map.insert_or_assign("key", 1);
      expect(inserted1);
      expect(it1->second == 1);
      expect(map.size() == 1);

      auto [it2, inserted2] = map.insert_or_assign("key", 2);
      expect(!inserted2);
      expect(it2->second == 2);
      expect(map.size() == 1);
   };

   "insert_or_assign_preserves_order_on_assign"_test = [] {
      glz::ordered_small_map<int> map;
      map["first"] = 1;
      map["second"] = 2;

      auto [it, inserted] = map.insert_or_assign("first", 10);
      expect(!inserted);
      expect(it->second == 10);

      std::vector<std::string> keys;
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }

      expect(keys.size() == 2);
      expect(keys[0] == "first");
      expect(keys[1] == "second");
   };

   "insert_or_assign_indexed_path"_test = [] {
      glz::ordered_small_map<int> map;
      for (int i = 0; i < 32; ++i) {
         auto [it, inserted] = map.insert_or_assign("k" + std::to_string(i), i);
         expect(inserted);
         expect(it->second == i);
      }

      expect(map.contains("k10"));
      const auto original_size = map.size();

      auto [existing, inserted_existing] = map.insert_or_assign("k10", 999);
      expect(!inserted_existing);
      expect(existing->second == 999);
      expect(map.size() == original_size);

      auto [added, inserted_new] = map.insert_or_assign("k_new", 4242);
      expect(inserted_new);
      expect(added->second == 4242);
      expect(map.size() == original_size + 1);

      std::vector<std::string> keys;
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys.back() == "k_new");
   };

   "try_emplace_constructs_mapped_in_place"_test = [] {
      tracked_try_emplace_value::reset_counts();

      {
         glz::ordered_small_map<tracked_try_emplace_value> map;

         auto [it1, inserted1] = map.try_emplace("alpha", 7);
         expect(inserted1);
         expect(it1->second.value == 7);
         expect(tracked_try_emplace_value::direct_ctor == 1);
         expect(tracked_try_emplace_value::move_ctor == 0);
         expect(tracked_try_emplace_value::copy_ctor == 0);

         auto [it2, inserted2] = map.try_emplace("alpha", 22);
         expect(!inserted2);
         expect(it2->second.value == 7);
         expect(tracked_try_emplace_value::direct_ctor == 1);
         expect(tracked_try_emplace_value::move_ctor == 0);
         expect(tracked_try_emplace_value::copy_ctor == 0);
      }

      tracked_try_emplace_value::reset_counts();
   };

   "try_emplace_indexed_path"_test = [] {
      glz::ordered_small_map<int> map;
      for (int i = 0; i < 32; ++i) {
         auto [it, inserted] = map.try_emplace("i" + std::to_string(i), i);
         expect(inserted);
         expect(it->second == i);
      }

      auto [existing, inserted_existing] = map.try_emplace("i12", 777);
      expect(!inserted_existing);
      expect(existing->second == 12);

      auto [added, inserted_new] = map.try_emplace("i_new", 888);
      expect(inserted_new);
      expect(added->second == 888);
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

#if __cpp_exceptions
   "grow_if_needed_exception_safety"_test = [] {
      expect(throwing_value::alive == 0);
      throwing_value::reset_throw_controls();

      {
         glz::ordered_small_map<throwing_value> map;
         for (int i = 0; i < 4; ++i) {
            map.emplace(std::to_string(i), i);
         }

         expect(map.size() == 4);

         throwing_value::reset_throw_controls();
         throwing_value::throw_on_copy = 0;

         bool threw = false;
         try {
            map.emplace("4", 4); // triggers grow_if_needed reallocation
         }
         catch (const std::runtime_error&) {
            threw = true;
         }

         expect(threw);
         expect(map.size() == 4);
         expect(!map.contains("4"));
         for (int i = 0; i < 4; ++i) {
            expect(map.contains(std::to_string(i)));
            expect(map.at(std::to_string(i)).value == i);
         }
         expect(throwing_value::alive == static_cast<int>(map.size()));
      }

      throwing_value::reset_throw_controls();
      expect(throwing_value::alive == 0);
   };
#endif

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

#if __cpp_exceptions
   "reserve_exception_safety"_test = [] {
      expect(throwing_value::alive == 0);
      throwing_value::reset_throw_controls();

      {
         glz::ordered_small_map<throwing_value> map;
         for (int i = 0; i < 6; ++i) {
            map.emplace(std::to_string(i), i);
         }

         const auto old_capacity = map.capacity();
         expect(old_capacity >= map.size());

         throwing_value::reset_throw_controls();
         throwing_value::throw_on_copy = 0;

         bool threw = false;
         try {
            map.reserve(old_capacity + 16);
         }
         catch (const std::runtime_error&) {
            threw = true;
         }

         expect(threw);
         expect(map.capacity() == old_capacity);
         expect(map.size() == 6);
         for (int i = 0; i < 6; ++i) {
            expect(map.at(std::to_string(i)).value == i);
         }
         expect(throwing_value::alive == static_cast<int>(map.size()));
      }

      throwing_value::reset_throw_controls();
      expect(throwing_value::alive == 0);
   };

   "shrink_to_fit_exception_safety"_test = [] {
      expect(throwing_value::alive == 0);
      throwing_value::reset_throw_controls();

      {
         glz::ordered_small_map<throwing_value> map;
         for (int i = 0; i < 6; ++i) {
            map.emplace(std::to_string(i), i);
         }
         map.reserve(64);

         const auto old_capacity = map.capacity();
         expect(old_capacity > map.size());

         throwing_value::reset_throw_controls();
         throwing_value::throw_on_copy = 0;

         bool threw = false;
         try {
            map.shrink_to_fit();
         }
         catch (const std::runtime_error&) {
            threw = true;
         }

         expect(threw);
         expect(map.capacity() == old_capacity);
         expect(map.size() == 6);
         for (int i = 0; i < 6; ++i) {
            expect(map.at(std::to_string(i)).value == i);
         }
         expect(throwing_value::alive == static_cast<int>(map.size()));
      }

      throwing_value::reset_throw_controls();
      expect(throwing_value::alive == 0);
   };

   "copy_constructor_exception_safety"_test = [] {
      expect(throwing_value::alive == 0);
      throwing_value::reset_throw_controls();

      {
         glz::ordered_small_map<throwing_value> src;
         for (int i = 0; i < 5; ++i) {
            src.emplace(std::to_string(i), i);
         }

         throwing_value::reset_throw_controls();
         throwing_value::throw_on_copy = 0;

         bool threw = false;
         try {
            glz::ordered_small_map<throwing_value> copy(src);
         }
         catch (const std::runtime_error&) {
            threw = true;
         }

         expect(threw);
         expect(src.size() == 5);
         for (int i = 0; i < 5; ++i) {
            expect(src.at(std::to_string(i)).value == i);
         }
         expect(throwing_value::alive == static_cast<int>(src.size()));
      }

      throwing_value::reset_throw_controls();
      expect(throwing_value::alive == 0);
   };

   "copy_assignment_exception_safety"_test = [] {
      expect(throwing_value::alive == 0);
      throwing_value::reset_throw_controls();

      {
         glz::ordered_small_map<throwing_value> src;
         for (int i = 0; i < 5; ++i) {
            src.emplace("src_" + std::to_string(i), i);
         }

         glz::ordered_small_map<throwing_value> dst;
         dst.emplace("dst_a", 100);
         dst.emplace("dst_b", 200);

         throwing_value::reset_throw_controls();
         throwing_value::throw_on_copy = 0;

         bool threw = false;
         try {
            dst = src;
         }
         catch (const std::runtime_error&) {
            threw = true;
         }

         expect(threw);
         expect(dst.size() == 2);
         expect(dst.at("dst_a").value == 100);
         expect(dst.at("dst_b").value == 200);
         expect(src.size() == 5);
         for (int i = 0; i < 5; ++i) {
            expect(src.at("src_" + std::to_string(i)).value == i);
         }
         expect(throwing_value::alive == static_cast<int>(src.size() + dst.size()));
      }

      throwing_value::reset_throw_controls();
      expect(throwing_value::alive == 0);
   };
#endif

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
