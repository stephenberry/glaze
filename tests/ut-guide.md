# How to use the ut library

The `ut` library is a lightweight, header-only C++ testing framework that uses a declarative style with lambdas to define test suites and test cases. This guide explains how to use the library to write effective unit tests.

## Basic Structure

The general structure for writing tests with the `ut` library looks like this:

```cpp
#include "ut/ut.hpp"
using namespace ut;

suite my_test_suite = [] {
    "test_name"_test = [] {
        // Test code goes here
        // Use expect() to verify conditions
    };
    
    "another_test"_test = [] {
        // Another test case
    };
};

int main() {}
```

## Creating Test Suites

A test suite is a logical grouping of related test cases:

```cpp
suite string_tests = [] {
    // String-related test cases go here
};

suite math_tests = [] {
    // Math-related test cases go here
};
```

## Writing Test Cases

Test cases are defined with a string literal followed by the `_test` suffix, assigned to a lambda function:

```cpp
"addition"_test = [] {
    expect(2 + 2 == 4) << "Addition should work correctly\n";
};
```

The string before `_test` should be descriptive of what you're testing.

## Making Assertions

The primary verification method is the `expect()` function:

```cpp
expect(condition) << "Optional message to display if the condition fails\n";
```

Examples:

```cpp
expect(result == expected) << "Result should equal expected value\n";
expect(vec.empty()) << "Vector should be empty after clear\n";
expect(a < b) << "a should be less than b\n";
expect(not is_empty) << "Container should not be empty\n";
```

## Testing Exceptions

To verify that code throws an exception:

```cpp
expect(throws([&] {
    // Code that should throw an exception
    function_that_throws();
})) << "Function should throw an exception\n";
```

To verify that code doesn't throw:

```cpp
expect(not throws([&] {
    // Code that should not throw
    safe_function();
})) << "Function should not throw an exception\n";
```

## Detailed Examples

### Testing a Simple Class

```cpp
class Counter {
public:
    Counter() = default;
    explicit Counter(int initial) : count(initial) {}
    
    void increment() { ++count; }
    void decrement() { --count; }
    int get() const { return count; }
    
private:
    int count = 0;
};

suite counter_tests = [] {
    "default_constructor"_test = [] {
        Counter c;
        expect(c.get() == 0) << "Default constructor should initialize to 0\n";
    };
    
    "value_constructor"_test = [] {
        Counter c(42);
        expect(c.get() == 42) << "Value constructor should initialize to given value\n";
    };
    
    "increment"_test = [] {
        Counter c(10);
        c.increment();
        expect(c.get() == 11) << "Increment should add 1 to count\n";
    };
    
    "decrement"_test = [] {
        Counter c(10);
        c.decrement();
        expect(c.get() == 9) << "Decrement should subtract 1 from count\n";
    };
};
```

### Testing Complex Class Behavior

```cpp
suite complex_behavior_tests = [] {
    "string_operations"_test = [] {
        std::string s = "Hello";
        s.append(", World!");
        
        expect(s == "Hello, World!") << "String append should work correctly\n";
        expect(s.length() == 13) << "String length should be updated after append\n";
        expect(s.find("World") == 7) << "find() should return correct position\n";
    };
    
    "vector_operations"_test = [] {
        std::vector<int> vec;
        
        expect(vec.empty()) << "New vector should be empty\n";
        
        vec.push_back(42);
        expect(vec.size() == 1) << "Size should be 1 after adding an element\n";
        expect(vec[0] == 42) << "Element should be accessible by index\n";
        
        vec.clear();
        expect(vec.empty()) << "Vector should be empty after clear\n";
    };
};
```

### Testing Thread Safety

```cpp
suite thread_safety_tests = [] {
    "concurrent_increment"_test = []() mutable {
        std::atomic<int> counter{0};
        
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
        
        expect(counter.load() == 10000) << "Concurrent increments should result in correct count\n";
    };
};
```

## Best Practices

1. **Use Descriptive Test Names**: Name your tests clearly to describe what they're testing.
   ```cpp
   "string_comparison_case_sensitive"_test = [] { /* ... */ };
   ```

2. **One Assertion per Test**: Ideally focus each test on verifying a single behavior.
   ```cpp
   "vector_size_after_push"_test = [] { /* ... */ };
   "vector_access_after_push"_test = [] { /* ... */ };
   ```

3. **Provide Detailed Failure Messages**: Include specific messages to help diagnose test failures.
   ```cpp
   expect(result == 42) << "calculate() should return 42 for input 'x'\n";
   ```

4. **Test Edge Cases**: Include tests for boundary conditions, empty collections, etc.
   ```cpp
   "empty_string_behavior"_test = [] { /* ... */ };
   "division_by_zero"_test = [] { /* ... */ };
   ```

5. **Group Related Tests**: Organize tests into logical suites.
   ```cpp
   suite string_tests = [] { /* string-related tests */ };
   suite math_tests = [] { /* math-related tests */ };
   ```

6. **Test Thread Safety**: For concurrent code, verify thread-safe behavior.
   ```cpp
   "concurrent_read_write"_test = [] { /* ... */ };
   ```

## Running Tests

The `ut` library is designed to be simple to use. To run your tests:

1. Compile your test file(s) including the `ut.hpp` header
2. Run the resulting executable
3. The library handles test execution and reports any failures

The `main()` function can be left empty as shown above, because the test library initializes and runs the tests automatically.

```cpp
int main() {}
```

By following these guidelines, you can write effective, maintainable unit tests with the `ut` library.
