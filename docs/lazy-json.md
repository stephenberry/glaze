# Lazy JSON Parsing

Glaze provides a truly lazy JSON parser (`glz::lazy_json`) that offers **on-demand** parsing without any upfront processing. This approach is ideal when you need to extract a few fields from large JSON documents or when parse time is critical.

## When to Use Lazy JSON

| Use Case | Recommended Approach |
|----------|---------------------|
| Extract 1-3 fields from large JSON | `glz::lazy_json` |
| Access fields near the beginning | `glz::lazy_json` |
| Full deserialization into structs | `glz::read_json` |
| Iterate all elements | `glz::read_json` or `glz::generic` |
| Unknown/dynamic JSON structure | `glz::generic` |

## Basic Usage

```cpp
#include "glaze/json.hpp"

std::string json = R"({"name":"John","age":30,"active":true,"balance":12345.67})";
auto result = glz::lazy_json(json);
if (result) {
    auto& doc = *result;

    // Access fields lazily - only parses what you access
    auto name = doc["name"].get<std::string_view>();
    auto age = doc["age"].get<int64_t>();
    auto active = doc["active"].get<bool>();
    auto balance = doc["balance"].get<double>();

    if (name && age && active && balance) {
        std::cout << *name << " is " << *age << " years old\n";
    }
}
```

## Why Lazy?

`glz::lazy_json` does **zero upfront work**:

- `lazy_json()` just stores a pointer and validates the first byte - O(1)
- Field access scans only the bytes needed to find that field

## UTF-8 Validation

To maximize performance, `lazy_json` does not validate UTF-8 encoding during initial parsing or field scanning. Validation only occurs when you extract string values:

- **`get<std::string>()`**: Processes escape sequences (`\n`, `\uXXXX`, etc.) and validates UTF-8 encoding
- **`get<std::string_view>()`**: Returns a raw view into the JSON buffer with no validation or processing

If you need validated UTF-8 strings, use `get<std::string>()`. If you're passing strings through unchanged or know the source is trusted, `get<std::string_view>()` is faster.

## Nested Object Access

Access deeply nested fields efficiently:

```cpp
std::string json = R"({
   "user": {
      "profile": {
         "name": "Alice",
         "email": "alice@example.com"
      },
      "settings": {
         "theme": "dark"
      }
   }
})";

auto result = glz::lazy_json(json);
if (result) {
    auto& doc = *result;

    // Chain field access - each level is lazy
    auto email = doc["user"]["profile"]["email"].get<std::string_view>();
    if (email) {
        std::cout << "Email: " << *email << "\n";
    }
}
```

## Array Access

Access array elements by index:

```cpp
std::string json = R"({
   "items": [
      {"id": 1, "value": 100},
      {"id": 2, "value": 200},
      {"id": 3, "value": 300}
   ]
})";

auto result = glz::lazy_json(json);
if (result) {
    auto& doc = *result;

    // Access specific array element
    auto first_value = doc["items"][0]["value"].get<int64_t>();
    auto third_id = doc["items"][2]["id"].get<int64_t>();

    if (first_value && third_id) {
        std::cout << "First value: " << *first_value << "\n";
        std::cout << "Third id: " << *third_id << "\n";
    }
}
```

## Iteration

Iterate over arrays and objects efficiently:

```cpp
std::string json = R"({"items": [{"id": 1}, {"id": 2}, {"id": 3}]})";
auto result = glz::lazy_json(json);

if (result) {
    auto& doc = *result;

    // Iterate array elements
    int64_t sum = 0;
    for (auto item : doc["items"]) {
        auto id = item["id"].get<int64_t>();
        if (id) sum += *id;
    }
    std::cout << "Sum of ids: " << sum << "\n";
}
```

For objects, you can access both keys and values:

```cpp
std::string json = R"({"a": 1, "b": 2, "c": 3})";
auto result = glz::lazy_json(json);

if (result) {
    for (auto item : result->root()) {
        std::cout << item.key() << ": ";
        auto val = item.get<int64_t>();
        if (val) std::cout << *val;
        std::cout << "\n";
    }
}
```

## Optimizing Performance: Sequential Access

The key to getting maximum performance from `lazy_json` is **accessing keys in document order**. The parser maintains a position pointer and continues scanning from where it left off.

### How Progressive Scanning Works

```cpp
std::string json = R"({"a":1,"b":2,"c":3,"d":4,"e":5})";
auto result = glz::lazy_json(json);

if (result) {
    auto& doc = *result;

    // FAST: Sequential access - O(n) total
    doc["a"].get<int64_t>();  // Scans from start, finds "a"
    doc["b"].get<int64_t>();  // Continues from after "a", finds "b"
    doc["c"].get<int64_t>();  // Continues from after "b", finds "c"
    doc["d"].get<int64_t>();  // Continues from after "c", finds "d"
    doc["e"].get<int64_t>();  // Continues from after "d", finds "e"
    // Total: scanned the object once
}
```

