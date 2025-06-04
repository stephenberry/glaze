# Rename Keys

The `rename_key` functionality in Glaze allows you to transform struct member names during JSON serialization and deserialization. This powerful feature operates entirely at compile time, directly affecting compile-time hash maps for optimal runtime performance.

## Overview

By default, Glaze uses C++ struct member names as JSON keys. However, you may need different key names in your JSON output to:

- Follow specific naming conventions (e.g., camelCase vs snake_case)
- Interface with external APIs that expect particular key formats
- Add prefixes, suffixes, or other transformations to keys
- Maintain backward compatibility while refactoring struct member names

## Basic Usage

To use `rename_key`, specialize the `glz::meta` template for your struct and implement a `rename_key` function:

```cpp
struct my_struct_t {
    std::string member_name{};
};

template <>
struct glz::meta<my_struct_t> {
    static constexpr std::string_view rename_key(const std::string_view key) {
        // Transform the key as needed
        return transformed_key;
    }
};
```

## Example 1: Specific Key Mapping

Transform specific member names to follow different naming conventions:

```cpp
struct renamed_t {
    std::string first_name{};
    std::string last_name{};
    int age{};
};

template <>
struct glz::meta<renamed_t> {
    static constexpr std::string_view rename_key(const std::string_view key) {
        if (key == "first_name") {
            return "firstName";
        }
        else if (key == "last_name") {
            return "lastName";
        }
        return key; // Return unchanged for other keys
    }
};
```

**Usage:**

```cpp
renamed_t obj{};
std::string buffer{};

// Writing JSON
glz::write_json(obj, buffer);
// Output: {"firstName":"","lastName":"","age":0}

// Reading JSON
buffer = R"({"firstName":"Kira","lastName":"Song","age":29})";
glz::read_json(obj, buffer);
// obj.first_name == "Kira"
// obj.last_name == "Song" 
// obj.age == 29
```

## Example 2: Dynamic Key Transformation

Apply systematic transformations to all keys using compile-time string manipulation:

```cpp
struct suffixed_keys_t {
    std::string first{};
    std::string last{};
};

template <>
struct glz::meta<suffixed_keys_t> {
    static constexpr std::string rename_key(const auto key) { 
        return std::string(key) + "_name"; 
    }
};
```

**Usage:**

```cpp
suffixed_keys_t obj{};
std::string buffer{};

// Writing JSON
glz::write_json(obj, buffer);
// Output: {"first_name":"","last_name":""}

// Reading JSON
buffer = R"({"first_name":"Kira","last_name":"Song"})";
glz::read_json(obj, buffer);
// obj.first == "Kira"
// obj.last == "Song"
```

## Compile-Time

The `rename_key` function, even when using `std::string` operates entirely at compile time. No string allocations or transformations at runtime.

## Implementation Details

### Return Types

The `rename_key` function can return:

- `const char*` or `std::string_view` for simple string literal transformations
- `std::string` when dynamic string construction is needed (as shown in the suffix example)
