// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/containers/flat_map.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ut/ut.hpp"

using namespace ut;

suite flat_map_construction_tests = [] {
   "default constructed map is empty"_test = [] {
      glz::flat_map<std::string, int> map{};

      expect(map.empty());
      expect(map.size() == 0);
      expect(map.begin() == map.end());
   };

   "initializer list is stored in key order"_test = [] {
      glz::flat_map<std::string, int> map{{"c", 3}, {"a", 1}, {"b", 2}};

      expect(map.size() == 3);

      std::vector<std::string> keys{};
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys == std::vector<std::string>{"a", "b", "c"});
   };

   "duplicate key in an initializer list keeps the first value"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}, {"a", 2}};

      expect(map.size() == 1);
      expect(map.at("a") == 1);
   };

   "iterator range is accepted"_test = [] {
      const std::vector<std::pair<std::string, int>> input{{"b", 2}, {"a", 1}};
      glz::flat_map<std::string, int> map{input.begin(), input.end()};

      expect(map.size() == 2);
      expect(map.begin()->first == "a");
   };

   "copy and move preserve the contents"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}, {"b", 2}};

      const glz::flat_map<std::string, int> copied{map};
      expect(copied.size() == 2);
      expect(copied.at("b") == 2);

      const glz::flat_map<std::string, int> moved{std::move(map)};
      expect(moved.size() == 2);
      expect(moved.at("b") == 2);
   };

   "copy and move assignment preserve the contents"_test = [] {
      glz::flat_map<std::string, int> source{{"a", 1}};
      glz::flat_map<std::string, int> target{{"z", 26}};

      target = source;
      expect(target.size() == 1);
      expect(target.at("a") == 1);

      target = glz::flat_map<std::string, int>{{"b", 2}};
      expect(target.size() == 1);
      expect(target.at("b") == 2);
   };
};

suite flat_map_modifier_tests = [] {
   "insert reports whether the key was new"_test = [] {
      glz::flat_map<std::string, int> map{};

      const auto [first, inserted] = map.insert({"a", 1});
      expect(inserted);
      expect(first->second == 1);

      const auto [second, reinserted] = map.insert({"a", 2});
      expect(not reinserted);
      expect(second->second == 1) << "insert must not overwrite an existing value";
      expect(map.size() == 1);
   };

   "insert accepts an lvalue"_test = [] {
      glz::flat_map<std::string, int> map{};
      const std::pair<std::string, int> value{"a", 1};

      expect(map.insert(value).second);
      expect(map.at("a") == 1);
   };

   "insert of a range keeps the map sorted"_test = [] {
      glz::flat_map<int, int> map{};
      map.insert({{5, 50}, {1, 10}, {3, 30}});

      std::vector<int> keys{};
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys == std::vector<int>{1, 3, 5});
   };

   "emplace inserts and does not overwrite"_test = [] {
      glz::flat_map<std::string, int> map{};

      expect(map.emplace("a", 1).second);
      expect(not map.emplace("a", 2).second);
      expect(map.at("a") == 1);
   };

   "erase by key reports how many were removed"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}, {"b", 2}};

      expect(map.erase("a") == 1);
      expect(map.erase("a") == 0);
      expect(map.size() == 1);
      expect(map.begin()->first == "b");
   };

   "erase accepts a heterogeneous key"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}, {"b", 2}};

      expect(map.erase(std::string_view{"a"}) == 1);
      expect(map.erase(std::string_view{"a"}) == 0);
      expect(map.size() == 1);
      expect(map.begin()->first == "b");
   };

   "erase by iterator returns the following element"_test = [] {
      glz::flat_map<int, int> map{{1, 10}, {2, 20}, {3, 30}};

      const auto next = map.erase(map.begin());
      expect(next->first == 2);
      expect(map.size() == 2);
   };

   "erase by const_iterator returns the following element"_test = [] {
      glz::flat_map<int, int> map{{1, 10}, {2, 20}, {3, 30}};

      const auto next = map.erase(map.cbegin());
      expect(next->first == 2);
      expect(map.size() == 2);
   };

   "erase by range removes the whole span"_test = [] {
      glz::flat_map<int, int> map{{1, 10}, {2, 20}, {3, 30}};

      const auto next = map.erase(map.begin(), map.begin() + 2);
      expect(next->first == 3);
      expect(map.size() == 1);
   };

   "clear empties the map"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      map.clear();
      expect(map.empty());
      expect(not map.contains("a"));
   };

   "swap exchanges the contents"_test = [] {
      glz::flat_map<std::string, int> left{{"a", 1}};
      glz::flat_map<std::string, int> right{{"b", 2}, {"c", 3}};

      left.swap(right);
      expect(left.size() == 2);
      expect(right.size() == 1);
      expect(left.contains("b"));
      expect(right.contains("a"));

      glz::swap(left, right);
      expect(left.size() == 1);
      expect(left.contains("a"));
   };
};

