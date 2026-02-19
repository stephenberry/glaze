// Glaze Library
// For the license information refer to glaze.hpp

#include <string>
#include <vector>

#include "glaze/containers/ordered_dict.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite ordered_dict_tests = [] {
   "insertion_order_preserved"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["zebra"] = 1;
      d["apple"] = 2;
      d["mango"] = 3;
      d["banana"] = 4;

      std::vector<std::string> keys;
      for (const auto& [key, value] : d) {
         keys.push_back(key);
      }

      expect(keys.size() == 4);
      expect(keys[0] == "zebra");
      expect(keys[1] == "apple");
      expect(keys[2] == "mango");
      expect(keys[3] == "banana");
   };

   "basic_insert_and_find"_test = [] {
      glz::ordered_dict<std::string, int> d;
      auto [it1, inserted1] = d.insert({"one", 1});
      expect(inserted1);
      expect(it1->first == "one");
      expect(it1->second == 1);

      auto [it2, inserted2] = d.insert({"one", 99});
      expect(!inserted2);
      expect(it2->second == 1); // not overwritten

      expect(d.size() == 1);
   };

   "operator_bracket"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 10;
      d["b"] = 20;
      d["c"] = 30;

      expect(d["a"] == 10);
      expect(d["b"] == 20);
      expect(d["c"] == 30);
      expect(d.size() == 3);

      d["a"] = 100;
      expect(d["a"] == 100);
      expect(d.size() == 3);
   };

   "at_works"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["exists"] = 42;

      expect(d.at("exists") == 42);
   };

   "find_works"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["key"] = 42;

      auto it = d.find("key");
      expect(it != d.end());
      expect(it->second == 42);

      auto it2 = d.find("missing");
      expect(it2 == d.end());
   };

   "contains_and_count"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;

      expect(d.contains("a"));
      expect(!d.contains("c"));
      expect(d.count("b") == 1);
      expect(d.count("z") == 0);
   };

   "ordered_erase_by_iterator"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;
      d["c"] = 3;
      d["d"] = 4;

      // Erase "b"
      auto it = d.find("b");
      d.erase(it);

      expect(d.size() == 3);
      expect(!d.contains("b"));

      // Order preserved: a, c, d
      std::vector<std::string> keys;
      for (const auto& [k, v] : d) keys.push_back(k);
      expect(keys[0] == "a");
      expect(keys[1] == "c");
      expect(keys[2] == "d");

      // Remaining elements still findable
      expect(d.at("a") == 1);
      expect(d.at("c") == 3);
      expect(d.at("d") == 4);
   };

   "ordered_erase_by_key"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["x"] = 10;
      d["y"] = 20;
      d["z"] = 30;

      expect(d.erase("y") == 1);
      expect(d.erase("missing") == 0);
      expect(d.size() == 2);
      expect(!d.contains("y"));
      expect(d.at("x") == 10);
      expect(d.at("z") == 30);
   };

   "ordered_erase_range"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;
      d["c"] = 3;
      d["d"] = 4;
      d["e"] = 5;

      // Erase b, c, d (indices 1..4)
      auto first = d.nth(1);
      auto last = d.nth(4);
      d.erase(first, last);

      expect(d.size() == 2);
      expect(d.at("a") == 1);
      expect(d.at("e") == 5);
   };

   "unordered_erase"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;
      d["c"] = 3;

      d.unordered_erase(d.find("a"));

      expect(d.size() == 2);
      expect(!d.contains("a"));
      // "c" was moved to position 0 (swapped with last), "b" stays at position 1
      expect(d.contains("b"));
      expect(d.contains("c"));
      expect(d.at("b") == 2);
      expect(d.at("c") == 3);
   };

   "unordered_erase_by_key"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["x"] = 10;
      d["y"] = 20;
      d["z"] = 30;

      expect(d.unordered_erase("y") == 1);
      expect(d.unordered_erase("missing") == 0);
      expect(d.size() == 2);
      expect(d.contains("x"));
      expect(d.contains("z"));
   };

   "insert_or_assign"_test = [] {
      glz::ordered_dict<std::string, int> d;

      auto [it1, ins1] = d.insert_or_assign("key", 10);
      expect(ins1);
      expect(it1->second == 10);

      auto [it2, ins2] = d.insert_or_assign("key", 20);
      expect(!ins2);
      expect(it2->second == 20);

      expect(d.size() == 1);
      expect(d.at("key") == 20);
   };

   "try_emplace"_test = [] {
      glz::ordered_dict<std::string, int> d;

      auto [it1, ins1] = d.try_emplace("key", 10);
      expect(ins1);
      expect(it1->second == 10);

      auto [it2, ins2] = d.try_emplace("key", 99);
      expect(!ins2);
      expect(it2->second == 10); // not overwritten

      expect(d.size() == 1);
   };

   "emplace"_test = [] {
      glz::ordered_dict<std::string, int> d;

      auto [it, ins] = d.emplace("hello", 42);
      expect(ins);
      expect(it->first == "hello");
      expect(it->second == 42);

      auto [it2, ins2] = d.emplace("hello", 99);
      expect(!ins2);
      expect(it2->second == 42);
   };

   "copy_constructor"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;
      d["c"] = 3;

      glz::ordered_dict<std::string, int> d2(d);

      expect(d2.size() == 3);
      expect(d2.at("a") == 1);
      expect(d2.at("b") == 2);
      expect(d2.at("c") == 3);

      // Verify order preserved
      auto it = d2.begin();
      expect(it->first == "a");
      ++it;
      expect(it->first == "b");
      ++it;
      expect(it->first == "c");
   };

   "move_constructor"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["x"] = 10;
      d["y"] = 20;

      glz::ordered_dict<std::string, int> d2(std::move(d));

      expect(d2.size() == 2);
      expect(d2.at("x") == 10);
      expect(d2.at("y") == 20);
      expect(d.empty()); // NOLINT: moved-from state
   };

   "copy_assignment"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;

      glz::ordered_dict<std::string, int> d2;
      d2["z"] = 99;
      d2 = d;

      expect(d2.size() == 1);
      expect(d2.at("a") == 1);
      expect(!d2.contains("z"));
   };

   "move_assignment"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;

      glz::ordered_dict<std::string, int> d2;
      d2 = std::move(d);

      expect(d2.size() == 1);
      expect(d2.at("a") == 1);
   };

   "initializer_list_constructor"_test = [] {
      glz::ordered_dict<std::string, int> d{{"a", 1}, {"b", 2}, {"c", 3}};

      expect(d.size() == 3);
      expect(d.at("a") == 1);
      expect(d.at("b") == 2);
      expect(d.at("c") == 3);

      // Duplicates in initializer list should be ignored
      glz::ordered_dict<std::string, int> d2{{"x", 1}, {"x", 2}, {"y", 3}};
      expect(d2.size() == 2);
      expect(d2.at("x") == 1);
      expect(d2.at("y") == 3);
   };

   "initializer_list_assignment"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["old"] = 99;
      d = {{"a", 1}, {"b", 2}};

      expect(d.size() == 2);
      expect(!d.contains("old"));
      expect(d.at("a") == 1);
      expect(d.at("b") == 2);
   };

   "range_constructor"_test = [] {
      std::vector<std::pair<std::string, int>> vec{{"a", 1}, {"b", 2}, {"c", 3}};
      glz::ordered_dict<std::string, int> d(vec.begin(), vec.end());

      expect(d.size() == 3);
      expect(d.at("a") == 1);
      expect(d.at("c") == 3);
   };

   "clear"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;

      d.clear();
      expect(d.empty());
      expect(d.size() == 0);

      // Should be able to insert again
      d["c"] = 3;
      expect(d.size() == 1);
      expect(d.at("c") == 3);
   };

   "front_and_back"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["first"] = 1;
      d["second"] = 2;
      d["third"] = 3;

      expect(d.front().first == "first");
      expect(d.front().second == 1);
      expect(d.back().first == "third");
      expect(d.back().second == 3);
   };

   "nth"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;
      d["c"] = 3;

      expect(d.nth(0)->first == "a");
      expect(d.nth(1)->first == "b");
      expect(d.nth(2)->first == "c");
      expect(d.nth(3) == d.end());
   };

   "equal_range"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["key"] = 42;

      auto [first, last] = d.equal_range("key");
      expect(first != d.end());
      expect(std::distance(first, last) == 1);
      expect(first->second == 42);

      auto [f2, l2] = d.equal_range("missing");
      expect(f2 == d.end());
      expect(l2 == d.end());
   };

   "reverse_iterators"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;
      d["c"] = 3;

      std::vector<std::string> keys;
      for (auto it = d.rbegin(); it != d.rend(); ++it) {
         keys.push_back(it->first);
      }
      expect(keys[0] == "c");
      expect(keys[1] == "b");
      expect(keys[2] == "a");
   };

   "rehash_and_reserve"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d.reserve(100);

      expect(d.bucket_count() >= 100);

      for (int i = 0; i < 100; ++i) {
         d[std::to_string(i)] = i;
      }

      expect(d.size() == 100);
      for (int i = 0; i < 100; ++i) {
         expect(d.at(std::to_string(i)) == i);
      }
   };

   "load_factor"_test = [] {
      glz::ordered_dict<std::string, int> d;
      expect(d.load_factor() == 0.0f);
      expect(d.max_load_factor() == 0.75f);

      d["a"] = 1;
      expect(d.load_factor() > 0.0f);

      d.max_load_factor(0.5f);
      expect(d.max_load_factor() == 0.5f);
   };

   "swap"_test = [] {
      glz::ordered_dict<std::string, int> d1{{"a", 1}};
      glz::ordered_dict<std::string, int> d2{{"b", 2}, {"c", 3}};

      d1.swap(d2);

      expect(d1.size() == 2);
      expect(d1.contains("b"));
      expect(d2.size() == 1);
      expect(d2.contains("a"));
   };

   "observers"_test = [] {
      glz::ordered_dict<std::string, int> d;
      [[maybe_unused]] auto h = d.hash_function();
      [[maybe_unused]] auto eq = d.key_eq();
   };

   "comparison"_test = [] {
      glz::ordered_dict<std::string, int> d1{{"a", 1}, {"b", 2}};
      glz::ordered_dict<std::string, int> d2{{"a", 1}, {"b", 2}};
      glz::ordered_dict<std::string, int> d3{{"b", 2}, {"a", 1}};

      expect(d1 == d2);
      expect(d1 != d3); // different order
   };

   "integer_keys"_test = [] {
      glz::ordered_dict<int, std::string> d;
      d[42] = "answer";
      d[7] = "lucky";
      d[13] = "unlucky";

      expect(d.size() == 3);
      expect(d.at(42) == "answer");
      expect(d.at(7) == "lucky");

      // Order preserved
      expect(d.front().first == 42);
      expect(d.back().first == 13);
   };

   "large_map_stress"_test = [] {
      glz::ordered_dict<int, int> d;
      constexpr int N = 10000;

      for (int i = 0; i < N; ++i) {
         d[i] = i * 2;
      }

      expect(d.size() == N);

      // All elements findable
      for (int i = 0; i < N; ++i) {
         expect(d.at(i) == i * 2);
      }

      // Order preserved
      int idx = 0;
      for (const auto& [k, v] : d) {
         expect(k == idx);
         expect(v == idx * 2);
         ++idx;
      }

      // Erase half (unordered, for speed)
      for (int i = 0; i < N; i += 2) {
         d.unordered_erase(i);
      }

      expect(d.size() == N / 2);

      // Remaining odd elements still findable
      for (int i = 1; i < N; i += 2) {
         expect(d.contains(i));
         expect(d.at(i) == i * 2);
      }
   };

   "erase_all_and_reinsert"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;
      d["c"] = 3;

      d.erase(d.begin(), d.end());
      expect(d.empty());

      d["d"] = 4;
      expect(d.size() == 1);
      expect(d.at("d") == 4);
   };

   "data_pointer"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;

      auto* ptr = d.data();
      expect(ptr[0].first == "a");
      expect(ptr[0].second == 1);
      expect(ptr[1].first == "b");
      expect(ptr[1].second == 2);
   };

   "empty_operations"_test = [] {
      glz::ordered_dict<std::string, int> d;
      expect(d.empty());
      expect(d.size() == 0);
      expect(d.find("x") == d.end());
      expect(!d.contains("x"));
      expect(d.count("x") == 0);
      expect(d.erase("x") == 0);
      expect(d.begin() == d.end());
   };

   "insert_move_semantics"_test = [] {
      glz::ordered_dict<std::string, std::string> d;
      std::string key = "key";
      std::string val = "value";

      d.insert({std::move(key), std::move(val)});
      expect(d.at("key") == "value");
   };

   "many_collisions"_test = [] {
      // Use a hash that always returns the same value to force maximum collisions
      struct bad_hash
      {
         size_t operator()(int) const { return 42; }
      };

      glz::ordered_dict<int, int, bad_hash> d;
      for (int i = 0; i < 50; ++i) {
         d[i] = i;
      }

      expect(d.size() == 50);
      for (int i = 0; i < 50; ++i) {
         expect(d.at(i) == i);
      }

      // Erase some
      for (int i = 0; i < 50; i += 3) {
         d.erase(d.find(i));
      }

      // Remaining elements still correct
      for (int i = 0; i < 50; ++i) {
         if (i % 3 == 0) {
            expect(!d.contains(i));
         }
         else {
            expect(d.at(i) == i);
         }
      }
   };

   "shrink_to_fit"_test = [] {
      glz::ordered_dict<int, int> d;
      d.reserve(1000);
      auto bc_before = d.bucket_count();

      d[1] = 1;
      d[2] = 2;

      d.shrink_to_fit();
      expect(d.bucket_count() <= bc_before);
      expect(d.at(1) == 1);
      expect(d.at(2) == 2);
   };

   "max_size"_test = [] {
      glz::ordered_dict<int, int> d;
      expect(d.max_size() > 0);
   };

   "capacity"_test = [] {
      glz::ordered_dict<int, int> d;
      d.reserve(100);
      expect(d.capacity() >= 100);
   };

   "values_container"_test = [] {
      glz::ordered_dict<std::string, int> d;
      d["a"] = 1;
      d["b"] = 2;

      const auto& vals = d.values();
      expect(vals.size() == 2);
      expect(vals[0].first == "a");
      expect(vals[1].first == "b");
   };
};

int main() { return 0; }
