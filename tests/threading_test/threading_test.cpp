// Glaze Library
// For the license information refer to glaze.hpp

#include <thread>

#include "glaze/glaze_exceptions.hpp"
#include "glaze/thread/atom.hpp"
#include "glaze/thread/async_vector.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite atom_tests = [] {
   "construction"_test = [] {
      glz::atom<int> a;
      expect(a.load() == 0) << "Default constructor should initialize to 0";

      glz::atom<int> b(42);
      expect(b.load() == 42) << "Value constructor should initialize to given value";

      glz::atom<double> c(3.14);
      expect(c.load() == 3.14) << "Should work with floating point types";
   };

   "copy_semantics"_test = [] {
      glz::atom<int> a(42);
      glz::atom<int> b = a;
      expect(b.load() == 42) << "Copy constructor should copy the value";

      glz::atom<int> c(10);
      c = a;
      expect(c.load() == 42) << "Copy assignment should copy the value";

      glz::atom<int> d;
      d = 100;
      expect(d.load() == 100) << "Assignment from T should store the value";
   };

   "move_semantics"_test = [] {
      glz::atom<int> a(42);
      glz::atom<int> b = std::move(a);
      expect(b.load() == 42) << "Move constructor should copy the value";
      expect(a.load() == 42) << "Source should still have its value after move";

      glz::atom<int> c(10);
      c = std::move(a);
      expect(c.load() == 42) << "Move assignment should copy the value";
      expect(a.load() == 42) << "Source should still have its value after move";
   };

   "comparison_with_atom"_test = [] {
      glz::atom<int> a(42);
      glz::atom<int> b(42);
      glz::atom<int> c(100);

      expect(a == b) << "Equal atoms should compare equal";
      expect(a != c) << "Different atoms should not compare equal";
      expect(a < c) << "Less than should work";
      expect(a <= b) << "Less than or equal should work";
      expect(c > a) << "Greater than should work";
      expect(b >= a) << "Greater than or equal should work";
   };

   "comparison_with_value"_test = [] {
      glz::atom<int> a(42);

      expect(a == 42) << "Atom should compare equal with equal value";
      expect(a != 100) << "Atom should not compare equal with different value";
      expect(a < 100) << "Less than should work with value";
      expect(a <= 42) << "Less than or equal should work with value";
      expect(a > 10) << "Greater than should work with value";
      expect(a >= 42) << "Greater than or equal should work with value";

      expect(42 == a) << "Value should compare equal with equal glz::atom";
      expect(100 != a) << "Value should not compare equal with different glz::atom";
      expect(10 < a) << "Less than should work with value on left";
      expect(42 <= a) << "Less than or equal should work with value on left";
      expect(100 > a) << "Greater than should work with value on left";
      expect(42 >= a) << "Greater than or equal should work with value on left";
   };

   "load_store"_test = [] {
      glz::atom<int> a(42);
      expect(a.load() == 42) << "Load should return current value";

      a.store(100);
      expect(a.load() == 100) << "Store should update the value";

      expect(static_cast<int>(a) == 100) << "Conversion operator should return the value";
   };

   "exchange"_test = [] {
      glz::atom<int> a(42);
      int old = a.exchange(100);

      expect(old == 42) << "Exchange should return old value";
      expect(a.load() == 100) << "Exchange should update the value";
   };

   "compare_exchange"_test = [] {
      glz::atom<int> a(42);

      int expected = 42;
      bool success = a.compare_exchange_strong(expected, 100);

      expect(success) << "Compare exchange should succeed when expected matches";
      expect(a.load() == 100) << "Value should be updated on successful exchange";

      expected = 42;
      success = a.compare_exchange_strong(expected, 200);

      expect(!success) << "Compare exchange should fail when expected doesn't match";
      expect(expected == 100) << "Expected should be updated to actual value on failure";
      expect(a.load() == 100) << "Value should not change on failed exchange";
   };

   "arithmetic_operations"_test = [] {
      glz::atom<int> a(42);

      expect(a.fetch_add(10) == 42) << "Fetch add should return old value";
      expect(a.load() == 52) << "Fetch add should update the value";

      expect(a.fetch_sub(20) == 52) << "Fetch sub should return old value";
      expect(a.load() == 32) << "Fetch sub should update the value";

      expect(a += 8) << "Addition assignment should return new value";
      expect(a.load() == 40) << "Addition assignment should update the value";

      expect(a -= 5) << "Subtraction assignment should return new value";
      expect(a.load() == 35) << "Subtraction assignment should update the value";

      expect(++a == 36) << "Pre-increment should return new value";
      expect(a.load() == 36) << "Pre-increment should update the value";

      expect(a++ == 36) << "Post-increment should return old value";
      expect(a.load() == 37) << "Post-increment should update the value";

      expect(--a == 36) << "Pre-decrement should return new value";
      expect(a.load() == 36) << "Pre-decrement should update the value";

      expect(a-- == 36) << "Post-decrement should return old value";
      expect(a.load() == 35) << "Post-decrement should update the value";
   };

   "bitwise_operations"_test = [] {
      glz::atom<int> a(0b1100);

      expect(a.fetch_and(0b1010) == 0b1100) << "Fetch AND should return old value";
      expect(a.load() == 0b1000) << "Fetch AND should update the value";

      expect(a.fetch_or(0b0011) == 0b1000) << "Fetch OR should return old value";
      expect(a.load() == 0b1011) << "Fetch OR should update the value";

      expect(a.fetch_xor(0b1111) == 0b1011) << "Fetch XOR should return old value";
      expect(a.load() == 0b0100) << "Fetch XOR should update the value";

      expect(a &= 0b0110) << "AND assignment should return new value";
      expect(a.load() == 0b0100) << "AND assignment should update the value";

      expect(a |= 0b0011) << "OR assignment should return new value";
      expect(a.load() == 0b0111) << "OR assignment should update the value";

      expect(a ^= 0b0101) << "XOR assignment should return new value";
      expect(a.load() == 0b0010) << "XOR assignment should update the value";
   };

   "thread_safety"_test = []() mutable {
      glz::atom<int> counter(0);

      std::vector<std::thread> threads;
      for (int i = 0; i < 10; ++i) {
         threads.emplace_back([&counter]() {
            for (int j = 0; j < 1000; ++j) {
               counter++;
            }
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      expect(counter.load() == 10000) << "Concurrent increments should result in correct count";
   };

   "complex_type"_test = [] {
      struct Point
      {
         int x = 0;
         int y = 0;
         constexpr auto operator<=>(const Point&) const = default;
      };

      Point p1{1, 2};
      Point p2{1, 2};
      Point p3{3, 4};

      glz::atom<Point> a(p1);
      glz::atom<Point> b(p2);
      glz::atom<Point> c(p3);

      expect(a == b) << "Atoms with structs should compare correctly";
      expect(a != c) << "Atoms with structs should compare correctly";
      expect(a == p1) << "Atom should compare with raw struct value";
   };

   "memory_order"_test = [] {
      glz::atom<int> a(42);

      int v1 = a.load(std::memory_order_relaxed);
      expect(v1 == 42) << "Load with relaxed memory order should work";

      a.store(100, std::memory_order_release);
      int v2 = a.load(std::memory_order_acquire);
      expect(v2 == 100) << "Store with release and load with acquire should work";

      int old = a.exchange(200, std::memory_order_acq_rel);
      expect(old == 100) << "Exchange with acq_rel memory order should work";
      expect(a.load() == 200) << "Value should be updated";
   };
};

struct TestObject
{
   int id = 0;
   std::string name = "default";
   
   TestObject() = default;
   TestObject(int id, std::string name) : id(id), name(std::move(name)) {}
   
   bool operator==(const TestObject& other) const
   {
      return id == other.id && name == other.name;
   }
};

suite async_vector_tests = [] {
   "construction"_test = [] {
      glz::async_vector<int> vec;
      expect(vec.size() == 0) << "Default constructor should create empty vector";
      expect(vec.empty()) << "Default constructed vector should be empty";
      
      // Initialize with elements
      glz::async_vector<int> vec_with_size;
      vec_with_size.resize(5, 42);
      expect(vec_with_size.size() == 5) << "Size should match requested size";
      for (size_t i = 0; i < vec_with_size.size(); ++i) {
         expect(vec_with_size[i] == 42) << "All elements should be initialized with the provided value";
      }
   };
   
   "copy_semantics"_test = [] {
      glz::async_vector<int> original;
      original.push_back(1);
      original.push_back(2);
      original.push_back(3);
      
      // Test copy constructor
      glz::async_vector<int> copy_constructed = original;
      expect(copy_constructed.size() == original.size()) << "Copy constructed vector should have same size";
      for (size_t i = 0; i < original.size(); ++i) {
         expect(copy_constructed[i] == original[i]) << "Elements should match after copy construction";
      }
      
      // Modify original to verify deep copy
      original.push_back(4);
      expect(copy_constructed.size() == 3) << "Copy should not be affected by changes to original";
      
      // Test copy assignment
      glz::async_vector<int> copy_assigned;
      copy_assigned = original;
      expect(copy_assigned.size() == original.size()) << "Copy assigned vector should have same size";
      for (size_t i = 0; i < original.size(); ++i) {
         expect(copy_assigned[i] == original[i]) << "Elements should match after copy assignment";
      }
      
      // Modify original again to verify deep copy
      original[0] = 99;
      expect(copy_assigned[0] == 1) << "Copy should not be affected by changes to original values";
   };
   
   "move_semantics"_test = [] {
      glz::async_vector<int> original;
      original.push_back(1);
      original.push_back(2);
      original.push_back(3);
      
      // Test move constructor
      glz::async_vector<int> move_constructed = std::move(original);
      expect(move_constructed.size() == 3) << "Move constructed vector should have original size";
      expect(move_constructed[0] == 1) << "Elements should be moved correctly";
      expect(move_constructed[1] == 2) << "Elements should be moved correctly";
      expect(move_constructed[2] == 3) << "Elements should be moved correctly";
      
      // Test move assignment
      glz::async_vector<int> move_assigned;
      move_assigned = std::move(move_constructed);
      expect(move_assigned.size() == 3) << "Move assigned vector should have original size";
      expect(move_assigned[0] == 1) << "Elements should be moved correctly";
      expect(move_assigned[1] == 2) << "Elements should be moved correctly";
      expect(move_assigned[2] == 3) << "Elements should be moved correctly";
   };
   
   "element_access"_test = [] {
      glz::async_vector<int> vec;
      vec.push_back(10);
      vec.push_back(20);
      vec.push_back(30);
      
      // Test operator[]
      expect(vec[0] == 10) << "Operator[] should return correct element";
      expect(vec[1] == 20) << "Operator[] should return correct element";
      expect(vec[2] == 30) << "Operator[] should return correct element";
      
      // Test at()
      expect(vec.at(0) == 10) << "at() should return correct element";
      expect(vec.at(1) == 20) << "at() should return correct element";
      expect(vec.at(2) == 30) << "at() should return correct element";
      
      // Test front() and back()
      expect(vec.front() == 10) << "front() should return first element";
      expect(vec.back() == 30) << "back() should return last element";
      
      // Test out of bounds with at()
      bool exception_thrown = false;
      try {
         vec.at(5);
      } catch (const std::out_of_range&) {
         exception_thrown = true;
      }
      expect(exception_thrown) << "at() should throw std::out_of_range for out of bounds access";
      
      // Test modification through element access
      vec[1] = 25;
      expect(vec[1] == 25) << "Element should be modifiable through operator[]";
      
      vec.at(2) = 35;
      expect(vec[2] == 35) << "Element should be modifiable through at()";
   };
   
   "capacity"_test = [] {
      glz::async_vector<int> vec;
      expect(vec.empty()) << "New vector should be empty";
      
      vec.push_back(1);
      expect(!vec.empty()) << "Vector with elements should not be empty";
      expect(vec.size() == 1) << "Size should reflect number of elements";
      
      vec.reserve(10);
      expect(vec.capacity() >= 10) << "Capacity should be at least the reserved amount";
      expect(vec.size() == 1) << "Reserve should not change size";
      
      vec.resize(5);
      expect(vec.size() == 5) << "Resize should change size";
      
      vec.resize(3);
      expect(vec.size() == 3) << "Resize to smaller should reduce size";
      
      size_t cap_before = vec.capacity();
      vec.shrink_to_fit();
      expect(vec.capacity() <= cap_before) << "Shrink to fit should not increase capacity";
   };
   
   "modifiers"_test = [] {
      glz::async_vector<int> vec;
      
      // Test push_back
      vec.push_back(1);
      vec.push_back(2);
      expect(vec.size() == 2) << "Size should increase after push_back";
      expect(vec[0] == 1 && vec[1] == 2) << "Elements should be in correct order";
      
      // Test emplace_back
      vec.emplace_back(3);
      expect(vec.size() == 3) << "Size should increase after emplace_back";
      expect(vec[2] == 3) << "Element should be constructed in place";
      
      // Test pop_back
      vec.pop_back();
      expect(vec.size() == 2) << "Size should decrease after pop_back";
      expect(vec[1] == 2) << "Last element should be removed";
      
      // Test insert
      auto it = vec.insert(vec.begin(), 0);
      expect(*it == 0) << "Insert should return iterator to inserted element";
      expect(vec.size() == 3) << "Size should increase after insert";
      expect(vec[0] == 0 && vec[1] == 1 && vec[2] == 2) << "Elements should be in correct order after insert";
      
      // Test emplace
      auto it2 = vec.emplace(vec.begin() + 2, 15);
      expect(*it2 == 15) << "Emplace should return iterator to inserted element";
      expect(vec.size() == 4) << "Size should increase after emplace";
      expect(vec[0] == 0 && vec[1] == 1 && vec[2] == 15 && vec[3] == 2) << "Elements should be in correct order after emplace";
      
      // Test erase
      auto it3 = vec.erase(vec.begin() + 1);
      expect(*it3 == 15) << "Erase should return iterator to element after erased";
      expect(vec.size() == 3) << "Size should decrease after erase";
      expect(vec[0] == 0 && vec[1] == 15 && vec[2] == 2) << "Elements should be in correct order after erase";
      
      // Test clear
      vec.clear();
      expect(vec.empty()) << "Vector should be empty after clear";
   };
   
   "iterators"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 5; ++i) {
         vec.push_back(i);
      }
      
      // Test iterator traversal
      int sum = 0;
      for (auto it = vec.begin(); it != vec.end(); ++it) {
         sum += *it;
      }
      expect(sum == 10) << "Iterator traversal should access all elements";
      
      // Test const_iterator traversal
      const glz::async_vector<int>& const_vec = vec;
      sum = 0;
      for (auto it = const_vec.begin(); it != const_vec.end(); ++it) {
         sum += *it;
      }
      expect(sum == 10) << "Const iterator traversal should access all elements";
      
      // Test iterator modification
      for (auto it = vec.begin(); it != vec.end(); ++it) {
         *it *= 2;
      }
      expect(vec[0] == 0 && vec[1] == 2 && vec[2] == 4 && vec[3] == 6 && vec[4] == 8)
      << "Elements should be modifiable through iterators";
      
      // Test range-based for loop
      sum = 0;
      for (const auto& val : vec) {
         sum += val;
      }
      expect(sum == 20) << "Range-based for loop should access all elements";
   };
   
   "complex_types"_test = [] {
      glz::async_vector<TestObject> vec;
      
      vec.emplace_back(1, "one");
      vec.emplace_back(2, "two");
      vec.emplace_back(3, "three");
      
      expect(vec.size() == 3) << "Size should reflect number of complex objects";
      expect(vec[0].id == 1 && vec[0].name == "one") << "Complex object should be stored correctly";
      expect(vec[1].id == 2 && vec[1].name == "two") << "Complex object should be stored correctly";
      expect(vec[2].id == 3 && vec[2].name == "three") << "Complex object should be stored correctly";
      
      // Test copying of complex types
      glz::async_vector<TestObject> vec_copy = vec;
      vec[0].id = 10;
      vec[0].name = "modified";
      
      expect(vec_copy[0].id == 1 && vec_copy[0].name == "one")
      << "Copied vector should not be affected by changes to original";
   };
   
   "swap"_test = [] {
      glz::async_vector<int> vec1;
      vec1.push_back(1);
      vec1.push_back(2);
      
      glz::async_vector<int> vec2;
      vec2.push_back(3);
      vec2.push_back(4);
      vec2.push_back(5);
      
      vec1.swap(vec2);
      
      expect(vec1.size() == 3) << "First vector should have size of second vector after swap";
      expect(vec2.size() == 2) << "Second vector should have size of first vector after swap";
      
      expect(vec1[0] == 3 && vec1[1] == 4 && vec1[2] == 5) << "First vector should have elements of second vector";
      expect(vec2[0] == 1 && vec2[1] == 2) << "Second vector should have elements of first vector";
   };
   
   "thread_safety_read"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 100; ++i) {
         vec.push_back(i);
      }
      
      std::vector<std::thread> threads;
      std::vector<int> sums(10, 0);
      
      // Create multiple threads that read from the vector
      for (int i = 0; i < 10; ++i) {
         threads.emplace_back([&vec, &sums, i]() {
            for (int j = 0; j < 100; ++j) {
               sums[i] += vec[j];
            }
         });
      }
      
      for (auto& t : threads) {
         t.join();
      }
      
      // All threads should have computed the same sum
      for (const auto& sum : sums) {
         expect(sum == 4950) << "All threads should read the same values";
      }
   };
   
   "thread_safety_write"_test = [] {
      glz::async_vector<int> vec;
      
      std::vector<std::thread> threads;
      
      // Create multiple threads that write to the vector
      for (int i = 0; i < 10; ++i) {
         threads.emplace_back([&vec, i]() {
            for (int j = 0; j < 100; ++j) {
               vec.push_back(i * 100 + j);
            }
         });
      }
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(vec.size() == 1000) << "Vector should contain all elements from all threads";
      
      // Verify that all expected values are in the vector
      std::vector<int> expected_values;
      for (int i = 0; i < 10; ++i) {
         for (int j = 0; j < 100; ++j) {
            expected_values.push_back(i * 100 + j);
         }
      }
      
      std::vector<int> actual_values;
      for (size_t i = 0; i < vec.size(); ++i) {
         actual_values.push_back(vec[i]);
      }
      
      std::sort(actual_values.begin(), actual_values.end());
      std::sort(expected_values.begin(), expected_values.end());
      
      expect(actual_values == expected_values) << "Vector should contain all expected values";
   };
   
   "thread_safety_mixed"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 100; ++i) {
         vec.push_back(i);
      }
      
      std::atomic<bool> stop{false};
      std::vector<std::thread> threads;
      
      // Reader threads
      for (int i = 0; i < 5; ++i) {
         std::atomic<size_t> sum = 0;
         threads.emplace_back([&vec, &stop, &sum]() {
            while (!stop) {
               for (size_t j = 0; j < vec.size(); ++j) {
                  sum += vec[j];
               }
            }
         });
      }
      
      // Writer threads
      for (int i = 0; i < 5; ++i) {
         threads.emplace_back([&vec, &stop, i]() {
            for (int j = 0; j < 100 && !stop; ++j) {
               vec.push_back(i * 100 + j);
               std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
         });
      }
      
      // Let the threads run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
      
      // We can't check exact values due to the concurrent nature, but we should have more than we started with
      expect(vec.size() > 100) << "Vector should have grown during concurrent operations";
   };
};

int main() {}
