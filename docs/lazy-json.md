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

To maximize performance, `lazy_json` does no validation during initial parsing or field scanning. Validation happens when you extract a string value:

- **`get<std::string>()`**: Processes escape sequences (`\n`, `\uXXXX`, etc.) and validates UTF-8 encoding, failing with `error_code::invalid_utf8` on malformed input
- **`get<std::string_view>()`**: Returns a raw view into the JSON buffer with no validation or processing

If you need validated UTF-8 strings and unescaping, use `get<std::string>()`. Otherwise `get<std::string_view>()` is faster, but the bytes it returns are unchecked. To validate a whole document up front, run `glz::validate_json` over the buffer. See [Reading](./json.md#utf-8-validation).

`get<std::string>()` honors the document's `validate_utf8` option, so a `lazy_json` opened with that option set to `false` returns the raw bytes rather than an error.

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
    std::size_t count = users.size();        // O(1) - no scanning
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
std::size_t count = indexed.size();

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

### Performance Example

For 10 random accesses to a 1000-element array:

| Approach | Throughput | Notes |
|----------|------------|-------|
| `lazy_json` (no index) | 232 MB/s | Each access scans from start |
| `lazy_json` (indexed) | 993 MB/s | Index built once, O(1) accesses |

The indexed approach is **327% faster** than non-indexed for this use case.

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

## Streaming Cursor (opt-in)

When you iterate a large container and fully consume each element, the iterator normally has to re-scan the element it just handed you in order to find where the next one begins. `read_into` already walked those same bytes, so the scan is pure repeated work.

The `lazy_streaming_cursor` option removes it. When a value is consumed end to end, its byte extent is recorded on the document, and the next advance jumps straight to the recorded end:

```cpp
struct stream_opts : glz::opts
{
   bool lazy_streaming_cursor = true;
};

inline constexpr stream_opts stream{};

auto doc = glz::lazy_json<stream>(buffer);

for (auto& row : (*doc)["rows"]) {
   Row parsed{};
   if (not row.read_into(parsed)) {
      use(parsed);  // the next ++ skips the re-scan of this row
   }
}
```

The optimization composes through nesting: an inner iterator that runs to its closing bracket records the whole inner container, so the outer advance skips it too.

An extent is only used when it starts exactly at the element the iterator is positioned on. A byte offset identifies exactly one value in the buffer, so this check is what keeps the single shared slot safe when iterators are interleaved or abandoned partway. Anything that does not match falls back to a normal scan.

### What it costs

Two `size_t` on `lazy_document` when enabled, and nothing when disabled, so a document that does not opt in is byte-for-byte what it was before. Views and iterators are unchanged in either case.

### Thread safety

Recording an extent writes to the document, from a `const` method. Two threads calling `read_into` on **disjoint** subviews of the same `lazy_document` do not race with the option off, but do with it on. Give each thread its own `lazy_document` (copying one is cheap — it holds a pointer and a length, not the buffer) or leave the option off.

Note that a `lazy_document` is already not safe to share for keyed access either way, because `operator[]` advances the cached root view's scan position.

### When it does not help

- **Scalar elements.** Only containers are recorded (see below); numbers, strings, booleans and nulls are skipped by the cheap paths anyway.
- **Elements consumed key by key** via `operator[]`. Those are already served by the progressive `parse_pos_` scan described above; no extent is produced.
- **Elements that are skipped rather than read.** Nothing was consumed, so there is nothing to record.
- **`partial_read`.** That option deliberately stops the parser as soon as the target's known keys are filled, which leaves it short of the element's true end.
- **The final element of a bounded (non-null-terminated) buffer**, which ends on `end_reached` rather than on a close bracket.

### What the cursor trusts, and what it verifies

`read_into` hands the iterator to `parse<JSON>::op`, and where that leaves it is up to the `from<JSON, T>` specialization. Glaze's own readers stop at the value's end, but that is a convention, not something the cursor can check for free — `glz::text` deliberately swallows the rest of the buffer, and a custom reader may consume only a prefix. Trusting the stopping point blindly is how a cursor turns a well-formed document into a short or mis-valued element stream with no error reported anywhere.

So an extent is recorded only when **the element is a container and the parse finished exactly on that container's own closing bracket**. That covers the case worth accelerating (large elements, where re-scanning is expensive) and rejects readers that stopped somewhere else.

One residual limitation: a custom reader that consumes a *complete inner container* but stops before the element's own end (for example reading `[[1],2]` and stopping after `[1]`) produces an extent that passes this check and is still wrong. Such a reader is already incompatible with ordinary `glz::read_json` on nested structures, since it would misparse whatever follows. If you write custom `from<JSON, T>` specializations, leave the iterator at the value's end.

### Measured effect

A 9 MB array of three-field objects, `read_into` per row, Apple M1, clang `-O3`:

| | throughput |
|---|---:|
| cursor off | 575 MB/s |
| cursor on | 955 MB/s (**+66%**) |

The gain scales with how much of each element `read_into` consumes; containers whose elements are large benefit most, since those are the scans being elided.

## Wide Number Skip (opt-in)

Skipping over a container walks its bytes looking for the structural characters (`"` `[` `]` `{` `}`) that carry depth. Everything in between — numbers, literals, commas, whitespace — is stepped over one byte at a time with a table lookup.

That is the right default. Typical JSON numbers are a few bytes long, and a wide SIMD-style scan cannot amortize its setup over three digits. But documents built from **long numeric cells** — telemetry dumps, exported matrices, scientific data — spend most of their bytes inside numeric runs, and there the byte-at-a-time walk dominates.

`lazy_wide_number_skip` targets exactly that case. It keeps the cheap table walk for short numbers and escalates to an 8-byte SWAR scan only once a run is still going after 8 bytes:

```cpp
struct wide_opts : glz::opts
{
   bool null_terminated = false;   // required: the option applies to bounded buffers
   bool lazy_wide_number_skip = true;
};
```

### When to enable it

Measure on your own data. This is a real trade, not a free win, and how it lands depends on more than just how long your numbers are. Full traversal, Apple M1, clang `-O3`, bounded buffers, interleaved best-of-5:

| document shape | option off | option on | |
|---|---:|---:|---|
| long numeric runs, array cells | 904 MB/s | 1231 MB/s | **+36%** |
| long numeric, object-keyed | 521 MB/s | 479 MB/s | **−8%** |
| short numeric cells | 380 MB/s | 380 MB/s | ±0% |
| tiny objects (`{"k":"v","w":1}`) | 314 MB/s | 310 MB/s | −1% |
| long strings | 1877 MB/s | 1884 MB/s | ±0% |
| telemetry records | 485 MB/s | 478 MB/s | −1% |
| nested mixed (typical API payload) | 356 MB/s | 340 MB/s | −4% |

The two rows that matter most are the numeric ones, and they point in opposite directions:

- **Long numbers as array cells win big.** The escalated scan runs past the commas and the following cells in one sweep, so it skips much more than a single number.
- **Long numbers as object values can lose.** Each run ends a few bytes later at the next key's `"`, so the scan never amortizes its setup. An independent measurement using a different traversal put this case as bad as −19%, so treat roughly −20% as the worst case rather than the −8% above.

Enable it for array-shaped numeric data. Leave it off otherwise. With the option off the generated code is byte-for-byte what it was before the option existed, so the default costs nothing at all.

### Restrictions

- **Non-null-terminated buffers only.** A null-terminated buffer stops on its own sentinel, and a detached view may have no end pointer at all, so the option is ignored there.
- **Correctness is independent of the option.** Inside a container every non-structural byte is ignorable, so escalating early, late, or never reaches the same position. The threshold is a performance knob, not a semantic one.

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
| `std::int32_t`, `int64_t` | Signed integers |
| `std::uint32_t`, `std::uint64_t` | Unsigned integers |
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
std::size_t count = arr.size();

// Check if object contains a key
if (doc.root().contains("name")) { /* ... */ }
```

## Deserializing into Structs

Use `glz::read_json()` to deserialize a lazy view directly into a typed struct:

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
    // Navigate lazily to "user", then deserialize into struct
    User user{};
    auto ec = glz::read_json(user, (*result)["user"]);

    // user.name == "Alice", user.age == 30, user.active == true
}
```

This works because Glaze provides a `read_json` overload that accepts `lazy_json_view` directly. The lazy navigation skips "metadata" entirely, and deserialization is single-pass (no double scanning).

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

    // Deserialize only the 500th person
    Person person{};
    glz::read_json(person, people[500]);
}
```

### Alternative: `read_into()` Member Function

If you prefer member function syntax, use `read_into()`:

```cpp
User user{};
(*result)["user"].read_into(user);  // Equivalent to glz::read_json(user, view)
```

### Performance Note

Both `glz::read_json(value, view)` and `view.read_into(value)` are **~49% faster** than the older pattern of `glz::read_json(value, view.raw_json())`. The `raw_json()` approach requires scanning the value twice: once to find its extent, and once to parse it.

### The `raw_json()` Method

Returns a `std::string_view` of the raw JSON bytes for any lazy view. Use this when you need the JSON text itself (for logging, forwarding, or storage):

```cpp
auto result = glz::lazy_json(R"({"user": {"name": "Alice"}, "count": 5})");

