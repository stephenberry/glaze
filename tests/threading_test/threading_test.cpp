// Glaze Library
// For the license information refer to glaze.hpp

#include <cstdint>
#include <random>
#include <thread>

#include "glaze/glaze_exceptions.hpp"
#include "glaze/thread/async_string.hpp"
#include "glaze/thread/async_vector.hpp"
#include "glaze/thread/guard.hpp"
#include "ut/ut.hpp"

using namespace ut;

static_assert(glz::is_atomic<glz::guard<int>>);
static_assert(glz::read_supported<glz::guard<int>, glz::JSON>);
static_assert(glz::read_supported<glz::guard<int>, glz::JSON>);

suite atom_tests = [] {
   "construction"_test = [] {
      glz::guard<int> a;
      expect(a.load() == 0) << "Default constructor should initialize to 0";

      glz::guard<int> b(42);
      expect(b.load() == 42) << "Value constructor should initialize to given value";

      glz::guard<double> c(3.14);
      expect(c.load() == 3.14) << "Should work with floating point types";
   };

   "copy_semantics"_test = [] {
      glz::guard<int> a(42);
      glz::guard<int> b = a;
      expect(b.load() == 42) << "Copy constructor should copy the value";

      glz::guard<int> c(10);
      c = a;
      expect(c.load() == 42) << "Copy assignment should copy the value";

      glz::guard<int> d;
      d = 100;
      expect(d.load() == 100) << "Assignment from T should store the value";
   };

   "move_semantics"_test = [] {
      glz::guard<int> a(42);
      glz::guard<int> b = std::move(a);
      expect(b.load() == 42) << "Move constructor should copy the value";
      expect(a.load() == 42) << "Source should still have its value after move";

      glz::guard<int> c(10);
      c = std::move(a);
      expect(c.load() == 42) << "Move assignment should copy the value";
      expect(a.load() == 42) << "Source should still have its value after move";
   };

   "comparison_with_atom"_test = [] {
      glz::guard<int> a(42);
      glz::guard<int> b(42);
      glz::guard<int> c(100);

      expect(a == b) << "Equal atoms should compare equal";
      expect(a != c) << "Different atoms should not compare equal";
      expect(a < c) << "Less than should work";
      expect(a <= b) << "Less than or equal should work";
      expect(c > a) << "Greater than should work";
      expect(b >= a) << "Greater than or equal should work";
   };

   "comparison_with_value"_test = [] {
      glz::guard<int> a(42);

      expect(a == 42) << "Atom should compare equal with equal value";
      expect(a != 100) << "Atom should not compare equal with different value";
      expect(a < 100) << "Less than should work with value";
      expect(a <= 42) << "Less than or equal should work with value";
      expect(a > 10) << "Greater than should work with value";
      expect(a >= 42) << "Greater than or equal should work with value";

      expect(42 == a) << "Value should compare equal with equal glz::guard";
      expect(100 != a) << "Value should not compare equal with different glz::guard";
      expect(10 < a) << "Less than should work with value on left";
      expect(42 <= a) << "Less than or equal should work with value on left";
      expect(100 > a) << "Greater than should work with value on left";
      expect(42 >= a) << "Greater than or equal should work with value on left";
   };

   "load_store"_test = [] {
      glz::guard<int> a(42);
      expect(a.load() == 42) << "Load should return current value";

      a.store(100);
      expect(a.load() == 100) << "Store should update the value";

      expect(static_cast<int>(a) == 100) << "Conversion operator should return the value";
   };

   "exchange"_test = [] {
      glz::guard<int> a(42);
      int old = a.exchange(100);

      expect(old == 42) << "Exchange should return old value";
      expect(a.load() == 100) << "Exchange should update the value";
   };

   "compare_exchange"_test = [] {
      glz::guard<int> a(42);

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
      glz::guard<int> a(42);

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
      glz::guard<int> a(0b1100);

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
      glz::guard<int> counter(0);

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

      glz::guard<Point> a(p1);
      glz::guard<Point> b(p2);
      glz::guard<Point> c(p3);

      expect(a == b) << "Atoms with structs should compare correctly";
      expect(a != c) << "Atoms with structs should compare correctly";
      expect(a == p1) << "Atom should compare with raw struct value";
   };

   "memory_order"_test = [] {
      glz::guard<int> a(42);

      int v1 = a.load(std::memory_order_relaxed);
      expect(v1 == 42) << "Load with relaxed memory order should work";

      a.store(100, std::memory_order_release);
      int v2 = a.load(std::memory_order_acquire);
      expect(v2 == 100) << "Store with release and load with acquire should work";

      int old = a.exchange(200, std::memory_order_acq_rel);
      expect(old == 100) << "Exchange with acq_rel memory order should work";
      expect(a.load() == 200) << "Value should be updated";
   };

   "json read/write"_test = [] {
      glz::guard<int> a(42);

      std::string buffer{};
      expect(not glz::write_json(a, buffer));
      expect(buffer == "42");

      a = 100;
      expect(not glz::read_json(a, buffer));
      expect(a == 42);
   };
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

   // Tests for proxy size/capacity methods
   "proxy size and capacity methods"_test = [] {
      glz::async_string s("Hello, world!");
      auto p = s.write();

      expect(p.size() == 13);
      expect(p.length() == 13);
      expect(p.max_size() > 0);
      expect(p.capacity() >= p.size());
      expect(!p.empty());

      p.reserve(100);
      expect(p.capacity() >= 100);

      p.shrink_to_fit();
      expect(p.capacity() >= p.size());

      glz::async_string empty_str;
      auto empty_p = empty_str.write();
      expect(empty_p.empty());
      expect(empty_p.size() == 0);
   };

   // Tests for proxy element access methods
   "proxy element access methods"_test = [] {
      glz::async_string s("Test string");
      auto p = s.write();

      expect(p[0] == 'T');
      expect(p.at(1) == 'e');
      expect(p.front() == 'T');
      expect(p.back() == 'g');

      // data() and c_str() should return valid pointers
      expect(p.data() != nullptr);
      expect(p.c_str() != nullptr);

      // Content should match
      expect(std::string(p.data()) == "Test string");
      expect(std::string(p.c_str()) == "Test string");

      // Null termination for c_str()
      expect(p.c_str()[p.size()] == '\0');

      // Check out_of_range for at()
      expect(throws([&] { (void)p.at(100); }));

      // Modifying through operator[]
      p[0] = 'B';
      expect(p[0] == 'B');
      expect(*p == "Best string");
   };

   // Tests for proxy string operations
   "proxy string operations"_test = [] {
      glz::async_string s("Hello, world!");
      auto p = s.write();

      // compare
      expect(p.compare("Hello, world!") == 0);
      expect(p.compare("Hello") > 0);
      expect(p.compare("Zebra") < 0);
      expect(p.compare(std::string("Hello, world!")) == 0);
      expect(p.compare(std::string_view("Hello, world!")) == 0);
      expect(p.compare(0, 5, std::string("Hello")) == 0);

      // substr
      expect(p.substr(0, 5) == "Hello");
      expect(p.substr(7, 5) == "world");
      expect(p.substr(7) == "world!");

      // starts_with
      expect(p.starts_with("Hello"));
      expect(p.starts_with(std::string_view("Hello")));
      expect(p.starts_with('H'));
      expect(!p.starts_with("hello")); // case sensitive

      // ends_with
      expect(p.ends_with("world!"));
      expect(p.ends_with(std::string_view("world!")));
      expect(p.ends_with('!'));
      expect(!p.ends_with("World!")); // case sensitive
   };

   // Tests for proxy search operations
   "proxy search operations"_test = [] {
      glz::async_string s("Hello, world! Hello again!");
      auto p = s.write();

      // find
      expect(p.find("world") == 7);
      expect(p.find('w') == 7);
      expect(p.find("notfound") == std::string::npos);
      expect(p.find("Hello", 1) == 14);
      expect(p.find(std::string("world")) == 7);
      expect(p.find(std::string_view("world")) == 7);
      expect(p.find("o", 0, 1) == 4);

      // rfind
      expect(p.rfind("Hello") == 14);
      expect(p.rfind('!') == 25);
      expect(p.rfind("notfound") == std::string::npos);
      expect(p.rfind(std::string("Hello")) == 14);
      expect(p.rfind(std::string_view("Hello")) == 14);

      // find_first_of
      expect(p.find_first_of("aeiou") == 1); // 'e' in "Hello"
      expect(p.find_first_of('e') == 1);
      expect(p.find_first_of("xyz") == std::string::npos);
      expect(p.find_first_of(std::string("aeiou")) == 1);
      expect(p.find_first_of(std::string_view("aeiou")) == 1);
      expect(p.find_first_of("aeiou", 5) == 8); // 'o' in "world"

      // find_last_of
      expect(p.find_last_of("aeiou") == 23); // 'i' in "again"
      expect(p.find_last_of('n') == 24);
      expect(p.find_last_of("xyz") == std::string::npos);

      // find_first_not_of
      expect(p.find_first_not_of("Helo") == 5); // ',' after "Hello"
      expect(p.find_first_not_of('H') == 1);

      // find_last_not_of
      expect(p.find_last_not_of("!") == 24); // 'n' before final '!'
      expect(p.find_last_not_of(" !") == 24); // 'n' before final '!'
   };

   // Tests for proxy modifiers
   "proxy advanced modifiers"_test = [] {
      glz::async_string s("Hello, world!");

      // Testing clear
      {
         auto p = s.write();
         p.clear();
         expect(p.empty());
         expect(p.size() == 0);
      }

      // Testing resize
      {
         s = "Resize me";
         auto p = s.write();

         // Shrink
         p.resize(7);
         expect(*p == "Resize ");
         expect(p.size() == 7);

         // Expand with default char
         p.resize(10);
         expect(p.size() == 10);
         expect(p[7] == '\0');

         // Expand with specified char
         p.resize(15, '!');
         expect(p.size() == 15);
         expect(p[10] == '!');
         expect(p[14] == '!');
      }

      // Testing push_back and pop_back
      {
         s = "Test";
         auto p = s.write();

         p.push_back('!');
         expect(*p == "Test!");

         p.pop_back();
         expect(*p == "Test");

         // Multiple operations
         p.push_back('1');
         p.push_back('2');
         p.push_back('3');
         expect(*p == "Test123");

         p.pop_back();
         p.pop_back();
         expect(*p == "Test1");
      }

      // Testing append and operator+=
      {
         s = "Hello";
         auto p = s.write();

         p.append(", ");
         expect(*p == "Hello, ");

         p.append(std::string("world"));
         expect(*p == "Hello, world");

         p.append(std::string_view("!"));
         expect(*p == "Hello, world!");

         p.append(" How", 4);
         expect(*p == "Hello, world! How");

         p.append(3, '!');
         expect(*p == "Hello, world! How!!!");

         // Testing operator+=
         p += " Testing";
         expect(*p == "Hello, world! How!!! Testing");

         p += std::string(" operator");
         expect(*p == "Hello, world! How!!! Testing operator");

         p += std::string_view(" +=");
         expect(*p == "Hello, world! How!!! Testing operator +=");

         p += '!';
         expect(*p == "Hello, world! How!!! Testing operator +=!");
      }

      // Testing swap
      {
         s = "First string";
         std::string other = "Second string";
         auto p = s.write();

         p.swap(other);
         expect(*p == "Second string");
         expect(other == "First string");
      }
   };

   // Tests for const_proxy methods
   "const_proxy methods"_test = [] {
      const glz::async_string s("This is a const test string!");
      auto cp = s.read();

      // Size/capacity
      expect(cp.size() == 28);
      expect(cp.length() == 28);
      expect(cp.max_size() > 0);
      expect(cp.capacity() >= cp.size());
      expect(!cp.empty());

      // Element access
      expect(cp[0] == 'T');
      expect(cp.at(1) == 'h');
      expect(cp.front() == 'T');
      expect(cp.back() == '!');
      expect(std::string(cp.data()) == "This is a const test string!");
      expect(std::string(cp.c_str()) == "This is a const test string!");

      // String operations
      expect(cp.compare("This is a const test string!") == 0);
      expect(cp.compare("This") > 0);
      expect(cp.compare("Zebra") < 0);
      expect(cp.compare(std::string("This is a const test string!")) == 0);
      expect(cp.compare(std::string_view("This is a const test string!")) == 0);

      expect(cp.substr(0, 4) == "This");
      expect(cp.substr(10, 5) == "const");
      expect(cp.substr(10) == "const test string!");

      expect(cp.starts_with("This"));
      expect(cp.starts_with('T'));
      expect(cp.starts_with(std::string_view("This")));
      expect(!cp.starts_with("this")); // case sensitive

      expect(cp.ends_with("string!"));
      expect(cp.ends_with('!'));
      expect(cp.ends_with(std::string_view("string!")));
      expect(!cp.ends_with("String!")); // case sensitive

      // Search operations
      expect(cp.find("const") == 10);
      expect(cp.find('c') == 10);
      expect(cp.find("notfound") == std::string::npos);
      expect(cp.find(std::string("test")) == 16);
      expect(cp.find(std::string_view("string")) == 21);

      expect(cp.rfind("is") == 5);
      expect(cp.rfind('s') == 21);

      // Testing operator conversions and dereference
      std::string_view sv = cp;
      expect(sv == "This is a const test string!");

      const std::string& ref = *cp;
      expect(ref == "This is a const test string!");

      const std::string* ptr = cp.operator->();
      expect(*ptr == "This is a const test string!");

      // Test .value() method still works
      expect(cp.value() == "This is a const test string!");
   };

   // Test proxy chain operations (ensuring method chaining works)
   "proxy method chaining"_test = [] {
      glz::async_string s("Start");

      // Chain several operations
      auto p = s.write();
      p.append(" ").append("with").append(" chaining").replace(0, 5, "Begin");
      expect(*p == "Begin with chaining");
   };
};