### Performance Comparison

| Access Pattern | Complexity | Example |
|----------------|------------|---------|
| Sequential (in document order) | O(n) total | `a`, `b`, `c`, `d`, `e` |
| Reverse order | O(n) per access | `e`, `d`, `c`, `b`, `a` |
| Random order | O(n) per access | `c`, `a`, `e`, `b`, `d` |

### Why Order Matters

Consider a JSON object with 1000 keys. Accessing 5 keys:

**Sequential access** (keys appear in order):
```
doc["key_001"]  → scan 1 key
doc["key_002"]  → scan 1 more key (continues from key_001)
doc["key_003"]  → scan 1 more key
doc["key_004"]  → scan 1 more key
doc["key_005"]  → scan 1 more key
Total: ~5 keys scanned
```

**Reverse order access**:
```
doc["key_005"]  → scan 5 keys from start
doc["key_004"]  → wrap around, scan 1004 keys
doc["key_003"]  → wrap around, scan 1003 keys
doc["key_002"]  → wrap around, scan 1002 keys
doc["key_001"]  → wrap around, scan 1001 keys
Total: ~5014 keys scanned (1000x slower!)
```

### Practical Guidelines

1. **Know your JSON structure**: If you know the key order, access them in that order:
   ```cpp
   // JSON: {"id":1,"name":"...","email":"...","created_at":"..."}
   // Access in document order:
   auto id = doc["id"].get<int64_t>();
   auto name = doc["name"].get<std::string_view>();
   auto email = doc["email"].get<std::string_view>();
   auto created = doc["created_at"].get<std::string_view>();
   ```

2. **Use iterators for unknown order**: If you need all keys but don't know the order:
   ```cpp
   for (auto item : doc.root()) {
       auto key = item.key();
       // Process each key-value pair in document order
   }
   ```

3. **Single field access is always fast**: Accessing just one field is O(k) where k is the position of that field - no penalty.

4. **Nested access is independent**: Each nested object has its own position tracking:
   ```cpp
   // Each level scans its own object independently
   doc["user"]["profile"]["email"]  // Fast - 3 separate scans
   ```

### Wrap-Around Behavior

If you access a key that appears earlier in the document, the parser wraps around:

```cpp
doc["c"].get<int64_t>();  // Position now after "c"
doc["a"].get<int64_t>();  // Wraps: scans from "c" to end, then start to "a"
```

This still works correctly but is slower than sequential access.

### Reset Parse Position

If you need to re-scan from the beginning:

```cpp
doc.reset_parse_pos();  // Next access starts from beginning
```

## Type Checking

Check the type of a value before extracting:

```cpp
auto& doc = *result;
auto value = doc["field"];

if (value.is_object()) { /* ... */ }
if (value.is_array()) { /* ... */ }
if (value.is_string()) { /* ... */ }
if (value.is_number()) { /* ... */ }
if (value.is_boolean()) { /* ... */ }
if (value.is_null()) { /* ... */ }

// Explicit bool conversion - true if not null/error
if (value) {
    // Value exists and is not null
}
```

## Supported Types for get<T>()

| Type | Description |
|------|-------------|
| `bool` | Boolean values |
| `int32_t`, `int64_t` | Signed integers |
| `uint32_t`, `uint64_t` | Unsigned integers |
| `float`, `double` | Floating-point numbers |
| `std::string` | String with escape processing |
| `std::string_view` | Raw string view (no escape processing) |
| `std::nullptr_t` | Null values |

## Error Handling

All operations return values that can be checked for errors:

```cpp
auto result = glz::lazy_json(json);
if (!result) {
    // Parse error
    auto error = result.error();
    std::cout << "Error: " << glz::format_error(error, json) << "\n";
    return;
}

auto& doc = *result;
auto value = doc["missing_key"];

if (value.has_error()) {
    // Key not found or type error
    auto ec = value.error();
    // Handle error...
}

auto num = doc["field"].get<int64_t>();
if (!num) {
    // Extraction failed (wrong type, parse error, etc.)
    auto error = num.error();
    // Handle error...
}
```

## Container Methods

```cpp
auto& doc = *result;
auto arr = doc["items"];

// Check if container is empty
if (arr.empty()) { /* ... */ }

// Get number of elements (requires scanning)
size_t count = arr.size();

// Check if object contains a key
if (doc.root().contains("name")) { /* ... */ }
```

## Writing Lazy Views

Lazy views can be written back to JSON:

```cpp
auto& doc = *result;
auto user = doc["user"];

std::string output;
auto ec = glz::write_json(user, output);
// output contains the JSON for just the "user" field
```

