# Lazy JSON Parsing

Glaze provides a truly lazy JSON parser (`glz::lazy_json`) that offers **on-demand** parsing without any upfront processing. This approach is ideal when you need to extract a few separate fields from large JSON documents.

## When to Use Lazy JSON

| Use Case | Recommended Approach |
|----------|---------------------|
| Extract 1-3 fields from large JSON | `glz::lazy_json` |
| Access fields near the beginning | `glz::lazy_json` or partial_read |
| Full deserialization into structs | `glz::read_json` |
| Iterate all elements (single pass) | `glz::lazy_json` |
| Multiple random accesses to array | `glz::lazy_json` with `.index()` |
| Unknown/dynamic JSON structure with persistent memory | `glz::generic` |

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

If you need validated UTF-8 strings and unescaping, use `get<std::string>()`.  Otherwise, `get<std::string_view>()` is faster.

> glz::lazy_json will ensure that any instantiated C++ values are valid JSON (except for std::string_view), but it doesn't validate the entire document, because this is often not a requirement for lazy parsing. If you want high performance full validation it is best to use C++ structs. Or, use glz::validate_json for pure validation passes.

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

## Indexed Views for O(1) Access

For scenarios requiring multiple random accesses or repeated iteration, you can build an index for O(1) element access:

```cpp
std::string json = R"({"users": [{"id": 0}, {"id": 1}, ..., {"id": 999}]})";
auto result = glz::lazy_json(json);

if (result) {
    // Build index once - O(n) scan
    auto users = (*result)["users"].index();

    // Now enjoy O(1) operations:
    size_t count = users.size();        // O(1) - no scanning
    auto user500 = users[500];          // O(1) - direct access
    auto user999 = users[999];          // O(1) - no matter the position

    // O(1) iteration advancement
    for (auto& user : users) {
        auto id = user["id"].get<int64_t>();  // Nested access still lazy
    }
}
```

### When to Use `.index()`

| Scenario | Without Index | With Index | Recommendation |
|----------|---------------|------------|----------------|
| Single random access | O(k) | O(n) build + O(1) | Don't index |
| 5+ random accesses | O(5k) | O(n) build + O(5) | **Use index** |
| Multiple iterations | O(n) each | O(n) build + O(n) each | **Use index** |
| Need size before iterating | O(n) | O(1) after build | **Use index** |
| Single sequential iteration | O(n) | O(n) build + O(n) | Don't index |

### Indexed View API

```cpp
auto indexed = doc["items"].index();

// O(1) size query
size_t count = indexed.size();

// O(1) empty check
if (!indexed.empty()) { /* ... */ }

// O(1) random access by position
auto third = indexed[2];

// For indexed objects: O(n) key lookup (linear search)
auto value = indexed["key"];

// Check if object contains key
if (indexed.contains("key")) { /* ... */ }

// Full random-access iterator support
auto it = indexed.begin();
it += 50;                    // Jump forward 50 elements
auto elem = it[10];          // Access 10 elements ahead
auto dist = indexed.end() - it;  // Distance to end
```

### Nested Access Remains Lazy

Elements returned from an indexed view are still `lazy_json_view` objects. Nested field access remains lazy:

```cpp
auto users = doc["users"].index();

// O(1) to get to user 500
auto user = users[500];

// Nested access is still lazy - scans only "email" field
auto email = user["profile"]["email"].get<std::string_view>();
```

### Memory Overhead

The index stores one pointer (8 bytes) per element, plus one `string_view` (16 bytes) per key for objects:

| Container | Elements | Index Memory |
|-----------|----------|--------------|
| Array | 1,000 | ~8 KB |
| Array | 10,000 | ~80 KB |
| Object | 1,000 | ~24 KB |
| Object | 10,000 | ~240 KB |

### Performance Example

For 10 random accesses to a 1000-element array:

| Approach | Throughput | Notes |
|----------|------------|-------|
| `lazy_json` (no index) | 232 MB/s | Each access scans from start |
| `lazy_json` (indexed) | 993 MB/s | Index built once, O(1) accesses |
| simdjson | 914 MB/s | Full structural index upfront |

The indexed approach is **327% faster** than non-indexed for this use case, and **9% faster than simdjson**.

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

## Deserializing into Structs

Use `read_into<T>()` to efficiently deserialize a lazy view directly into a typed struct:

```cpp
struct User {
   std::string name;
   int age;
   bool active;
};

std::string json = R"({
   "user": {"name": "Alice", "age": 30, "active": true},
   "metadata": {"version": 1, "large_data": "..."}
})";

auto result = glz::lazy_json(json);
if (result) {
    // Navigate lazily to "user" - skips "metadata" entirely
    auto user_view = (*result)["user"];

    // Deserialize directly into struct (single-pass, most efficient)
    User user{};
    auto ec = user_view.read_into(user);

    // user.name == "Alice", user.age == 30, user.active == true
}
```

