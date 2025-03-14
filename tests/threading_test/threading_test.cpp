// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze_exceptions.hpp"
#include "glaze/thread/atom.hpp"
#include "ut/ut.hpp"

#include <thread>

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
      struct Point {
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

int main() {}
