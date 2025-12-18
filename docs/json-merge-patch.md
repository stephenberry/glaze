# JSON Merge Patch (RFC 7386)

Glaze provides full support for [JSON Merge Patch (RFC 7386)](https://datatracker.ietf.org/doc/html/rfc7386), a format for describing modifications to JSON documents. JSON Merge Patch works with [glz::generic](./generic-json.md) for runtime JSON manipulation.

## Include

```c++
#include "glaze/json/patch.hpp"
```

## When to Use JSON Merge Patch

JSON Merge Patch is simpler than [JSON Patch (RFC 6902)](./json-patch.md) but has some limitations:

| Use Case | Recommendation |
|----------|----------------|
| Partial update with simple values | Merge Patch |
| Need to set explicit null values | JSON Patch |
| Array element manipulation | JSON Patch |
| Move/copy operations | JSON Patch |
| Simple API for consumers | Merge Patch |
| Conditional updates (test operation) | JSON Patch |

## Basic Usage

### Applying a Merge Patch

Use `glz::merge_patch()` to apply a merge patch to a JSON value:

```c++
auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");

auto patch = glz::read_json<glz::generic>(R"({"a": 99, "c": 3})");

auto ec = glz::merge_patch(*target, *patch);
if (!ec) {
   // target is now {"a": 99, "b": 2, "c": 3}
}
```

### Removing Keys with Null

A `null` value in the patch removes the corresponding key:

```c++
auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2, "c": 3})");

auto patch = glz::read_json<glz::generic>(R"({"b": null})");

glz::merge_patch(*target, *patch);
// target is now {"a": 1, "c": 3}
```

### Generating a Merge Patch

Use `glz::merge_diff()` to generate a merge patch that transforms one document into another:

```c++
auto source = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
auto target = glz::read_json<glz::generic>(R"({"a": 1, "c": 3})");

auto patch = glz::merge_diff(*source, *target);
if (patch) {
   // patch is {"b": null, "c": 3}
   // - "b": null means remove key "b"
   // - "c": 3 means add key "c" with value 3
}
```

## Using Merge Patch with C++ Structs

In addition to working with `glz::generic`, merge patch can be applied directly to C++ structs. This leverages Glaze's automatic reflection to apply partial updates to your types.

> **Implementation Note**: Glaze's `read_json` naturally has merge-patch semantics for structs—it only updates fields present in the JSON and leaves other fields unchanged. The `merge_patch` function for structs leverages this directly, making it highly efficient with no intermediate serialization or data structure conversions.

### Basic Struct Patching

```c++
struct Person {
   std::string name;
   int age;
   std::string city;
};

Person person{.name = "Alice", .age = 30, .city = "NYC"};

// Apply a partial update - only change the age
auto patch = glz::read_json<glz::generic>(R"({"age": 31})");
auto ec = glz::merge_patch(person, *patch);

if (!ec) {
   // person.name is still "Alice"
   // person.age is now 31
   // person.city is still "NYC"
}
```

### Patching from JSON String

You can apply a patch directly from a JSON string:

```c++
Person person{.name = "Bob", .age = 25, .city = "LA"};

auto ec = glz::merge_patch(person, R"({"city": "San Francisco"})");
// person.city is now "San Francisco"
```

### Non-Mutating Struct Patching

Use `glz::merge_patched()` to get a new struct without modifying the original:

```c++
Person original{.name = "Charlie", .age = 40, .city = "Boston"};

auto result = glz::merge_patched(original, R"({"age": 41})");
if (result) {
   // original.age is still 40
   // result->age is 41
}
```

### Generating Patches Between Structs

Use `glz::merge_diff()` to compute the difference between two struct instances:

```c++
Person before{.name = "Dave", .age = 35, .city = "Seattle"};
Person after{.name = "Dave", .age = 36, .city = "Portland"};

auto patch = glz::merge_diff(before, after);
if (patch) {
   // patch contains: {"age": 36, "city": "Portland"}
   // "name" is not in the patch because it didn't change
}

// Get the patch as a JSON string
auto patch_json = glz::merge_diff_json(before, after);
// *patch_json is: {"age":36,"city":"Portland"}
```

### Round-Trip with Structs

You can generate a patch and apply it to transform one struct into another:

```c++
struct Config {
   std::string host = "localhost";
   int port = 8080;
   bool enabled = true;
   std::vector<std::string> tags;
};

Config source{.host = "localhost", .port = 8080, .enabled = true, .tags = {"dev"}};
Config target{.host = "production.example.com", .port = 443, .enabled = true, .tags = {"prod"}};

// Generate patch
auto patch = glz::merge_diff(source, target);

// Apply patch to transform source into target
Config result = source;
glz::merge_patch(result, *patch);

// result now matches target:
// result.host == "production.example.com"
// result.port == 443
// result.tags == {"prod"}
```

### Nested Structs

Merge patch works with nested structures:

```c++
struct Address {
   std::string street;
   std::string city;
};

struct Employee {
   std::string name;
   Address address;
};

Employee emp{
   .name = "Eve",
   .address = {.street = "123 Main St", .city = "Boston"}
};

// Update just the city in the nested address
auto ec = glz::merge_patch(emp, R"({"address": {"city": "NYC"}})");

// emp.address.street is still "123 Main St"
// emp.address.city is now "NYC"
```

**Note**: When patching nested objects, the entire nested object in the patch is merged with the target. If you want to completely replace a nested object, include all its fields in the patch.

### Struct API Reference

```c++
// Apply patch to struct (in-place)
template <class T>
[[nodiscard]] error_ctx merge_patch(T& target, const generic& patch);

// Apply patch from JSON string to struct (in-place)
template <class T>
[[nodiscard]] error_ctx merge_patch(T& target, std::string_view patch_json);

// Apply patch to struct (non-mutating)
template <class T>
[[nodiscard]] expected<T, error_ctx> merge_patched(const T& target, const generic& patch);

// Apply patch from JSON string (non-mutating)
template <class T>
[[nodiscard]] expected<T, error_ctx> merge_patched(const T& target, std::string_view patch_json);

// Generate patch between two structs
template <class T>
[[nodiscard]] expected<generic, error_ctx> merge_diff(const T& source, const T& target);

// Generate patch between two structs as JSON string
template <class T>
[[nodiscard]] expected<std::string, error_ctx> merge_diff_json(const T& source, const T& target);
```

## Key Semantics

### Objects Are Merged Recursively

When both the target and patch have nested objects at the same key, they are merged recursively:

```c++
auto target = glz::read_json<glz::generic>(R"({
   "user": {"name": "Alice", "age": 30}
})");

auto patch = glz::read_json<glz::generic>(R"({
   "user": {"age": 31, "city": "NYC"}
})");

glz::merge_patch(*target, *patch);
// target is now:
// {"user": {"name": "Alice", "age": 31, "city": "NYC"}}
```

### Arrays Are Replaced Entirely

Unlike JSON Patch, Merge Patch cannot modify individual array elements:

```c++
auto target = glz::read_json<glz::generic>(R"({"tags": ["a", "b", "c"]})");

auto patch = glz::read_json<glz::generic>(R"({"tags": ["x"]})");

glz::merge_patch(*target, *patch);
// target is now {"tags": ["x"]} - complete replacement
```

### Non-Object Patches Replace the Target

If the patch is not an object, it completely replaces the target:

```c++
auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");

auto patch = glz::read_json<glz::generic>(R"(42)");

glz::merge_patch(*target, *patch);
// target is now 42
```

## API Reference

### merge_patch

Apply a merge patch in-place:

```c++
[[nodiscard]] error_ctx merge_patch(
   generic& target,
   const generic& patch
);
```

### merge_patched

Apply a merge patch, returning a new value (non-mutating):

```c++
[[nodiscard]] expected<generic, error_ctx> merge_patched(
   const generic& target,
   const generic& patch
);
```

### merge_diff

Generate a merge patch that transforms source into target:

```c++
[[nodiscard]] expected<generic, error_ctx> merge_diff(
   const generic& source,
   const generic& target
);
```

### Convenience Overloads

```c++
// Apply patch from JSON string
[[nodiscard]] error_ctx merge_patch(
   generic& target,
   std::string_view patch_json
);

// Apply patch from strings, return JSON string
[[nodiscard]] expected<std::string, error_ctx> merge_patch_json(
   std::string_view target_json,
   std::string_view patch_json
);

// Apply patch from strings, return generic
[[nodiscard]] expected<generic, error_ctx> merge_patched(
   std::string_view target_json,
   std::string_view patch_json
);

// Generate patch from strings, return JSON string
[[nodiscard]] expected<std::string, error_ctx> merge_diff_json(
   std::string_view source_json,
   std::string_view target_json
);
```

## Limitations

### Cannot Set Explicit Null Values

RFC 7386 uses `null` to mean "remove this key", so you cannot use merge patch to set a value to `null`. This is documented in the RFC:

> "This design means that merge patch documents are suitable for describing modifications to JSON documents that primarily use objects for their structure and do not make use of explicit null values."

For use cases requiring explicit null values, use [JSON Patch (RFC 6902)](./json-patch.md) instead.

### Round-Trip Limitation

When generating a merge patch with `merge_diff()`, documents containing explicit null values cannot be perfectly reconstructed:

```c++
auto source = glz::read_json<glz::generic>(R"({"a": 1})");
auto target = glz::read_json<glz::generic>(R"({"a": null})");

auto patch = glz::merge_diff(*source, *target);
// patch is {"a": null}

auto result = *source;
glz::merge_patch(result, *patch);
// result is {} - the key "a" was removed, not set to null
```

## HTTP Integration

When using JSON Merge Patch with HTTP, use the MIME type `application/merge-patch+json`:

```c++
// Example with HTTP client
auto response = client.patch(url, patch_json, {
   {"Content-Type", "application/merge-patch+json"}
});
```

## RFC 7386 Appendix A Example

This example from RFC 7386 demonstrates all key behaviors:

```c++
auto target = glz::read_json<glz::generic>(R"({
   "title": "Goodbye!",
   "author": {
      "givenName": "John",
      "familyName": "Doe"
   },
   "tags": ["example", "sample"],
   "content": "This will be unchanged"
})");

auto patch = glz::read_json<glz::generic>(R"({
   "title": "Hello!",
   "phoneNumber": "+01-123-456-7890",
   "author": {
      "familyName": null
   },
   "tags": ["example"]
})");

glz::merge_patch(*target, *patch);

// Result:
// {
//    "title": "Hello!",              // replaced
//    "author": {
//       "givenName": "John"          // preserved
//                                    // familyName removed
//    },
//    "tags": ["example"],            // replaced entirely
//    "content": "This will be unchanged",  // preserved
//    "phoneNumber": "+01-123-456-7890"     // added
// }
```

## Error Handling

`glz::merge_patch()` returns an `error_ctx`. Possible errors:

| Error Code | Description |
|------------|-------------|
| `parse_error` | Invalid JSON in string input |
| `exceeded_max_recursive_depth` | Recursion depth exceeded the compile-time limit (256) |

Unlike JSON Patch, merge patch operations cannot fail due to missing paths or type mismatches - the algorithm handles all cases by design.

## See Also

- [JSON Patch (RFC 6902)](./json-patch.md) - More expressive patching with explicit operations
- [Generic JSON](./generic-json.md) - Working with `glz::generic`
