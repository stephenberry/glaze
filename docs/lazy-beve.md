# Lazy BEVE Parsing

Glaze provides a truly lazy BEVE parser (`glz::lazy_beve`) that offers **on-demand** parsing without any upfront processing. This approach is ideal when you need to extract a few separate fields from large BEVE documents.

## When to Use Lazy BEVE

| Use Case | Recommended Approach |
|----------|---------------------|
| Extract 1-3 fields from large BEVE | `glz::lazy_beve` |
| Access fields near the beginning | `glz::lazy_beve` |
| Full deserialization into structs | `glz::read_beve` |
| Iterate all elements (single pass) | `glz::lazy_beve` |
| Multiple random accesses to array | `glz::lazy_beve` with `.index()` |
| Unknown/dynamic BEVE structure with persistent memory | `glz::generic` |

## Basic Usage

```cpp
#include "glaze/beve.hpp"

struct User {
   std::string name{};
   int age{};
   bool active{};
};

User original{"John", 30, true};
std::vector<std::byte> buffer;
glz::write_beve(original, buffer);

auto result = glz::lazy_beve(buffer);
if (result) {
    auto& doc = *result;

    // Access fields lazily - only parses what you access
    auto name = doc["name"].get<std::string_view>();
    auto age = doc["age"].get<int64_t>();
    auto active = doc["active"].get<bool>();

    if (name && age && active) {
        std::cout << *name << " is " << *age << " years old\n";
    }
}
```

## Why Lazy?

`glz::lazy_beve` does **zero upfront work**:

- `lazy_beve()` just stores a pointer and validates the first byte - O(1)
- Field access scans only the bytes needed to find that field

## Advantages of BEVE for Lazy Parsing

BEVE's binary format provides several advantages for lazy parsing compared to JSON:

| Feature | BEVE | JSON |
|---------|------|------|
| Type detection | O(1) - single byte check | O(k) - scan to identify type |
| String length | O(1) - length-prefixed | O(n) - scan for closing quote |
| Container size | O(1) - count-prefixed | O(n) - scan all elements |
| Skip value | O(1) for fixed types | O(n) - must parse structure |

## Nested Object Access

Access deeply nested fields efficiently:

```cpp
struct Address {
   std::string city{};
   std::string country{};
};

struct Profile {
   std::string name{};
   std::string email{};
};

struct UserProfile {
   Profile profile{};
   Address address{};
};

struct Container {
   UserProfile user{};
};

Container original{
   {{"Alice", "alice@example.com"}, {"New York", "USA"}}
};

std::vector<std::byte> buffer;
glz::write_beve(original, buffer);

auto result = glz::lazy_beve(buffer);
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
struct Item {
   int id{};
   int value{};
};

struct Container {
   std::vector<Item> items;
};

Container original{{{1, 100}, {2, 200}, {3, 300}}};
std::vector<std::byte> buffer;
glz::write_beve(original, buffer);

auto result = glz::lazy_beve(buffer);
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
std::map<std::string, int> data{{"a", 1}, {"b", 2}, {"c", 3}};
std::vector<std::byte> buffer;
glz::write_beve(data, buffer);

auto result = glz::lazy_beve(buffer);
if (result) {
    // Iterate object entries
    int64_t sum = 0;
    for (auto& item : result->root()) {
        auto val = item.get<int64_t>();
        if (val) sum += *val;
    }
    std::cout << "Sum: " << sum << "\n";  // Sum: 6
}
```

For objects, you can access both keys and values:

```cpp
for (auto& item : result->root()) {
    std::cout << item.key() << ": ";
    auto val = item.get<int64_t>();
    if (val) std::cout << *val;
    std::cout << "\n";
}
```

## Indexed Views for O(1) Access

For scenarios requiring multiple random accesses or repeated iteration, you can build an index for O(1) element access:

```cpp
std::map<std::string, int> data;
for (int i = 0; i < 1000; ++i) {
    data["key" + std::to_string(i)] = i;
}
std::vector<std::byte> buffer;
glz::write_beve(data, buffer);

auto result = glz::lazy_beve(buffer);
if (result) {
    // Build index once - O(n) scan
    auto indexed = result->root().index();

    // Now enjoy O(1) operations:
    size_t count = indexed.size();      // O(1) - no scanning
    auto key500 = indexed["key500"];    // Linear search in indexed keys
    auto first = indexed[0];            // O(1) - direct access by position

    // O(1) iteration advancement
    for (auto& item : indexed) {
        auto val = item.get<int64_t>();  // Value access still lazy
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

## Optimizing Performance: Sequential Access

The key to getting maximum performance from `lazy_beve` is **accessing keys in document order**. The parser maintains a position pointer and continues scanning from where it left off.

### How Progressive Scanning Works

```cpp
std::map<std::string, int> data{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}, {"e", 5}};
std::vector<std::byte> buffer;
glz::write_beve(data, buffer);

auto result = glz::lazy_beve(buffer);
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

// BEVE-specific: check array type
if (value.is_typed_array()) { /* homogeneous elements */ }
if (value.is_generic_array()) { /* heterogeneous elements */ }

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
| `std::string` | String value |
| `std::string_view` | String view (zero-copy) |
| `std::nullptr_t` | Null values |

## Error Handling

All operations return values that can be checked for errors:

```cpp
auto result = glz::lazy_beve(buffer);
if (!result) {
    // Parse error
    auto error = result.error();
    std::cout << "Error code: " << static_cast<int>(error.ec) << "\n";
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

// Get number of elements (O(1) in BEVE - count is stored)
size_t count = arr.size();

// Check if object contains a key
if (doc.root().contains("name")) { /* ... */ }
```

