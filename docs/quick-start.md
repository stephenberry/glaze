# Glaze Quick Start Guide

Glaze is one of the fastest JSON libraries in the world, featuring compile-time reflection that lets you serialize C++ structs without writing any metadata or macros.

## Basic Reflection

### Simple Struct Serialization

```cpp
#include "glaze/glaze.hpp"

struct Person {
    int age = 25;
    std::string name = "John";
    double height = 5.9;
};

int main() {
    Person person{30, "Alice", 5.7};
    
    // Write to JSON
    std::string json = glz::write_json(person).value_or("error");
    // Result: {"age":30,"name":"Alice","height":5.7}
    
    // Read from JSON
    std::string input = R"({"age":25,"name":"Bob","height":6.1})";
    Person new_person{};
    auto error = glz::read_json(new_person, input);
    if (error) {
       std::string error_msg = glz::format_error(error, input);
       std::cout << error_msg << std::endl;
    }
    else {
       // Success! new_person is now populated
       std::cout << new_person.name << " is " << new_person.age << " years old\n";
    }
    
    return 0;
}
```

### Alternative Expected Handling

```cpp
// Using std::expected return values
auto result = glz::read_json<Person>(input);
if (result) {
    Person person = result.value();
    // Use person...
} else {
    // Handle error
    std::string error_msg = glz::format_error(result, input);
    std::cout << error_msg << std::endl;
}
```

## Custom Field Names

Use `glz::meta` to customize JSON field names:

```cpp
struct Product {
    int id;
    std::string name;
    double price;
};

template <>
struct glz::meta<Product> {
    using T = Product;
    static constexpr auto value = glz::object(
        "product_id", &T::id,
        "product_name", &T::name,
        "unit_price", &T::price
    );
};

// JSON will use: {"product_id":123,"product_name":"Widget","unit_price":9.99}
```

## Containers and Standard Types

Glaze automatically handles standard containers:

```cpp
struct Data {
    std::vector<int> numbers = {1, 2, 3};
    std::map<std::string, std::string> config = {{"key", "value"}};
    std::array<double, 3> coordinates = {1.0, 2.0, 3.0};
    std::optional<std::string> description; // null if empty (output skipped by default)
};

// JSON: {"numbers":[1,2,3],"config":{"key":"value"},"coordinates":[1,2,3]}
```

## Enums as Strings

```cpp
enum class Status { Active, Inactive, Pending };

template <>
struct glz::meta<Status> {
    using enum Status;
    static constexpr auto value = glz::enumerate(Active, Inactive, Pending);
};

struct Task {
    std::string name;
    Status status = Status::Pending;
};

// JSON: {"name":"My Task","status":"Pending"}
```

## File I/O

```cpp
Person person{25, "Charlie", 5.8};

// Write to file
auto write_error = glz::write_file_json(person, "./person.json", std::string{});

// Read from file  
Person loaded_person{};
auto read_error = glz::read_file_json(loaded_person, "./person.json", std::string{});
```

## Pretty Printing

```cpp
Person person{25, "Diana", 5.6};

// Write prettified JSON
std::string pretty_json;
auto error = glz::write<glz::opts{.prettify = true}>(person, pretty_json);

// Or prettify existing JSON
std::string minified = R"({"age":25,"name":"Diana","height":5.6})";
std::string beautiful = glz::prettify_json(minified);
```

## Optional Fields and Null Handling

```cpp
struct User {
    std::string username;
    std::optional<std::string> email;  // Can be null
    std::unique_ptr<std::string> bio;  // Can be null
};

User user{"alice", std::nullopt, nullptr};

// With skip_null_members (default: true)
// JSON: {"username":"alice"}

// With skip_null_members = false
std::string json;
glz::write<glz::opts{.skip_null_members = false}>(user, json);
// JSON: {"username":"alice","email":null,"bio":null}
```

## Variants

```cpp
struct Circle { double radius; };
struct Rectangle { double width, height; };

using Shape = std::variant<Circle, Rectangle>;

struct Drawing {
    std::string name;
    Shape shape;
};

// Glaze automatically handles variants based on which fields are present
```

## Error Handling with Details

```cpp
std::string invalid_json = R"({"name": "test", "age": "not_a_number"})";
Person person{};
auto error = glz::read_json(person, invalid_json);

if (error) {
    std::string detailed_error = glz::format_error(error, invalid_json);
    std::cout << detailed_error << std::endl;
    // Shows exactly where the error occurred with context
}
```

## Common Options

```cpp
// Read with comments support (JSONC)
auto error = glz::read<glz::opts{.comments = true}>(obj, json_with_comments);

// Allow unknown keys (don't error on extra fields)
auto error = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, json);

// Require all keys to be present
auto error = glz::read<glz::opts{.error_on_missing_keys = true}>(obj, json);

// Write with custom indentation
auto error = glz::write<glz::opts{.prettify = true, .indentation_width = 2}>(obj, json);
```

## Performance Tips

1. **Use `std::string` buffers**: Glaze is optimized for `std::string` input/output
3. **Reuse buffers**: Clear and reuse `std::string` buffers instead of creating new ones

## Next Steps

- Explore [advanced features](https://github.com/stephenberry/glaze/tree/main/docs) like JSON pointers, JMESPath queries, and binary formats
- Check out [wrappers](https://github.com/stephenberry/glaze/blob/main/docs/wrappers.md) for fine-grained control
- Learn about [custom serialization](https://github.com/stephenberry/glaze/blob/main/docs/custom-serialization.md) for complex types