### Why Use This Pattern?

This hybrid approach gives you the best of both worlds:

1. **Lazy navigation**: Skip large sections of JSON you don't need
2. **Fast deserialization**: Use Glaze's optimized struct parsing for the parts you do need
3. **Type safety**: Get compile-time checked structs instead of runtime field access

### Deserializing Array Elements

Combine with indexed views for efficient random access deserialization:

```cpp
struct Person {
   std::string name;
   Address address;
};

std::string json = R"({"people": [{"name": "Alice", ...}, {"name": "Bob", ...}, ...]})";

auto result = glz::lazy_json(json);
if (result) {
    // Build index for O(1) random access
    auto people = (*result)["people"].index();

    // Deserialize only the 500th person directly
    Person person{};
    people[500].read_into(person);
}
```

### Deserialization Methods

There are three ways to deserialize a lazy view:

```cpp
// Method 1: glz::read_json with lazy view - recommended, familiar API
User user1{};
glz::read_json(user1, doc["user"]);  // Single-pass, ~49% faster

// Method 2: read_into() member function - equivalent to Method 1
User user2{};
doc["user"].read_into(user2);  // Single-pass, ~49% faster

// Method 3: raw_json() + read_json() - double-pass, slower
User user3{};
glz::read_json(user3, doc["user"].raw_json());  // Scans value twice
```

**Performance**: Methods 1 and 2 are approximately **49% faster** than Method 3 because they avoid a double-scan of the data. The `raw_json()` method must first scan the value to find its end, then `read_json()` parses the same bytes again.

**Use `glz::read_json(value, view)`** for the familiar Glaze API. Use `raw_json()` only when you need the raw JSON string itself (e.g., for logging, forwarding, or storage).

### The `raw_json()` Method

Returns a `std::string_view` of the raw JSON bytes for any lazy view:

```cpp
auto result = glz::lazy_json(R"({"user": {"name": "Alice"}, "count": 5})");

// Get raw JSON for different value types
(*result).raw_json();                  // {"user": {"name": "Alice"}, "count": 5}
(*result)["user"].raw_json();          // {"name": "Alice"}
(*result)["user"]["name"].raw_json();  // "Alice"
(*result)["count"].raw_json();         // 5
```

> **Note**: For deserialization, prefer `read_into()` over `raw_json()` + `read_json()` for better performance.

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

4. **Use `.index()` for multiple random accesses**: If you need to access many elements by index or iterate multiple times:
   ```cpp
   auto items = doc["items"].index();  // Build index once
   auto first = items[0];              // O(1) access
   auto last = items[items.size()-1];  // O(1) access
   ```

5. **Keep JSON buffer alive**: The lazy parser stores pointers into the original buffer - it must remain valid for the lifetime of the document.

6. **Prefer `std::string_view` for strings**: When you don't need escape processing, `get<std::string_view>()` is faster than `get<std::string>()`.

7. **Access few fields for best speedup**: Lazy JSON shines when you access 1-5 fields from a large document. For full deserialization, use `glz::read_json`.

8. **Use `glz::read_json(value, view)` for struct deserialization**: When deserializing lazy views into structs, use `glz::read_json(obj, view)` or `view.read_into(obj)` instead of `glz::read_json(obj, view.raw_json())` - it's ~49% faster because it avoids a double scan.

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

| Feature | `glz::read_json` | `partial_read` | `glz::lazy_json` | `lazy_json` + `.index()` | `glz::generic` |
|---------|------------------|----------------|------------------|--------------------------|----------------|
| Parse time | O(n) | O(n) worst | O(1) | O(1) + O(n) on index | O(n) |
| Field access | O(1) | Hash-based | O(k)* | O(1) after index | O(1) |
| Random array access | O(1) | N/A | O(k)* | O(1) after index | O(1) |
| Memory usage | Struct size | Struct size | ~48 bytes | ~48 + 8n bytes | Dynamic |
| Type safety | Compile-time | Compile-time | Runtime | Runtime | Runtime |
| Short-circuit | No | Yes | Yes | Yes | No |
| Best for | Full deser. | Known subset | Few accesses | Many accesses | Unknown structure |

*k = bytes to skip to reach field

## See Also

- [Partial Read](./partial-read.md) - Compile-time partial reading with structs
- [Generic JSON](./generic-json.md) - Dynamic JSON with `glz::generic`
- [Reading](./reading.md) - Standard JSON reading with `glz::read_json`
- [JSON Pointer Syntax](./json-pointer-syntax.md) - Alternative path-based access