// Get raw JSON for different value types
(*result).raw_json();                  // {"user": {"name": "Alice"}, "count": 5}
(*result)["user"].raw_json();          // {"name": "Alice"}
(*result)["user"]["name"].raw_json();  // "Alice"
(*result)["count"].raw_json();         // 5
```

> **Note**: For deserialization, use `glz::read_json(value, view)` instead of `glz::read_json(value, view.raw_json())` for better performance.

## Writing Lazy Views

Lazy views can be written back to JSON:

```cpp
auto& doc = *result;
auto user = doc["user"];

std::string output;
auto ec = glz::write_json(user, output);
// output contains the JSON for just the "user" field
```

The bytes written are the same bytes `raw_json()` returns, so writing a view is a copy of the original text rather than a re-serialization: formatting, key order, and number spelling are all preserved exactly.

The writer determines the value's extent under the *document's* options, which means a view over a buffer opened with `null_terminated = false` stays inside that buffer. It reports an error rather than writing anything if the value is truncated: a container that never reaches its closing bracket, or a string that never reaches its closing quote. A scalar that runs to the last byte of the document is complete, not truncated, and is written normally.

Like the rest of the lazy API, the writer does not validate scalars. A malformed literal is copied through as-is, exactly as `raw_json()` would return it.

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

8. **Use `glz::read_json(value, view)` for struct deserialization**: Glaze provides an overload of `read_json` that accepts `lazy_json_view` directly. Use `glz::read_json(obj, view)` instead of `glz::read_json(obj, view.raw_json())` - it's ~49% faster because it avoids scanning the value twice.

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

## Keys Containing Escapes

Key lookup matches the raw bytes in the document, so a key that is stored escaped must be looked up in its escaped form. `glz::escape_unicode` generates that form at compile time:

```cpp
std::string buffer = R"({"he said \"hi\"":1})";
auto doc = glz::lazy_json(buffer).value();

doc[glz::escape_unicode<R"(he said "hi")">].get<int64_t>();  // 1
doc[R"(he said "hi")"].get<int64_t>();                       // error - no such key
```

Non-ASCII keys need no special handling as long as the document stores them as raw UTF-8, which is what Glaze writes by default:

```cpp
std::string buffer = R"({"€uro":2})";
auto doc = glz::lazy_json(buffer).value();
doc["€uro"].get<int64_t>();  // 2
```

If the document instead stores them escaped, `glz::escape_unicode<"€uro">` produces the matching `\u20ACuro` form.

## See Also

- [Partial Read](./partial-read.md) - Compile-time partial reading with structs
- [Generic JSON](./generic-json.md) - Dynamic JSON with `glz::generic`
- [Reading](./reading.md) - Standard JSON reading with `glz::read_json`
- [JSON Pointer Syntax](./json-pointer-syntax.md) - Alternative path-based access
