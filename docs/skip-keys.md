# Skip Keys

The `skip` functionality in Glaze allows you to conditionally skip struct members during JSON serialization at compile time. This feature operates at compile time.

## Overview

By default, Glaze serializes all public C++ struct members to JSON. However, you may need to omit certain fields from your JSON output.

## Basic Usage

To use `skip`, specialize the `glz::meta` template for your struct and implement a `skip` function:

```cpp
struct my_struct_t {
    std::string public_member{};
    std::string internal_member{};
};

template <>
struct glz::meta<my_struct_t> {
    static constexpr bool skip(const std::string_view key) {
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
    static constexpr bool skip(const std::string_view key) {
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

// Reading JSON (skipped fields are ignored if present in input)
buffer = R"({"name":"Jane Doe","age":25,"secret_info":"Should be ignored"})";
glz::read_json(obj, buffer);
// obj.name == "Jane Doe"
// obj.age == 25
// obj.secret_info == "My Top Secret" (original value, not overwritten)
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
    static constexpr bool skip(const std::string_view key) { 
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
