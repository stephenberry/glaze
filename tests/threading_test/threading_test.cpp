// Glaze Library
// For the license information refer to glaze.hpp

#include <thread>
#include <random>

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

      std::deque<std::thread> threads;
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
      vec_with_size.write().resize(5, 42);
      expect(vec_with_size.size() == 5) << "Size should match requested size";
      for (size_t i = 0; i < vec_with_size.size(); ++i) {
         expect(vec_with_size.read()[i] == 42) << "All elements should be initialized with the provided value";
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
         expect(copy_constructed.read()[i] == original.read()[i]) << "Elements should match after copy construction";
      }
      
      // Modify original to verify deep copy
      original.push_back(4);
      expect(copy_constructed.size() == 3) << "Copy should not be affected by changes to original";
      
      // Test copy assignment
      glz::async_vector<int> copy_assigned;
      copy_assigned = original;
      expect(copy_assigned.size() == original.size()) << "Copy assigned vector should have same size";
      for (size_t i = 0; i < original.size(); ++i) {
         expect(copy_assigned.read()[i] == original.read()[i]) << "Elements should match after copy assignment";
      }
      
      // Modify original again to verify deep copy
      original.write()[0] = 99;
      expect(copy_assigned.read()[0] == 1) << "Copy should not be affected by changes to original values";
   };
   
   "move_semantics"_test = [] {
      glz::async_vector<int> original;
      original.push_back(1);
      original.push_back(2);
      original.push_back(3);
      
      // Test move constructor
      glz::async_vector<int> move_constructed = std::move(original);
      expect(move_constructed.size() == 3) << "Move constructed vector should have original size";
      expect(move_constructed.read()[0] == 1) << "Elements should be moved correctly";
      expect(move_constructed.read()[1] == 2) << "Elements should be moved correctly";
      expect(move_constructed.read()[2] == 3) << "Elements should be moved correctly";
      
      // Test move assignment
      glz::async_vector<int> move_assigned;
      move_assigned = std::move(move_constructed);
      expect(move_assigned.size() == 3) << "Move assigned vector should have original size";
      expect(move_assigned.read()[0] == 1) << "Elements should be moved correctly";
      expect(move_assigned.read()[1] == 2) << "Elements should be moved correctly";
      expect(move_assigned.read()[2] == 3) << "Elements should be moved correctly";
   };
   
   "element_access"_test = [] {
      glz::async_vector<int> vec;
      vec.push_back(10);
      vec.push_back(20);
      vec.push_back(30);
      
      // Test operator[]
      expect(vec.read()[0] == 10) << "Operator[] should return correct element";
      expect(vec.read()[1] == 20) << "Operator[] should return correct element";
      expect(vec.read()[2] == 30) << "Operator[] should return correct element";
      
      // Test at()
      expect(vec.read().at(0) == 10) << "at() should return correct element";
      expect(vec.read().at(1) == 20) << "at() should return correct element";
      expect(vec.read().at(2) == 30) << "at() should return correct element";
      
      // Test front() and back()
      expect(vec.read().front() == 10) << "front() should return first element";
      expect(vec.read().back() == 30) << "back() should return last element";
      
      // Test out of bounds with at()
      bool exception_thrown = false;
      try {
         vec.read().at(5);
      } catch (const std::out_of_range&) {
         exception_thrown = true;
      }
      expect(exception_thrown) << "at() should throw std::out_of_range for out of bounds access";
      
      // Test modification through element access
      vec.write()[1] = 25;
      expect(vec.read()[1] == 25) << "Element should be modifiable through operator[]";
      
      vec.write().at(2) = 35;
      expect(vec.read()[2] == 35) << "Element should be modifiable through at()";
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
      
      vec.write().resize(5);
      expect(vec.size() == 5) << "Resize should change size";
      
      vec.write().resize(3);
      expect(vec.size() == 3) << "Resize to smaller should reduce size";
      
      size_t cap_before = vec.capacity();
      vec.write().shrink_to_fit();
      expect(vec.capacity() <= cap_before) << "Shrink to fit should not increase capacity";
   };
   
   "modifiers"_test = [] {
      glz::async_vector<int> vec;
      
      // Test push_back
      vec.push_back(1);
      vec.push_back(2);
      expect(vec.size() == 2) << "Size should increase after push_back";
      expect(vec.read()[0] == 1 && vec.read()[1] == 2) << "Elements should be in correct order";
      
      // Test emplace_back
      vec.emplace_back(3);
      expect(vec.size() == 3) << "Size should increase after emplace_back";
      expect(vec.read()[2] == 3) << "Element should be constructed in place";
      
      // Test pop_back
      vec.pop_back();
      expect(vec.size() == 2) << "Size should decrease after pop_back";
      expect(vec.read()[1] == 2) << "Last element should be removed";
      
      // Test insert
      {
         auto proxy = vec.write();
         auto it = proxy.insert(proxy.begin(), 0);
         expect(*it == 0) << "Insert should return iterator to inserted element";
      }
      expect(vec.size() == 3) << "Size should increase after insert";
      expect(vec.read()[0] == 0 && vec.read()[1] == 1 && vec.read()[2] == 2) << "Elements should be in correct order after insert";
      
      // Test emplace
      {
         auto proxy = vec.write();
         auto it2 = proxy.emplace(proxy.begin() + 2, 15);
         expect(*it2 == 15) << "Emplace should return iterator to inserted element";
      }
      expect(vec.size() == 4) << "Size should increase after emplace";
      expect(vec.read()[0] == 0 && vec.read()[1] == 1 && vec.read()[2] == 15 && vec.read()[3] == 2) << "Elements should be in correct order after emplace";
      
      // Test erase
      {
         auto proxy = vec.write();
         auto it3 = proxy.erase(proxy.begin() + 1);
         expect(*it3 == 15) << "Erase should return iterator to element after erased";
      }
      expect(vec.size() == 3) << "Size should decrease after erase";
      expect(vec.read()[0] == 0 && vec.read()[1] == 15 && vec.read()[2] == 2) << "Elements should be in correct order after erase";
      
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
      {
         auto proxy = vec.read();
         for (auto it = proxy.begin(); it != proxy.end(); ++it) {
            sum += *it;
         }
      }
      expect(sum == 10) << "Iterator traversal should access all elements";
      
      // Test const_iterator traversal
      const glz::async_vector<int>& const_vec = vec;
      sum = 0;
      {
         auto proxy = const_vec.read();
         for (auto it = proxy.begin(); it != proxy.end(); ++it) {
            sum += *it;
         }
      }
      expect(sum == 10) << "Const iterator traversal should access all elements";
      
      // Test iterator modification
      {
         auto proxy = vec.write();
         for (auto it = proxy.begin(); it != proxy.end(); ++it) {
            *it *= 2;
         }
      }
      expect(vec.read()[0] == 0 && vec.read()[1] == 2 && vec.read()[2] == 4 && vec.read()[3] == 6 && vec.read()[4] == 8)
      << "Elements should be modifiable through iterators";
      
      // Test range-based for loop
      sum = 0;
      for (const auto& val : vec.read()) {
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
      expect(vec.read()[0].id == 1 && vec.read()[0].name == "one") << "Complex object should be stored correctly";
      expect(vec.read()[1].id == 2 && vec.read()[1].name == "two") << "Complex object should be stored correctly";
      expect(vec.read()[2].id == 3 && vec.read()[2].name == "three") << "Complex object should be stored correctly";
      
      // Test copying of complex types
      glz::async_vector<TestObject> vec_copy = vec;
      vec.write()[0].id = 10;
      vec.write()[0].name = "modified";
      
      expect(vec_copy.read()[0].id == 1 && vec_copy.read()[0].name == "one")
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
      
      expect(vec1.read()[0] == 3 && vec1.read()[1] == 4 && vec1.read()[2] == 5) << "First vector should have elements of second vector";
      expect(vec2.read()[0] == 1 && vec2.read()[1] == 2) << "Second vector should have elements of first vector";
   };
   
   "thread_safety_read"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 100; ++i) {
         vec.push_back(i);
      }
      
      std::deque<std::thread> threads;
      std::vector<int> sums(10, 0);
      
      // Create multiple threads that read from the vector
      for (int i = 0; i < 10; ++i) {
         threads.emplace_back([&vec, &sums, i]() {
            for (int j = 0; j < 100; ++j) {
               sums[i] += vec.read()[j];
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
      
      std::deque<std::thread> threads;
      
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
         actual_values.push_back(vec.read()[i]);
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
      std::deque<std::thread> threads;
      
      // Reader threads
      for (int i = 0; i < 5; ++i) {
         std::atomic<size_t> sum = 0;
         threads.emplace_back([&vec, &stop, &sum]() {
            while (!stop) {
               for (size_t j = 0; j < vec.size(); ++j) {
                  sum += vec.read()[j];
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

// Additional stress tests and edge cases for async_vector
/*
// Complex data type with move/copy semantics verification
struct ComplexObject {
   int id;
   std::string data;
   std::vector<double> values;
   std::unique_ptr<int> ptr;
   bool was_moved = false;
   std::atomic<int> access_count{0};
   
   ComplexObject(int id, std::string data, std::vector<double> values)
   : id(id), data(std::move(data)), values(std::move(values)), ptr(std::make_unique<int>(id)) {}
   
   ComplexObject() : id(0), ptr(std::make_unique<int>(0)) {}
   
   // Copy constructor
   ComplexObject(const ComplexObject& other)
   : id(other.id), data(other.data), values(other.values),
   ptr(other.ptr ? std::make_unique<int>(*other.ptr) : nullptr),
   access_count(other.access_count.load()) {}
   
   // Move constructor
   ComplexObject(ComplexObject&& other) noexcept
   : id(other.id), data(std::move(other.data)), values(std::move(other.values)),
   ptr(std::move(other.ptr)), was_moved(true),
   access_count(other.access_count.load()) {}
   
   // Copy assignment
   ComplexObject& operator=(const ComplexObject& other) {
      if (this != &other) {
         id = other.id;
         data = other.data;
         values = other.values;
         ptr = other.ptr ? std::make_unique<int>(*other.ptr) : nullptr;
         access_count.store(other.access_count.load());
      }
      return *this;
   }
   
   // Move assignment
   ComplexObject& operator=(ComplexObject&& other) noexcept {
      if (this != &other) {
         id = other.id;
         data = std::move(other.data);
         values = std::move(other.values);
         ptr = std::move(other.ptr);
         was_moved = true;
         access_count.store(other.access_count.load());
      }
      return *this;
   }
   
   void access() {
      access_count++;
   }
   
   bool operator==(const ComplexObject& other) const {
      return id == other.id &&
      data == other.data &&
      values == other.values &&
      ((!ptr && !other.ptr) || (ptr && other.ptr && *ptr == *other.ptr));
   }
};

suite additional_async_vector_tests = [] {
   
   "concurrent_iterator_stress"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 1000; ++i) {
         vec.push_back(i);
      }
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> errors{0};
      
      // Thread that continually creates and destroys iterators
      threads.emplace_back([&]() {
         while (!stop) {
            auto it1 = vec.begin();
            [[maybe_unused]] auto it2 = vec.begin();
            auto it3 = vec.end();
            [[maybe_unused]] auto it4 = vec.cbegin();
            [[maybe_unused]] auto it5 = vec.cend();
            
            // Test iterator arithmetic and comparisons
            try {
               auto it_mid = it1 + vec.size() / 2;
               if (!(it_mid > it1 && it_mid < it3)) {
                  errors++;
               }
               
               auto it_adv = it1;
               it_adv += 10;
               if (it_adv - it1 != 10) {
                  errors++;
               }
               
               // Create iterator and then immediately discard
               for (int i = 0; i < 50; ++i) {
                  [[maybe_unused]] auto temp_it = vec.begin() + i;
                  [[maybe_unused]] auto temp_cit = vec.cbegin() + i;
               }
            } catch (const std::exception& e) {
               errors++;
            }
         }
      });
      
      // Thread that reads through iterators
      threads.emplace_back([&]() {
         while (!stop) {
            try {
               int sum = 0;
               for (auto it = vec.begin(); it != vec.end(); ++it) {
                  sum += *it;
               }
               // Basic validation check
               if (sum < 0) {
                  errors++;
               }
            } catch (const std::exception& e) {
               errors++;
            }
         }
      });
      
      // Thread that modifies through iterators
      threads.emplace_back([&]() {
         int counter = 0;
         while (!stop) {
            try {
               auto it = vec.begin() + (counter % vec.size());
               *it = counter;
               counter++;
            } catch (const std::exception& e) {
               errors++;
            }
         }
      });
      
      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(errors == 0) << "Iterator operations should not produce errors";
   };
   
   "concurrent_insert_erase"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 100; ++i) {
         vec.push_back(i);
      }
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> errors{0};
      
      // Multiple threads inserting and erasing concurrently
      for (int t = 0; t < 5; ++t) {
         threads.emplace_back([&, t]() {
            for (int i = 0; i < 100 && !stop; ++i) {
               try {
                  // Pick a random position
                  size_t pos = std::rand() % (vec.size() + 1);
                  auto insert_pos = vec.begin();
                  std::advance(insert_pos, std::min(pos, vec.size()));
                  
                  // Insert a new value
                  auto it = vec.insert(insert_pos, t * 1000 + i);
                  
                  // Verify the inserted value
                  if (*it != t * 1000 + i) {
                     errors++;
                  }
                  
                  // Small delay to increase chance of contention
                  std::this_thread::sleep_for(std::chrono::microseconds(std::rand() % 100));
                  
                  // Pick another random position for erasure
                  if (vec.size() > 0) {
                     size_t erase_pos = std::rand() % vec.size();
                     auto erase_iter = vec.begin();
                     std::advance(erase_iter, erase_pos);
                     vec.erase(erase_iter);
                  }
               } catch (const std::exception& e) {
                  errors++;
               }
            }
         });
      }
      
      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(errors == 0) << "Concurrent insert/erase operations should not produce errors";
   };
   
   "emplace_stress"_test = [] {
      glz::async_vector<ComplexObject> vec;
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> errors{0};
      
      // Multiple threads using emplace concurrently
      for (int t = 0; t < 5; ++t) {
         threads.emplace_back([&, t]() {
            for (int i = 0; i < 100 && !stop; ++i) {
               try {
                  // Pick a random position
                  size_t pos = std::rand() % (vec.size() + 1);
                  auto insert_pos = vec.begin();
                  std::advance(insert_pos, std::min(pos, vec.size()));
                  
                  // Create a string and vector for the complex object
                  std::string data = "Thread " + std::to_string(t) + " Item " + std::to_string(i);
                  std::vector<double> values;
                  for (int j = 0; j < 5; ++j) {
                     values.push_back(t + i + j / 10.0);
                  }
                  
                  // Emplace a new complex object
                  auto it = vec.emplace(insert_pos, t * 1000 + i, data, values);
                  
                  // Verify the emplaced object
                  if (it->id != t * 1000 + i || it->data != data) {
                     errors++;
                  }
                  
                  // Small delay to increase chance of contention
                  std::this_thread::sleep_for(std::chrono::microseconds(std::rand() % 100));
               } catch (const std::exception& e) {
                  errors++;
               }
            }
         });
      }
      
      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(errors == 0) << "Concurrent emplace operations should not produce errors";
      expect(vec.size() > 0) << "Vector should contain elements after emplace operations";
      
      // Validate all objects
      for (const auto& obj : vec) {
         expect(obj.ptr != nullptr) << "Each object should have a valid unique_ptr";
         expect(*obj.ptr == obj.id) << "Each object's unique_ptr should point to correct value";
      }
   };
   
   "deadlock_prevention"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 1000; ++i) {
         vec.push_back(i);
      }
      
      // Test the most deadlock-prone operations:
      // - Getting an iterator
      // - Using that iterator to modify the container
      // - Multiple nesting levels
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> completed_operations{0};
      
      for (int t = 0; t < 5; ++t) {
         threads.emplace_back([&, t]() {
            while (!stop) {
               try {
                  // Get a const_iterator
                  auto it1 = vec.begin() + (t * 100 % vec.size());
                  int val1 = *it1;
                  
                  // Use that iterator in an insert operation (potential deadlock scenario)
                  auto new_it = vec.insert(it1, val1 * 2);
                  
                  // Now use the new iterator in another operation
                  auto another_it = vec.begin() + (vec.size() / 2);
                  auto erased_it = vec.erase(another_it);
                  
                  // Try to use all three iterators
                  if (*it1 >= 0 && *new_it >= 0 && (erased_it == vec.end() || *erased_it >= 0)) {
                     completed_operations++;
                  }
               } catch (const std::exception& e) {
                  // Error, but not necessarily a deadlock
               }
            }
         });
      }
      
      // Run for enough time to detect deadlocks
      std::this_thread::sleep_for(std::chrono::milliseconds(400));
      stop = true;
      
      for (auto& t : threads) {
         if (t.joinable()) {
            t.join();
         }
      }
      
      expect(completed_operations > 0) << "Some operations should complete without deadlock";
   };
   
   "iterator_races"_test = [] {
      glz::async_vector<ComplexObject> vec;
      
      // Fill with complex objects
      for (int i = 0; i < 100; ++i) {
         std::vector<double> values{static_cast<double>(i), static_cast<double>(i*2)};
         vec.emplace_back(i, "Object " + std::to_string(i), values);
      }
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> errors{0};
      
      // Thread that creates iterators and performs random jumps
      threads.emplace_back([&]() {
         auto it = vec.begin();
         while (!stop) {
            try {
               int jump = std::rand() % 20 - 10; // Random jump forward or backward
               if (it + jump >= vec.begin() && it + jump < vec.end()) {
                  it += jump;
                  it->access(); // Access the object to increment counter
               }
            } catch (const std::exception& e) {
               errors++;
            }
         }
      });
      
      // Thread that continually modifies the vector
      threads.emplace_back([&]() {
         int counter = 0;
         while (!stop) {
            try {
               if (counter % 3 == 0 && vec.size() > 0) {
                  // Remove from beginning
                  vec.erase(vec.begin());
               } else if (counter % 3 == 1) {
                  // Add to end
                  std::vector<double> values{static_cast<double>(counter)};
                  vec.emplace_back(1000 + counter, "New " + std::to_string(counter), values);
               } else if (vec.size() > 0) {
                  // Replace middle
                  size_t mid = vec.size() / 2;
                  auto mid_it = vec.begin() + mid;
                  std::vector<double> values{static_cast<double>(counter * 10)};
                  *mid_it = ComplexObject(2000 + counter, "Replaced", values);
               }
               counter++;
               std::this_thread::sleep_for(std::chrono::microseconds(100));
            } catch (const std::exception& e) {
               errors++;
            }
         }
      });
      
      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(errors < 10) << "Iterator races should be handled gracefully";
      // We expect some errors due to race conditions, but not too many
   };
   
   "swap_under_contention"_test = [] {
      glz::async_vector<int> vec1;
      glz::async_vector<int> vec2;
      
      // Fill vectors
      for (int i = 0; i < 1000; ++i) {
         vec1.push_back(i);
         vec2.push_back(1000 - i);
      }
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> swap_count{0};
      std::atomic<int> errors{0};
      
      // Thread that continually swaps the vectors
      threads.emplace_back([&]() {
         while (!stop) {
            try {
               vec1.swap(vec2);
               swap_count++;
            } catch (const std::exception& e) {
               errors++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
         }
      });
      
      // Threads that concurrently read from both vectors
      for (int t = 0; t < 3; ++t) {
         threads.emplace_back([&]() {
            while (!stop) {
               try {
                  int sum1 = 0, sum2 = 0;
                  for (const auto& v : vec1) {
                     sum1 += v;
                  }
                  for (const auto& v : vec2) {
                     sum2 += v;
                  }
                  // Basic validation: we can't know which vector has which values due to swaps
                  // but we know what the total should be
                  int total = sum1 + sum2;
                  int expected_total = 0;
                  for (int i = 0; i < 1000; ++i) {
                     expected_total += i + (1000 - i);
                  }
                  if (total != expected_total) {
                     errors++;
                  }
               } catch (const std::exception& e) {
                  errors++;
               }
            }
         });
      }
      
      // Threads that concurrently modify both vectors
      for (int t = 0; t < 2; ++t) {
         threads.emplace_back([&, t]() {
            int counter = 0;
            while (!stop) {
               try {
                  size_t idx = std::rand() % std::min(vec1.size(), vec2.size());
                  vec1[idx] = t * 1000 + counter;
                  vec2[idx] = t * 2000 + counter;
                  counter++;
                  std::this_thread::sleep_for(std::chrono::microseconds(50));
               } catch (const std::exception& e) {
                  errors++;
               }
            }
         });
      }
      
      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(swap_count > 0) << "Multiple swaps should succeed";
      expect(errors < 10) << "Concurrent swap operations should not cause many errors";
   };
   
   "resize_under_contention"_test = [] {
      glz::async_vector<int> vec;
      for (int i = 0; i < 1000; ++i) {
         vec.push_back(i);
      }
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> resize_count{0};
      std::atomic<int> errors{0};
      
      // Thread that continually resizes the vector
      threads.emplace_back([&]() {
         int size = 1000;
         bool growing = true;
         
         while (!stop) {
            try {
               if (growing) {
                  size += 100;
                  if (size > 2000) growing = false;
               } else {
                  size -= 100;
                  if (size < 500) growing = true;
               }
               
               vec.resize(size, 42);
               resize_count++;
               std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (const std::exception& e) {
               errors++;
            }
         }
      });
      
      // Threads that read the vector during resizing
      std::atomic<int> sum = 0;
      for (int t = 0; t < 3; ++t) {
         threads.emplace_back([&]() {
            while (!stop) {
               try {
                  size_t current_size = vec.size();
                  for (size_t i = 0; i < current_size && i < vec.size(); ++i) {
                     sum += vec[i];
                  }
               } catch (const std::exception& e) {
                  errors++;
               }
            }
         });
      }
      
      // Threads that write to the vector during resizing
      for (int t = 0; t < 2; ++t) {
         threads.emplace_back([&]() {
            int counter = 0;
            while (!stop) {
               try {
                  size_t current_size = vec.size();
                  if (current_size > 0) {
                     size_t idx = counter % current_size;
                     if (idx < vec.size()) {
                        vec[idx] = counter;
                     }
                  }
                  counter++;
               } catch (const std::exception& e) {
                  errors++;
               }
            }
         });
      }
      
      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(resize_count > 0) << "Multiple resize operations should succeed";
   };
   
   "massive_parallel_operations"_test = [] {
      glz::async_vector<size_t> vec;
      
      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<size_t> operation_count{0};
      
      // Create numerous threads performing different operations
      for (int t = 0; t < 20; ++t) {
         threads.emplace_back([&, t]() {
            std::mt19937 gen(t); // Deterministic random generator
            std::uniform_int_distribution<> dis(0, 9);
            
            while (!stop) {
               int op = dis(gen);
               try {
                  switch (op) {
                     case 0: { // Push back
                        vec.push_back(t * 1000 + operation_count.load());
                        break;
                     }
                     case 1: { // Pop back if not empty
                        if (!vec.empty()) {
                           vec.pop_back();
                        }
                        break;
                     }
                     case 2: { // Read random element
                        if (!vec.empty()) {
                           size_t idx = gen() % vec.size();
                           [[maybe_unused]] volatile size_t val = vec[idx]; // Force read
                        }
                        break;
                     }
                     case 3: { // Modify random element
                        if (!vec.empty()) {
                           size_t idx = gen() % vec.size();
                           vec[idx] = t;
                        }
                        break;
                     }
                     case 4: { // Insert at random position
                        size_t pos = vec.empty() ? 0 : (gen() % vec.size());
                        auto it = vec.begin();
                        std::advance(it, std::min(pos, vec.size()));
                        vec.insert(it, t);
                        break;
                     }
                     case 5: { // Erase at random position
                        if (!vec.empty()) {
                           size_t pos = gen() % vec.size();
                           auto it = vec.begin();
                           std::advance(it, pos);
                           vec.erase(it);
                        }
                        break;
                     }
                     case 6: { // Iterate through
                        [[maybe_unused]] volatile size_t count = 0;
                        for (const auto& val : vec) {
                           count += (val > 0 ? 1 : 0); // Simple operation to ensure compiler doesn't optimize away
                        }
                        break;
                     }
                     case 7: { // Resize
                        size_t new_size = 500 + (gen() % 500);
                        vec.resize(new_size, t);
                        break;
                     }
                     case 8: { // Reserve
                        size_t new_cap = 1000 + (gen() % 1000);
                        vec.reserve(new_cap);
                        break;
                     }
                     case 9: { // Clear occasionally
                        if (gen() % 100 == 0) { // Rare
                           vec.clear();
                        }
                        break;
                     }
                  }
                  operation_count++;
               } catch (const std::exception& e) {
                  expect(false) << e.what();
               }
            }
         });
      }
      
      // Run for a longer time to stress test
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      stop = true;
      
      for (auto& t : threads) {
         t.join();
      }
   };
};
*/
int main() {}
