# JSON Patch (RFC 6902)

Glaze provides full support for [JSON Patch (RFC 6902)](https://datatracker.ietf.org/doc/html/rfc6902), a format for describing changes to JSON documents. JSON Patch works with [glz::generic](./generic-json.md) for runtime JSON manipulation.

> **See also:** [JSON Merge Patch (RFC 7386)](./json-merge-patch.md) for a simpler alternative when you don't need fine-grained control over array elements or explicit null values.

## Include

```c++
#include "glaze/json/patch.hpp"
```

## Basic Usage

### Applying a Patch

Use `glz::patch()` to apply a patch document to a JSON value:

```c++
auto doc = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");

glz::patch_document ops = {
   {glz::patch_op_type::replace, "/a", glz::generic(10.0), std::nullopt},
   {glz::patch_op_type::add, "/c", glz::generic(3.0), std::nullopt}
};

auto ec = glz::patch(*doc, ops);
if (!ec) {
   // doc is now {"a": 10, "b": 2, "c": 3}
}
```

### Generating a Patch

Use `glz::diff()` to generate a patch that transforms one document into another:

```c++
auto source = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
auto target = glz::read_json<glz::generic>(R"({"a": 1, "c": 3})");

auto patch = glz::diff(*source, *target);
if (patch) {
   // patch contains:
   // - remove "/b"
   // - add "/c" with value 3
}
```

## Patch Operations

JSON Patch supports six operations defined by RFC 6902:

### add

Adds a value at the specified path. If the path points to an array index, the value is inserted at that position.

```c++
// Add to object
glz::patch_op{glz::patch_op_type::add, "/newkey", glz::generic("value"), std::nullopt}

// Insert into array at index 1
glz::patch_op{glz::patch_op_type::add, "/array/1", glz::generic(42.0), std::nullopt}

// Append to array using "-"
glz::patch_op{glz::patch_op_type::add, "/array/-", glz::generic(99.0), std::nullopt}
```

### remove

Removes the value at the specified path.

```c++
glz::patch_op{glz::patch_op_type::remove, "/keytoremove", std::nullopt, std::nullopt}
```

### replace

Replaces the value at the specified path. The path must exist.

```c++
glz::patch_op{glz::patch_op_type::replace, "/existing", glz::generic("newvalue"), std::nullopt}
```

### move

Moves a value from one location to another. Equivalent to remove + add.

```c++
// Move value from /a to /b
glz::patch_op{glz::patch_op_type::move, "/b", std::nullopt, "/a"}
```

### copy

Copies a value from one location to another.

```c++
// Copy value from /a to /b
glz::patch_op{glz::patch_op_type::copy, "/b", std::nullopt, "/a"}
```

### test

Tests that the value at the specified path equals the given value. If the test fails, the entire patch operation fails.

```c++
// Verify /version equals 1 before proceeding
glz::patch_op{glz::patch_op_type::test, "/version", glz::generic(1.0), std::nullopt}
```

## Convenience Functions

### patched() - Non-mutating Patch

Returns a new document with the patch applied, leaving the original unchanged:

```c++
auto doc = glz::read_json<glz::generic>(R"({"a": 1})");
glz::patch_document ops = {{glz::patch_op_type::add, "/b", glz::generic(2.0), std::nullopt}};

auto result = glz::patched(*doc, ops);
if (result) {
   // *result is {"a": 1, "b": 2}
   // *doc is still {"a": 1}
}
```

### patch_json() - String Convenience

Apply a JSON patch string to a JSON document string:

```c++
auto result = glz::patch_json(
   R"({"a": 1})",
   R"([{"op": "add", "path": "/b", "value": 2}])"
);
if (result) {
   // *result is the JSON string: {"a":1,"b":2}
}
```

### diff() with Strings

Generate a patch from JSON strings:

```c++
auto patch = glz::diff(
   std::string_view{R"({"a": 1})"},
   std::string_view{R"({"a": 2})"}
);
```

## Options

### patch_opts

```c++
struct patch_opts {
   // If true, create intermediate objects for add operations (like mkdir -p)
   // Default: false (RFC 6902 compliant - parent must exist)
   bool create_intermediate = false;

   // If true, rollback all changes on any operation failure
   // Default: true (atomic application)
   // Note: Requires O(n) space and time for backup copy of large documents
   bool atomic = true;
};
```

Example with `create_intermediate`:

```c++
auto doc = glz::read_json<glz::generic>("{}");

glz::patch_opts opts{.create_intermediate = true};
glz::patch_document ops = {{glz::patch_op_type::add, "/a/b/c", glz::generic(42.0), std::nullopt}};

auto ec = glz::patch(*doc, ops, opts);
// doc is now {"a": {"b": {"c": 42}}}
```

### diff_opts

```c++
struct diff_opts {
   // Reserved for future implementation:
   bool detect_moves = false;   // Detect move operations
   bool detect_copies = false;  // Detect copy operations
   bool array_lcs = false;      // Use LCS for smarter array diffs
};
```

## Error Handling

`glz::patch()` returns an `error_ctx`. Common error codes:

| Error Code | Description |
|------------|-------------|
| `nonexistent_json_ptr` | Path does not exist (for remove/replace/test/move/copy) |
| `patch_test_failed` | Test operation value mismatch |
| `missing_key` | Required field (value/from) missing in patch operation |
| `syntax_error` | Invalid operation or move into self |
| `invalid_json_pointer` | Malformed JSON pointer syntax |

```c++
auto ec = glz::patch(*doc, ops);
if (ec) {
   if (ec.ec == glz::error_code::patch_test_failed) {
      // Test operation failed - values didn't match
   }
   else if (ec.ec == glz::error_code::nonexistent_json_ptr) {
      // Path doesn't exist
   }
}
```

## Round-trip Example

A common pattern is to diff two documents, serialize the patch, and apply it elsewhere:

```c++
// Generate patch
auto source = glz::read_json<glz::generic>(R"({"name": "Alice", "age": 30})");
auto target = glz::read_json<glz::generic>(R"({"name": "Alice", "age": 31, "city": "NYC"})");

auto patch = glz::diff(*source, *target);

// Serialize patch to JSON
auto patch_json = glz::write_json(*patch);
// [{"op":"replace","path":"/age","value":31},{"op":"add","path":"/city","value":"NYC"}]

// Later, deserialize and apply
auto ops = glz::read_json<glz::patch_document>(*patch_json);
auto doc = glz::read_json<glz::generic>(R"({"name": "Alice", "age": 30})");
glz::patch(*doc, *ops);
// doc now matches target
```

## JSON Serialization Format

Patch operations serialize to standard RFC 6902 format:

```json
[
   {"op": "add", "path": "/foo", "value": "bar"},
   {"op": "remove", "path": "/old"},
   {"op": "replace", "path": "/count", "value": 42},
   {"op": "move", "path": "/new", "from": "/old"},
   {"op": "copy", "path": "/backup", "from": "/data"},
   {"op": "test", "path": "/version", "value": 1}
]
```

## See Also

- [JSON Merge Patch (RFC 7386)](./json-merge-patch.md) - Simpler patching for partial updates
- [Generic JSON](./generic-json.md) - Working with `glz::generic`
- [JSON Pointer Syntax](./json-pointer-syntax.md) - Path syntax used by JSON Patch
