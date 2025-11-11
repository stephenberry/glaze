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

## Value-Based Skipping with `skip_if`

While `skip` allows skipping fields based on their key names at compile time, `skip_if` enables runtime skipping based on the actual field values. This is useful for omitting fields with default values or applying conditional logic based on the data.

### Overview

The `skip_if` function is a template method that receives:
- The field value (with type information preserved)
- The key name
- The `meta_context` (operation type)

This allows you to inspect the actual value and decide whether to skip serialization.

### Basic Usage

```cpp
struct my_struct_t {
    std::string name = "default";
    int age = 0;
    std::string city{};
};

template <>
struct glz::meta<my_struct_t> {
    template <class T>
    static constexpr bool skip_if(T&& value, std::string_view key, const glz::meta_context&) {
        using V = std::decay_t<T>;
        if constexpr (std::same_as<V, std::string>) {
            return key == "name" && value == "default";
        }
        else if constexpr (std::same_as<V, int>) {
            return key == "age" && value == 0;
        }
        return false;
    }
};
```

### Example: Skipping Default Values

A common use case is omitting fields that contain their default values to reduce JSON size:

```cpp
struct user_settings_t {
    std::string theme = "light";
    int volume = 50;
    bool notifications = true;
    std::string custom_path{};
};

template <>
struct glz::meta<user_settings_t> {
    template <class T>
    static constexpr bool skip_if(T&& value, std::string_view key, const glz::meta_context&) {
        using V = std::decay_t<T>;
        if constexpr (std::same_as<V, std::string>) {
            if (key == "theme") return value == "light";
            if (key == "custom_path") return value.empty();
        }
        else if constexpr (std::same_as<V, int>) {
            if (key == "volume") return value == 50;
        }
        else if constexpr (std::same_as<V, bool>) {
            if (key == "notifications") return value == true;
        }
        return false;
    }
};
```

**Usage:**

```cpp
user_settings_t settings1{"light", 50, true, ""};
std::string buffer1{};
glz::write_json(settings1, buffer1);
// Output: {}  (all fields have default values)

user_settings_t settings2{"dark", 75, false, "/custom/path"};
std::string buffer2{};
glz::write_json(settings2, buffer2);
// Output: {"theme":"dark","volume":75,"notifications":false,"custom_path":"/custom/path"}

user_settings_t settings3{"light", 80, true, ""};
std::string buffer3{};
glz::write_json(settings3, buffer3);
// Output: {"volume":80}  (only non-default field included)
```

### Important Notes

- `skip_if` is evaluated at **runtime** during serialization, unlike `skip` which is **compile-time**
- The template method allows type-safe inspection of field values using `if constexpr`
- `skip_if` is primarily for **serialization** (writing); during parsing, all present fields are read
- You **can** use both `skip` and `skip_if` together: `skip` is checked first (compile-time), and if a field isn't skipped, `skip_if` is then evaluated (runtime)
- Use `std::decay_t` to get the underlying type without references and cv-qualifiers

### Combining `skip` and `skip_if`

You can use both methods together for maximum flexibility. `skip` is evaluated first (at compile-time), and if it returns false, `skip_if` is then evaluated (at runtime):

```cpp
struct api_response_t {
    std::string id{};
    std::string secret_key{};  // Never serialize
    std::string name{};
    int count = 0;
};

template <>
struct glz::meta<api_response_t> {
    // Compile-time skip: always exclude secret_key
    static constexpr bool skip(const std::string_view key, const glz::meta_context&) {
        return key == "secret_key";
    }

    // Runtime skip: exclude count when it's 0
    template <class T>
    static constexpr bool skip_if(T&& value, std::string_view key, const glz::meta_context&) {
        using V = std::decay_t<T>;
        if constexpr (std::same_as<V, int>) {
            return key == "count" && value == 0;
        }
        return false;
    }
};
```

**Usage:**

```cpp
api_response_t resp1{"123", "secret", "Widget", 0};
std::string buffer1{};
glz::write_json(resp1, buffer1);
// Output: {"id":"123","name":"Widget"}
// secret_key skipped by skip(), count skipped by skip_if()

api_response_t resp2{"456", "secret", "Gadget", 5};
std::string buffer2{};
glz::write_json(resp2, buffer2);
// Output: {"id":"456","name":"Gadget","count":5}
// secret_key skipped by skip(), count included (non-zero)
```

### Comparison: `skip` vs `skip_if`

| Feature | `skip` (Key-Based) | `skip_if` (Value-Based) |
|---------|-------------------|------------------------|
| Decision time | Compile-time | Runtime |
| Input | Key name only | Key name + field value |
| Use case | Always skip certain fields | Skip based on field values |
| Performance | Zero runtime overhead | Minimal runtime check per field |
| Template | Not templated | Template method |
| Combinable | Can be used with `skip_if` | Can be used with `skip` |

Choose `skip` when you want to permanently exclude fields, and `skip_if` when the decision depends on the data. Use both together for maximum control.