suite flat_map_lookup_tests = [] {
   "find returns the entry for a present key and end for an absent one"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}, {"b", 2}};

      const auto it = map.find("b");
      expect(it != map.end());
      expect(it->second == 2);
      expect(map.find("z") == map.end());
   };

   "find on a non-const map exposes a mutable value"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      map.find("a")->second = 42;
      expect(map.at("a") == 42);
   };

   "contains and count agree"_test = [] {
      const glz::flat_map<std::string, int> map{{"a", 1}};

      expect(map.contains("a"));
      expect(map.count("a") == 1);
      expect(not map.contains("z"));
      expect(map.count("z") == 0);
   };

   "lower_bound and upper_bound bracket a key"_test = [] {
      glz::flat_map<int, int> map{{1, 10}, {3, 30}, {5, 50}};

      expect(map.lower_bound(3)->first == 3);
      expect(map.upper_bound(3)->first == 5);
      expect(map.lower_bound(4)->first == 5);
      expect(map.upper_bound(5) == map.end());
      expect(map.lower_bound(0)->first == 1);
   };

   "find, contains, and at accept a heterogeneous key"_test = [] {
      const glz::flat_map<std::string, int> map{{"a", 1}};

      expect(map.contains(std::string_view{"a"}));
      expect(map.find(std::string_view{"a"}) != map.end());
      expect(map.at(std::string_view{"a"}) == 1);
   };
};

suite flat_map_element_access_tests = [] {
   "subscript operator inserts a default value"_test = [] {
      glz::flat_map<std::string, int> map{};

      expect(map["a"] == 0);
      expect(map.size() == 1);

      map["a"] = 42;
      expect(map.at("a") == 42);
      expect(map.size() == 1) << "assigning through the subscript operator must not insert again";
   };

   "subscript operator keeps the map sorted"_test = [] {
      glz::flat_map<std::string, int> map{};
      map["c"] = 3;
      map["a"] = 1;
      map["b"] = 2;

      std::vector<std::string> keys{};
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys == std::vector<std::string>{"a", "b", "c"});
   };

   "at returns a reference on both const and non-const maps"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      map.at("a") = 42;
      expect(map.at("a") == 42);

      const auto& const_map = map;
      static_assert(std::is_same_v<decltype(const_map.at("a")), const int&>);
      expect(const_map.at("a") == 42);
   };
};

#if __cpp_exceptions
suite flat_map_exception_tests = [] {
   "at throws out_of_range for an absent key"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      try {
         (void)map.at("z");
         expect(false) << "at() should have thrown an exception";
      }
      catch (const std::out_of_range&) {
         expect(true);
      }
      catch (...) {
         expect(false) << "at() threw unexpected exception type";
      }
   };

   "const at throws out_of_range for an absent key"_test = [] {
      const glz::flat_map<std::string, int> map{{"a", 1}};

      try {
         (void)map.at("z");
         expect(false) << "at() should have thrown an exception";
      }
      catch (const std::out_of_range&) {
         expect(true);
      }
      catch (...) {
         expect(false) << "at() threw unexpected exception type";
      }
   };
};
#endif

suite flat_map_capacity_tests = [] {
   "reserve grows capacity without changing size"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      map.reserve(64);
      expect(map.capacity() >= 64);
      expect(map.size() == 1);

      map.clear();
      map.shrink_to_fit();
      expect(map.empty());
   };
};

suite flat_map_comparator_tests = [] {
   "custom comparator drives the iteration order"_test = [] {
      glz::flat_map<int, int, std::greater<>> map{{1, 10}, {3, 30}, {2, 20}};

      std::vector<int> keys{};
      for (const auto& [key, value] : map) {
         keys.push_back(key);
      }
      expect(keys == std::vector<int>{3, 2, 1});

      expect(map.at(2) == 20);
      expect(map.find(3) != map.end());
      expect(map.find(9) == map.end());
      expect(map.lower_bound(2)->first == 2);
   };

   "key_comp returns the stored comparator"_test = [] {
      const glz::flat_map<int, int, std::greater<>> map{};

      expect(map.key_comp()(2, 1));
      expect(not map.key_comp()(1, 2));
   };
};