## Size Accessor

Get string lengths or element counts without extracting values - useful for validation:

```cpp
auto result = glz::lazy_beve(buffer);
if (result) {
    // Validate string length before extracting
    if (result->size["username"] > 50) {
        // Username too long - reject without parsing
        return error;
    }

    // Check array size
    if (result->size["items"] == 0) {
        // Empty items array
    }

    // Also works with index access
    size_t first_name_len = result->size[0];  // For arrays
}
```

The `size()` method returns:
- **Strings**: character length
- **Arrays**: element count
- **Objects**: number of keys
- **Primitives**: 0

You can also call `size()` directly on any view:

```cpp
auto username = (*result)["username"];
if (username.size() > 50) {
    // Too long
}
```

## Deserializing into Structs

Use `glz::read_beve()` to deserialize a lazy view directly into a typed struct:

```cpp
struct User {
   std::string name;
   int age;
   bool active;
};

struct Container {
   User user;
   int version;
};

Container original{{"Alice", 30, true}, 1};
std::vector<std::byte> buffer;
glz::write_beve(original, buffer);

auto result = glz::lazy_beve(buffer);
if (result) {
    // Navigate lazily to "user", then deserialize into struct
    User user{};
    auto ec = glz::read_beve(user, (*result)["user"]);

    // user.name == "Alice", user.age == 30, user.active == true
}
```

This works because Glaze provides a `read_beve` overload that accepts `lazy_beve_view` directly. The lazy navigation skips other fields entirely, and deserialization is single-pass.

### Why Use This Pattern?

This hybrid approach gives you the best of both worlds:

1. **Lazy navigation**: Skip large sections of BEVE you don't need
2. **Fast deserialization**: Use Glaze's optimized struct parsing for the parts you do need
3. **Type safety**: Get compile-time checked structs instead of runtime field access

### Deserializing Array Elements

Combine with indexed views for efficient random access deserialization:

```cpp
struct Person {
   std::string name;
   Address address;
};

struct Container {
   std::vector<Person> people;
};

Container original{{{"Alice", {...}}, {"Bob", {...}}, ...}};
std::vector<std::byte> buffer;
glz::write_beve(original, buffer);

auto result = glz::lazy_beve(buffer);
if (result) {
    // Build index for O(1) random access
    auto people = (*result)["people"].index();

    // Deserialize only the 500th person
    Person person{};
    glz::read_beve(person, people[500]);
}
```

### Alternative: `read_into()` Member Function

If you prefer member function syntax, use `read_into()`:

```cpp
User user{};
(*result)["user"].read_into(user);  // Equivalent to glz::read_beve(user, view)
```

### The `raw_beve()` Method

Returns a `std::string_view` of the raw BEVE bytes for any lazy view. Use this when you need the binary data itself (for forwarding, storage, or separate parsing):

```cpp
auto result = glz::lazy_beve(buffer);

// Get raw BEVE for different values
auto root_bytes = result->root().raw_beve();
auto user_bytes = (*result)["user"].raw_beve();
```

> **Note**: For deserialization, use `glz::read_beve(value, view)` instead of `glz::read_beve(value, view.raw_beve())` for better performance.

## Best Practices

1. **Access keys in document order**: This is the most important optimization. Sequential access gives O(n) total complexity.

2. **Store the document reference**: To benefit from progressive scanning, use the same document object:
   ```cpp
   auto& doc = *result;  // Store reference
   doc["a"];  // Position tracked in doc
   doc["b"];  // Continues from where "a" left off
   ```

3. **Use iterators when order is unknown**: If you don't know the key order or need all keys:
   ```cpp
   for (auto& item : doc.root()) {
       // Always efficient - iterates in document order
   }
   ```

4. **Use `.index()` for multiple random accesses**: If you need to access many elements by index or iterate multiple times:
   ```cpp
   auto items = doc["items"].index();  // Build index once
   auto first = items[0];              // O(1) access
   auto last = items[items.size()-1];  // O(1) access
   ```

5. **Keep BEVE buffer alive**: The lazy parser stores pointers into the original buffer - it must remain valid for the lifetime of the document.

6. **Use `std::string_view` for strings**: When you don't need a copy, `get<std::string_view>()` provides zero-copy access.

7. **Access few fields for best speedup**: Lazy BEVE shines when you access 1-5 fields from a large document. For full deserialization, use `glz::read_beve`.

8. **Use `glz::read_beve(value, view)` for struct deserialization**: Glaze provides an overload of `read_beve` that accepts `lazy_beve_view` directly.

## Comparison with All Approaches

| Feature | `glz::read_beve` | `glz::lazy_beve` | `lazy_beve` + `.index()` |
|---------|------------------|------------------|--------------------------|
| Parse time | O(n) | O(1) | O(1) + O(n) on index |
| Field access | O(1) | O(k)* | O(1) after index |
| Random array access | O(1) | O(k)* | O(1) after index |
| Memory usage | Struct size | ~48 bytes | ~48 + 8n bytes |
| Type safety | Compile-time | Runtime | Runtime |
| Best for | Full deser. | Few accesses | Many accesses |

*k = bytes to skip to reach field

## See Also

- [BEVE (Binary)](./binary.md) - BEVE format documentation
- [Lazy JSON](./lazy-json.md) - Lazy parsing for JSON format
- [Reading](./reading.md) - Standard reading with `glz::read`