struct TestObject
{
   int id = 0;
   std::string name = "default";

   TestObject() = default;
   TestObject(int id, std::string name) : id(id), name(std::move(name)) {}

   bool operator==(const TestObject& other) const { return id == other.id && name == other.name; }
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

   "compare"_test = [] {
      glz::async_vector<int> v1;
      v1.push_back(10);
      v1.push_back(20);
      v1.push_back(30);
      glz::async_vector v2 = v1;

      expect(v1 == v2);

      std::vector<int> std_vec = {10, 20, 30};
      expect(v1 == std_vec);
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
      }
      catch (const std::out_of_range&) {
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

      vec.resize(5);
      expect(vec.size() == 5) << "Resize should change size";

      vec.resize(3);
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
      expect(vec.read()[0] == 0 && vec.read()[1] == 1 && vec.read()[2] == 2)
         << "Elements should be in correct order after insert";

      // Test emplace
      {
         auto proxy = vec.write();
         auto it2 = proxy.emplace(proxy.begin() + 2, 15);
         expect(*it2 == 15) << "Emplace should return iterator to inserted element";
      }
      expect(vec.size() == 4) << "Size should increase after emplace";
      expect(vec.read()[0] == 0 && vec.read()[1] == 1 && vec.read()[2] == 15 && vec.read()[3] == 2)
         << "Elements should be in correct order after emplace";

      // Test erase
      {
         auto proxy = vec.write();
         auto it3 = proxy.erase(proxy.begin() + 1);
         expect(*it3 == 15) << "Erase should return iterator to element after erased";
      }
      expect(vec.size() == 3) << "Size should decrease after erase";
      expect(vec.read()[0] == 0 && vec.read()[1] == 15 && vec.read()[2] == 2)
         << "Elements should be in correct order after erase";

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

      expect(vec1.read()[0] == 3 && vec1.read()[1] == 4 && vec1.read()[2] == 5)
         << "First vector should have elements of second vector";
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
      std::atomic<size_t> sum = 0;
      for (int i = 0; i < 5; ++i) {
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

// Complex data type with move/copy semantics verification
struct ComplexObject
{
   int id;
   std::string data;
   std::vector<double> values;
   std::unique_ptr<int> ptr;
   bool was_moved = false;
   std::atomic<int> access_count{0};

   ComplexObject(int id, std::string data, std::vector<double> values)
      : id(id), data(std::move(data)), values(std::move(values)), ptr(std::make_unique<int>(id))
   {}

   ComplexObject() : id(0), ptr(std::make_unique<int>(0)) {}

   // Copy constructor
   ComplexObject(const ComplexObject& other)
      : id(other.id),
        data(other.data),
        values(other.values),
        ptr(other.ptr ? std::make_unique<int>(*other.ptr) : nullptr),
        access_count(other.access_count.load())
   {}

   // Move constructor
   ComplexObject(ComplexObject&& other) noexcept
      : id(other.id),
        data(std::move(other.data)),
        values(std::move(other.values)),
        ptr(std::move(other.ptr)),
        was_moved(true),
        access_count(other.access_count.load())
   {}

   // Copy assignment
   ComplexObject& operator=(const ComplexObject& other)
   {
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
   ComplexObject& operator=(ComplexObject&& other) noexcept
   {
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

   void access() { access_count++; }

   bool operator==(const ComplexObject& other) const
   {
      return id == other.id && data == other.data && values == other.values &&
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
            auto proxy = vec.read();
            auto it1 = proxy.begin();
            [[maybe_unused]] auto it2 = proxy.begin();
            auto it3 = proxy.end();
            [[maybe_unused]] auto it4 = proxy.cbegin();
            [[maybe_unused]] auto it5 = proxy.cend();

            // Test iterator arithmetic and comparisons
            try {
               auto it_mid = it1 + proxy.size() / 2;
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
                  [[maybe_unused]] auto temp_it = proxy.begin() + i;
                  [[maybe_unused]] auto temp_cit = proxy.cbegin() + i;
               }
            }
            catch (const std::exception&) {
               errors++;
            }
         }
      });

      // Thread that reads through iterators
      threads.emplace_back([&]() {
         while (!stop) {
            try {
               int64_t sum = 0; // use wider type to avoid overflow on fast machines
               {
                  auto proxy = vec.read();
                  for (auto it = proxy.begin(); it != proxy.end(); ++it) {
                     sum += static_cast<int64_t>(*it);
                  }
               }

               // Basic validation check
               if (sum < 0) {
                  errors++;
               }
            }
            catch (const std::exception&) {
               errors++;
            }
         }
      });

      // Thread that modifies through iterators
      threads.emplace_back([&]() {
         int counter = 0;
         while (!stop) {
            try {
               auto proxy = vec.write();
               auto it = proxy.begin() + (counter % proxy.size());
               *it = counter;
               counter++;
            }
            catch (const std::exception&) {
               errors++;
            }
         }
      });

      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
                  thread_local std::mt19937 rng(std::random_device{}());
                  size_t upper = vec.size();
                  std::uniform_int_distribution<size_t> pos_dist(0, upper);
                  size_t pos = pos_dist(rng);
                  {
                     auto proxy = vec.write();
                     auto insert_pos = proxy.begin();
                     std::advance(insert_pos, (std::min)(pos, proxy.size()));

                     // Insert a new value
                     auto it = proxy.insert(insert_pos, t * 1000 + i);

                     // Verify the inserted value
                     if (*it != t * 1000 + i) {
                        errors++;
                     }
                  }

                  // Small delay to increase chance of contention
                  std::uniform_int_distribution<int> us_dist(0, 99);
                  std::this_thread::sleep_for(std::chrono::microseconds(us_dist(rng)));

                  // Pick another random position for erasure
                  if (vec.size() > 0) {
                     size_t cur = vec.size();
                     std::uniform_int_distribution<size_t> erase_dist(0, cur - 1);
                     size_t erase_pos = erase_dist(rng);
                     auto proxy = vec.write();
                     auto erase_iter = proxy.begin();
                     std::advance(erase_iter, erase_pos);
                     proxy.erase(erase_iter);
                  }
               }
               catch (const std::exception&) {
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
                  thread_local std::mt19937 rng(std::random_device{}());
                  size_t upper = vec.size();
                  std::uniform_int_distribution<size_t> pos_dist(0, upper);
                  size_t pos = pos_dist(rng);
                  {
                     auto proxy = vec.write();
                     auto insert_pos = proxy.begin();
                     std::advance(insert_pos, (std::min)(pos, proxy.size()));

                     // Create a string and vector for the complex object
                     std::string data = "Thread " + std::to_string(t) + " Item " + std::to_string(i);
                     std::vector<double> values;
                     for (int j = 0; j < 5; ++j) {
                        values.push_back(t + i + j / 10.0);
                     }

                     // Emplace a new complex object
                     auto it = proxy.emplace(insert_pos, t * 1000 + i, data, values);

                     // Verify the emplaced object
                     if (it->id != t * 1000 + i || it->data != data) {
                        errors++;
                     }
                  }

                  // Small delay to increase chance of contention
                  std::uniform_int_distribution<int> us_dist(0, 99);
                  std::this_thread::sleep_for(std::chrono::microseconds(us_dist(rng)));
               }
               catch (const std::exception&) {
                  errors++;
               }
            }
         });
      }

      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      stop = true;

      for (auto& t : threads) {
         t.join();
      }

      expect(errors == 0) << "Concurrent emplace operations should not produce errors";
      expect(vec.size() > 0) << "Vector should contain elements after emplace operations";

      // Validate all objects
      for (const auto& obj : vec.read()) {
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
                  auto proxy = vec.write();
                  auto it1 = proxy.begin() + (t * 100 % proxy.size());
                  int val1 = *it1;

                  // Use that iterator in an insert operation (potential deadlock scenario)
                  auto new_it = proxy.insert(it1, val1 * 2);

                  // Now use the new iterator in another operation
                  auto another_it = proxy.begin() + (proxy.size() / 2);
                  auto erased_it = proxy.erase(another_it);

                  // Try to use all three iterators
                  if (*it1 >= 0 && *new_it >= 0 && (erased_it == proxy.end() || *erased_it >= 0)) {
                     completed_operations++;
                  }
               }
               catch (const std::exception&) {
                  // Error, but not necessarily a deadlock
               }
            }
         });
      }

      // Run for enough time to detect deadlocks
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
         std::vector<double> values{static_cast<double>(i), static_cast<double>(i * 2)};
         vec.emplace_back(i, "Object " + std::to_string(i), values);
      }

      std::atomic<bool> stop{false};
      std::deque<std::thread> threads;
      std::atomic<int> errors{0};

      // Thread that creates iterators and performs random jumps
      threads.emplace_back([&]() {
         auto proxy = vec.write();
         auto it = proxy.begin();
         while (!stop) {
            try {
               thread_local std::mt19937 rng(std::random_device{}());
               std::uniform_int_distribution<int> jump_dist(-10, 9);
               int jump = jump_dist(rng); // Random jump forward or backward
               if (it + jump >= proxy.begin() && it + jump < proxy.end()) {
                  it += jump;
                  it->access(); // Access the object to increment counter
               }
            }
            catch (const std::exception&) {
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
                  auto proxy = vec.write();
                  proxy.erase(proxy.begin());
               }
               else if (counter % 3 == 1) {
                  // Add to end
                  std::vector<double> values{static_cast<double>(counter)};
                  vec.emplace_back(1000 + counter, "New " + std::to_string(counter), values);
               }
               else if (vec.size() > 0) {
                  // Replace middle
                  size_t mid = vec.size() / 2;
                  auto proxy = vec.write();
                  auto mid_it = proxy.begin() + mid;
                  std::vector<double> values{static_cast<double>(counter * 10)};
                  *mid_it = ComplexObject(2000 + counter, "Replaced", values);
               }
               counter++;
               std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            catch (const std::exception&) {
               errors++;
            }
         }
      });

      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      stop = true;

      for (auto& t : threads) {
         t.join();
      }

      expect(errors < 10) << "Iterator races should be handled gracefully";
      // We expect some errors due to race conditions, but not too many
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
               }
               else {
                  size -= 100;
                  if (size < 500) growing = true;
               }

               vec.write().resize(size, 42);
               resize_count++;
               std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            catch (const std::exception&) {
               errors++;
            }
         }
      });

      // Threads that read the vector during resizing
      std::atomic<uint64_t> sum = 0;
      for (int t = 0; t < 3; ++t) {
         threads.emplace_back([&]() {
            while (!stop) {
               try {
                  size_t current_size = vec.size();
                  auto proxy = vec.read();
                  for (size_t i = 0; i < current_size && i < proxy.size(); ++i) {
                     sum.fetch_add(static_cast<uint64_t>(proxy[i]), std::memory_order_relaxed);
                  }
               }
               catch (const std::exception&) {
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
                     auto proxy = vec.write();
                     if (idx < proxy.size()) {
                        proxy[idx] = counter;
                     }
                  }
                  counter++;
               }
               catch (const std::exception&) {
                  errors++;
               }
            }
         });
      }

      // Run for a short time
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
                     auto proxy = vec.write();
                     if (!proxy.empty()) {
                        proxy.pop_back();
                     }
                     break;
                  }
                  case 2: { // Read random element
                     auto proxy = vec.read();
                     if (!proxy.empty()) {
                        size_t idx = gen() % proxy.size();
                        [[maybe_unused]] volatile size_t val = proxy[idx]; // Force read
                     }
                     break;
                  }
                  case 3: { // Modify random element
                     auto proxy = vec.write();
                     if (!proxy.empty()) {
                        size_t idx = gen() % proxy.size();
                        proxy[idx] = t;
                     }
                     break;
                  }
                  case 4: { // Insert at random position
                     auto proxy = vec.write();
                     size_t pos = proxy.empty() ? 0 : (gen() % proxy.size());
                     auto it = proxy.begin();
                     std::advance(it, (std::min)(pos, proxy.size()));
                     proxy.insert(it, t);
                     break;
                  }
                  case 5: { // Erase at random position
                     auto proxy = vec.write();
                     if (!proxy.empty()) {
                        size_t pos = gen() % proxy.size();
                        auto it = proxy.begin();
                        std::advance(it, pos);
                        proxy.erase(it);
                     }
                     break;
                  }
                  case 6: { // Iterate through
                     [[maybe_unused]] volatile size_t count = 0;
                     for (const auto& val : vec.read()) {
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
               }
               catch (const std::exception& e) {
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

struct Point
{
   int x;
   int y;

   bool operator==(const Point& other) const { return x == other.x && y == other.y; }

   struct glaze
   {
      using T = Point;
      static constexpr auto value = glz::object("x", &T::x, "y", &T::y);
   };
};

struct User
{
   std::string name;
   int age;
   std::vector<std::string> hobbies;

   bool operator==(const User& other) const
   {
      return name == other.name && age == other.age && hobbies == other.hobbies;
   }

   struct glaze
   {
      using T = User;
      static constexpr auto value = glz::object("name", &T::name, "age", &T::age, "hobbies", &T::hobbies);
   };
};

suite async_vector_json_tests = [] {
   // JSON serialization/deserialization tests
   "async_vector write_json / read_json"_test = [] {
      glz::async_vector<int> v;
      v.push_back(1);
      v.push_back(2);
      v.push_back(3);
      v.push_back(4);
      v.push_back(5);

      std::string buffer{};

      // write_json returns a status code: false means success, true means error
      expect(not glz::write_json(v, buffer)) << "Failed to serialize async_vector";
      // The JSON for a vector of integers should be an array of numbers
      expect(buffer == "[1,2,3,4,5]") << buffer;

      glz::async_vector<int> result;
      expect(not glz::read_json(result, buffer)) << "Failed to deserialize async_vector";

      auto result_reader = result.read();
      expect(result_reader->size() == 5);
      expect((*result_reader)[0] == 1);
      expect((*result_reader)[1] == 2);
      expect((*result_reader)[2] == 3);
      expect((*result_reader)[3] == 4);
      expect((*result_reader)[4] == 5);
   };

   // Test an empty vector's serialization
   "async_vector empty serialization"_test = [] {
      glz::async_vector<int> v;
      std::string buffer{};

      expect(not glz::write_json(v, buffer));
      // An empty array in JSON
      expect(buffer == "[]") << buffer;

      glz::async_vector<int> result;
      result.push_back(99);
      result.push_back(100); // Non-empty to start
      expect(not glz::read_json(result, buffer));
      expect(result.empty());
   };

   // Test serialization with custom objects
   "async_vector custom object serialization"_test = [] {
      glz::async_vector<Point> points;
      points.push_back({1, 2});
      points.push_back({3, 4});
      points.push_back({5, 6});

      std::string buffer{};
      expect(not glz::write_json(points, buffer)) << "Failed to serialize custom objects in async_vector";

      glz::async_vector<Point> result;
      expect(not glz::read_json(result, buffer)) << "Failed to deserialize custom objects in async_vector";

      auto result_reader = result.read();
      expect(result_reader->size() == 3);
      expect((*result_reader)[0] == Point{1, 2});
      expect((*result_reader)[1] == Point{3, 4});
      expect((*result_reader)[2] == Point{5, 6});
   };

   // Test serialization with nested async_vector
   "async_vector nested serialization"_test = [] {
      glz::async_vector<glz::async_vector<int>> nested;

      // Create and add inner vectors
      glz::async_vector<int> inner1;
      inner1.push_back(1);
      inner1.push_back(2);
      inner1.push_back(3);

      glz::async_vector<int> inner2;
      inner2.push_back(4);
      inner2.push_back(5);

      glz::async_vector<int> inner3;
      inner3.push_back(6);

      nested.push_back(std::move(inner1));
      nested.push_back(std::move(inner2));
      nested.push_back(std::move(inner3));

      std::string buffer{};
      expect(not glz::write_json(nested, buffer)) << "Failed to serialize nested async_vector";

      // JSON should be a nested array
      expect(buffer == "[[1,2,3],[4,5],[6]]") << buffer;

      glz::async_vector<glz::async_vector<int>> result;
      expect(not glz::read_json(result, buffer)) << "Failed to deserialize nested async_vector";

      auto result_reader = result.read();
      expect(result_reader->size() == 3);

      auto inner1_reader = (*result_reader)[0].read();
      expect(inner1_reader->size() == 3);
      expect((*inner1_reader)[0] == 1);
      expect((*inner1_reader)[1] == 2);
      expect((*inner1_reader)[2] == 3);

      auto inner2_reader = (*result_reader)[1].read();
      expect(inner2_reader->size() == 2);
      expect((*inner2_reader)[0] == 4);
      expect((*inner2_reader)[1] == 5);

      auto inner3_reader = (*result_reader)[2].read();
      expect(inner3_reader->size() == 1);
      expect((*inner3_reader)[0] == 6);
   };

   // Test with more complex JSON structures
   "async_vector complex JSON structures"_test = [] {
      // Create test data
      glz::async_vector<User> users;
      users.push_back({"Alice", 30, {"reading", "hiking"}});
      users.push_back({"Bob", 25, {"gaming", "coding", "music"}});

      std::string buffer{};
      expect(not glz::write_json(users, buffer)) << "Failed to serialize complex structure";

      // Check JSON format (approximate)
      expect(buffer.find("Alice") != std::string::npos);
      expect(buffer.find("Bob") != std::string::npos);
      expect(buffer.find("reading") != std::string::npos);
      expect(buffer.find("gaming") != std::string::npos);

      // Deserialize
      glz::async_vector<User> result;
      expect(not glz::read_json(result, buffer)) << "Failed to deserialize complex structure";

      auto reader = result.read();
      expect(reader->size() == 2);
      expect((*reader)[0] == User{"Alice", 30, {"reading", "hiking"}});
      expect((*reader)[1] == User{"Bob", 25, {"gaming", "coding", "music"}});
   };

   // Test JSON serialization with pretty print
   "async_vector pretty print JSON"_test = [] {
      glz::async_vector<int> v;
      v.push_back(1);
      v.push_back(2);
      v.push_back(3);

      std::string buffer{};
      expect(not glz::write<glz::opts{.prettify = true}>(v, buffer)) << "Failed to serialize with pretty print";

      // Should include newlines and indentation
      expect(buffer.find("\n") != std::string::npos);
      expect(buffer.find(" ") != std::string::npos);

      // Should still deserialize correctly
      glz::async_vector<int> result;
      expect(not glz::read_json(result, buffer)) << "Failed to deserialize pretty-printed JSON";

      auto reader = result.read();
      expect(reader->size() == 3);
      expect((*reader)[0] == 1);
      expect((*reader)[1] == 2);
      expect((*reader)[2] == 3);
   };
};

suite async_vector_beve_tests = [] {
   "async_vector write_beve / read_beve"_test = [] {
      glz::async_vector<int> v;
      v.push_back(1);
      v.push_back(2);
      v.push_back(3);
      v.push_back(4);
      v.push_back(5);

      std::string buffer{};

      // write_json returns a status code: false means success, true means error
      expect(not glz::write_beve(v, buffer)) << "Failed to serialize async_vector";
      glz::async_vector<int> result;
      expect(not glz::read_beve(result, buffer)) << "Failed to deserialize async_vector";

      auto result_reader = result.read();
      expect(result_reader->size() == 5);
      expect((*result_reader)[0] == 1);
      expect((*result_reader)[1] == 2);
      expect((*result_reader)[2] == 3);
      expect((*result_reader)[3] == 4);
      expect((*result_reader)[4] == 5);
   };

   "async_vector custom object serialization"_test = [] {
      glz::async_vector<Point> points;
      points.push_back({1, 2});
      points.push_back({3, 4});
      points.push_back({5, 6});

      std::string buffer{};
      expect(not glz::write_beve(points, buffer)) << "Failed to serialize custom objects in async_vector";

      glz::async_vector<Point> result;
      expect(not glz::read_beve(result, buffer)) << "Failed to deserialize custom objects in async_vector";

      auto result_reader = result.read();
      expect(result_reader->size() == 3);
      expect((*result_reader)[0] == Point{1, 2});
      expect((*result_reader)[1] == Point{3, 4});
      expect((*result_reader)[2] == Point{5, 6});
   };
};

int main() {}