suite flat_map_value_semantics_tests = [] {
   "move-only mapped type is supported"_test = [] {
      glz::flat_map<std::string, std::unique_ptr<int>> map{};

      expect(map.emplace("b", std::make_unique<int>(2)).second);
      expect(map.insert({"a", std::make_unique<int>(1)}).second);

      expect(map.size() == 2);
      expect(map.begin()->first == "a");
      expect(*map.at("a") == 1);
      expect(*map.at("b") == 2);
   };

   "equality compares keys and values"_test = [] {
      const glz::flat_map<std::string, int> left{{"a", 1}, {"b", 2}};
      const glz::flat_map<std::string, int> right{{"b", 2}, {"a", 1}};
      const glz::flat_map<std::string, int> different_value{{"a", 1}, {"b", 3}};
      const glz::flat_map<std::string, int> different_size{{"a", 1}};

      expect(left == right) << "insertion order must not affect equality";
      expect(left != different_value);
      expect(left != different_size);
   };
};

#if GLZ_HAS_OPTIONAL_REF

suite flat_map_optional_lookup_tests = [] {
   "lookup returns the mapped value for a present key"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}, {"b", 2}};

      const auto a = map.lookup("a");
      expect(a.has_value());
      expect(*a == 1);

      const auto b = map.lookup("b");
      expect(b.has_value());
      expect(*b == 2);
   };

   "lookup returns std::nullopt for an absent key"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      expect(map.lookup("z") == std::nullopt);
   };

   "lookup yields a reference into the map"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}, {"b", 2}};

      if (auto a = map.lookup("a")) {
         *a = 42;
      }

      expect(map.at("a") == 42);
      expect(map.at("b") == 2);
      expect(&*map.lookup("a") == &map.at("a"));
   };

   "lookup on an empty map returns std::nullopt"_test = [] {
      glz::flat_map<std::string, int> map{};

      expect(not map.lookup("a").has_value());
   };

   "lookup reflects erase and insert"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      map.erase("a");
      expect(not map.lookup("a").has_value());

      map.insert({"a", 7});
      const auto a = map.lookup("a");
      expect(a.has_value());
      expect(*a == 7);
   };

   "const lookup yields std::optional<const mapped_type&>"_test = [] {
      const glz::flat_map<std::string, int> map{{"a", 1}};

      static_assert(std::is_same_v<decltype(map.lookup("a")), std::optional<const int&>>);

      const auto a = map.lookup("a");
      expect(a.has_value());
      expect(*a == 1);
      expect(&*map.lookup("a") == &map.at("a"));
   };

   "non-const lookup yields std::optional<mapped_type&>"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      static_assert(std::is_same_v<decltype(map.lookup("a")), std::optional<int&>>);
      expect(map.lookup("a").has_value());
   };

   "lookup accepts a heterogeneous key"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      expect(map.lookup(std::string_view{"a"}).has_value());
      expect(map.lookup(std::string{"a"}).has_value());
      expect(not map.lookup(std::string_view{"z"}).has_value());
   };

   "lookup finds a key under a custom comparator"_test = [] {
      glz::flat_map<int, int, std::greater<>> map{{1, 10}, {3, 30}, {2, 20}};

      const auto found = map.lookup(2);
      expect(found.has_value());
      expect(*found == 20);
      expect(&*found == &map.at(2)) << "lookup must reach the same element the comparator search finds";

      expect(not map.lookup(9).has_value());
      expect(not map.lookup(0).has_value());
   };

   "lookup composes with monadic operations"_test = [] {
      glz::flat_map<std::string, int> map{{"a", 1}};

      expect(map.lookup("a").transform([](int v) { return v * 2; }) == 2);
      expect(map.lookup("z").value_or(-1) == -1);
   };

   "lookup works with a move-only mapped type"_test = [] {
      glz::flat_map<std::string, std::unique_ptr<int>> map{};
      map.emplace("a", std::make_unique<int>(1));

      const auto a = map.lookup("a");
      expect(a.has_value());
      expect(**a == 1);
   };
};

#endif

int main() { return 0; }
