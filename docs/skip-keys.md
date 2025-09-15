# Skip Keys

The `skip` functionality in Glaze allows you to conditionally skip struct members during JSON serialization and parsing at compile time. This feature operates at compile time and can differentiate between serialize and parse operations.

## Overview

By default, Glaze serializes all public C++ struct members to JSON. However, you may need to omit certain fields from your JSON output or input processing.

## Meta Context

The `skip` function receives a `meta_context` parameter that provides compile-time information about the current operation:

```cpp
namespace glz {
    enum struct operation {
        serialize,  // Writing/serializing to JSON
        parse      // Reading/parsing from JSON
    };

    struct meta_context {
        operation op = operation::serialize;
    };
}
```

This allows you to implement different skipping logic for serialization versus parsing operations.

## Basic Usage

To use `skip`, specialize the `glz::meta` template for your struct and implement a `skip` function:

```cpp
struct my_struct_t {
    std::string public_member{};
    std::string internal_member{};
};

template <>
struct glz::meta<my_struct_t> {
    static constexpr bool skip(const std::string_view key, const meta_context&) {
        // Return true to skip the key, false to include it
        return key == "internal_member";
    }
};
```

## Example 1: Skipping Specific Keys

Omit specific member names from the JSON output:

```cpp
struct skipped_t {
    std::string name{};
    int age{};
    std::string secret_info{};
};

template <>
struct glz::meta<skipped_t> {
    static constexpr bool skip(const std::string_view key, const meta_context&) {
        if (key == "secret_info") {
            return true; // Skip this field
        }
        return false; // Include other fields
    }
};
```

**Usage:**

```cpp
skipped_t obj{"John Doe", 30, "My Top Secret"};
std::string buffer{};

// Writing JSON
glz::write_json(obj, buffer);
// Output: {"name":"John Doe","age":30}
```

## Example 2: Skipping Based on Prefix/Suffix

Apply systematic skipping based on key patterns using compile-time string manipulation:

```cpp
struct prefixed_skipped_t {
    std::string user_id{};
    std::string user_name{};
    std::string temp_data{};
};

template <>
struct glz::meta<prefixed_skipped_t> {
    static constexpr bool skip(const std::string_view key, const meta_context&) { 
        return key.starts_with("temp_"); // Skip keys starting with "temp_"
    }
};
```

**Usage:**

```cpp
prefixed_skipped_t obj{"123", "Alice", "temporary value"};
std::string buffer{};

// Writing JSON
glz::write_json(obj, buffer);
// Output: {"user_id":"123","user_name":"Alice"}
```

## Example 3: Operation-Specific Skipping

Use the `meta_context` to implement different skipping behavior for serialization versus parsing:

```cpp
struct versioned_data_t {
    std::string name{};
    int version{};
    std::string computed_field{};
    std::string input_only_field{};
};

template <>
struct glz::meta<versioned_data_t> {
    static constexpr bool skip(const std::string_view key, const meta_context& ctx) {
        // Skip computed_field during parsing (reading) - it should only be written
        if (key == "computed_field" && ctx.op == glz::operation::parse) {
            return true;
        }
        
        // Skip input_only_field during serialization (writing) - it should only be read
        if (key == "input_only_field" && ctx.op == glz::operation::serialize) {
            return true;
        }
        
        return false;
    }
};
```

**Usage:**

```cpp
versioned_data_t obj{"TestData", 1, "computed_value", "input_value"};
std::string buffer{};

// Writing JSON - skips input_only_field
glz::write_json(obj, buffer);
// Output: {"name":"TestData","version":1,"computed_field":"computed_value"}

// Reading JSON - skips computed_field
const char* json = R"({"name":"NewData","version":2,"computed_field":"ignored","input_only_field":"new_input"})";
glz::read_json(obj, json);
// obj.computed_field retains "computed_value", obj.input_only_field becomes "new_input"
```