## Options

Use compile-time options for non-null-terminated buffers:

```cpp
// For null-terminated strings (default, fastest)
auto result = glz::lazy_json(json);

// For non-null-terminated buffers
constexpr auto opts = glz::opts{.null_terminated = false};
auto result = glz::lazy_json<opts>(buffer);
```

## Memory Layout

The lazy parser is designed for minimal memory overhead. A `lazy_json_view` is 48 bytes on 64-bit systems and 24 bytes on 32-bit systems.

## Best Practices

1. **Access keys in document order**: This is the most important optimization. Sequential access gives O(n) total complexity:
   ```cpp
   // If JSON is: {"a":1,"b":2,"c":3}
   doc["a"];  // Good: starts scanning
   doc["b"];  // Good: continues from "a"
   doc["c"];  // Good: continues from "b"
   // Total: one scan of the object
   ```

2. **Store the document reference**: To benefit from progressive scanning, use the same document object:
   ```cpp
   auto& doc = *result;  // Store reference
   doc["a"];  // Position tracked in doc
   doc["b"];  // Continues from where "a" left off
   ```

3. **Use iterators when order is unknown**: If you don't know the key order or need all keys:
   ```cpp
   for (auto item : doc.root()) {
       // Always efficient - iterates in document order
   }
   ```

4. **Keep JSON buffer alive**: The lazy parser stores pointers into the original buffer - it must remain valid for the lifetime of the document.

5. **Prefer `std::string_view` for strings**: When you don't need escape processing, `get<std::string_view>()` is faster than `get<std::string>()`.

6. **Access few fields for best speedup**: Lazy JSON shines when you access 1-5 fields from a large document. For full deserialization, use `glz::read_json`.

## Partial Read vs Lazy JSON

Glaze offers two approaches for reading a subset of JSON data. Choose based on whether you know the fields at compile time:

### Use `partial_read` When:

- **Fields are known at compile time**: You can define a struct with just the fields you need
- **Type safety matters**: You want compile-time type checking
- **Fields appear early in the document**: Partial read short-circuits after finding all struct fields
- **Hash-based lookup**: Uses Glaze's optimized key matching

```cpp
// Define a struct with only the fields you need
struct Header {
   std::string id{};
   std::string type{};
};

std::string json = R"({"id":"abc123","type":"request","payload":{...large data...}})";
Header h{};
auto ec = glz::read<glz::opts{.partial_read = true}>(h, json);
// Parsing stops after "id" and "type" are found - "payload" is never parsed
```

### Use `lazy_json` When:

- **Fields determined at runtime**: You don't know which fields to access until execution
- **Conditional access**: You need to check one field before deciding to read others
- **Path-based access**: You want to access nested fields by path (e.g., `doc["user"]["email"]`)
- **Iteration**: You need to iterate over array/object elements

```cpp
auto result = glz::lazy_json(json);
if (result) {
    auto& doc = *result;

    // Decide at runtime which fields to access
    auto type = doc["type"].get<std::string_view>();
    if (type && *type == "user_event") {
        auto user_id = doc["user"]["id"].get<int64_t>();  // Only accessed conditionally
    }
}
```

### Performance Comparison

| Scenario | `partial_read` | `lazy_json` | Winner |
|----------|---------------|-------------|--------|
| Known fields, near start | Very fast | Fast | `partial_read` |
| Known fields, scattered | Moderate | Fast (sequential) | Depends on order |
| Conditional field access | N/A | Fast | `lazy_json` |
| Dynamic field names | N/A | Supported | `lazy_json` |
| Type-safe structs | Yes | No | `partial_read` |

See [Partial Read](./partial-read.md) for detailed documentation.

## Comparison with All Approaches

| Feature | `glz::read_json` | `partial_read` | `glz::lazy_json` | `glz::generic` |
|---------|------------------|----------------|------------------|----------------|
| Parse time | O(n) | O(n) worst | O(1) | O(n) |
| Field access | O(1) | Hash-based | O(k)* | O(1) |
| Memory usage | Struct size | Struct size | ~48 bytes | Dynamic |
| Type safety | Compile-time | Compile-time | Runtime | Runtime |
| Short-circuit | No | Yes | Yes | No |
| Best for | Full deserialization | Known subset | Dynamic access | Unknown structure |

*k = bytes to skip to reach field

## See Also

- [Partial Read](./partial-read.md) - Compile-time partial reading with structs
- [Generic JSON](./generic-json.md) - Dynamic JSON with `glz::generic`
- [Reading](./reading.md) - Standard JSON reading with `glz::read_json`
- [JSON Pointer Syntax](./json-pointer-syntax.md) - Alternative path-based access
