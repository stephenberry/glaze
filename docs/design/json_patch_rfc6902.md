# RFC 6902 JSON Patch Implementation Design

## Overview

This document outlines the design for implementing RFC 6902 JSON Patch support in glaze, providing `diff` and `patch` functionality for `glz::generic` JSON values.

### Goals

1. **RFC 6902 Compliance** - Support all six patch operations: `add`, `remove`, `replace`, `move`, `copy`, `test`
2. **Idiomatic glaze API** - Follow existing patterns (e.g., `expected` return types, options structs)
3. **Performance** - Minimize allocations, support in-place modifications
4. **Correctness** - Atomic patch application with rollback on failure

### Non-Goals

1. Compile-time struct diffing (future enhancement)
2. JSON Merge Patch (RFC 7386) - separate feature
3. Three-way merge functionality

---

## API Design

### Patch Operation Structure

```cpp
namespace glz
{
   // RFC 6902 operation types
   enum struct patch_op_type : uint8_t {
      add,
      remove,
      replace,
      move,
      copy,
      test
   };

   // Single patch operation
   // Uses std::optional for fields that are only present for certain operations:
   // - value: required for add, replace, test
   // - from: required for move, copy
   struct patch_op
   {
      patch_op_type op{};
      std::string path{};                    // JSON Pointer (RFC 6901)
      std::optional<generic> value{};        // For add, replace, test
      std::optional<std::string> from{};     // For move, copy
   };

   // patch_op is reflectable - no glz::meta needed since member names
   // match the JSON keys. std::optional fields are automatically omitted
   // when std::nullopt during serialization.

   template <>
   struct meta<patch_op_type>
   {
      using enum patch_op_type;
      static constexpr auto value = enumerate(add, remove, replace, move, copy, test);
   };

   // A patch document is an array of operations
   using patch_document = std::vector<patch_op>;
}
```

### Primary Functions

```cpp
namespace glz
{
   // Generate a patch document that transforms 'source' into 'target'
   // Guarantees:
   //   auto doc = source;
   //   patch(doc, diff(source, target));
   //   assert(doc == target);
   [[nodiscard]] expected<patch_document, error_ctx> diff(
      const generic& source,
      const generic& target,
      diff_opts opts = {}
   );

   // Apply a patch document to a JSON value (in-place modification)
   [[nodiscard]] error_ctx patch(
      generic& document,
      const patch_document& ops,
      patch_opts opts = {}
   );

   // Apply a patch document, returning a new value (non-mutating)
   [[nodiscard]] expected<generic, error_ctx> patched(
      const generic& document,
      const patch_document& ops,
      patch_opts opts = {}
   );

   // Convenience overloads for JSON string input
   [[nodiscard]] expected<patch_document, error_ctx> diff(
      std::string_view source_json,
      std::string_view target_json,
      diff_opts opts = {}
   );

   [[nodiscard]] expected<std::string, error_ctx> patch_json(
      std::string_view document_json,
      std::string_view patch_json,
      patch_opts opts = {}
   );
}
```

### Options Structure

```cpp
namespace glz
{
   struct diff_opts
   {
      // Generate move operations when a value is removed and added elsewhere
      // Default: false (only generates add/remove/replace like nlohmann)
      bool detect_moves = false;

      // Generate copy operations when identical values appear in target
      // Default: false
      bool detect_copies = false;

      // For arrays: use LCS (longest common subsequence) for smarter diffs
      // Default: false (simpler index-based comparison)
      bool array_lcs = false;
   };

   struct patch_opts
   {
      // If true, create intermediate objects/arrays for add operations
      // Default: false (RFC 6902 compliant - parent must exist)
      // RFC 6902 Section 4.1: "The target location MUST reference one of:
      // the root of the target document, a member to add to an existing object,
      // or an element to add to an existing array."
      bool create_intermediate = false;

      // If true, rollback all changes on any operation failure
      // Default: true (atomic application)
      // Note: Requires O(n) space for backup copy of document
      bool atomic = true;
   };
}
```

---

## Detailed Design

### Building on Existing Infrastructure

Glaze already has JSON Pointer support that this implementation should build upon:

| Existing Function | Location | Usage |
|-------------------|----------|-------|
| `navigate_to(generic*, sv)` | `generic.hpp` | Navigate to a location in generic JSON |
| `seek(func, value, json_ptr)` | `seek.hpp` | Call function on value at path |
| `glz::get<T>(value, path)` | `seek.hpp` | Get typed reference at path |
| `glz::set(value, path, val)` | `seek.hpp` | Set value at path |

The patch implementation should reuse these rather than creating parallel infrastructure.

### 1. Diff Algorithm

The diff algorithm recursively compares two `generic` values and generates patch operations.

#### Pseudocode

```
diff(source, target, path) -> operations:
    if source.type != target.type:
        return [replace(path, target)]

    switch source.type:
        case null, bool, number, string:
            if source != target:
                return [replace(path, target)]
            return []

        case object:
            ops = []
            // Keys removed from source
            for key in source.keys():
                if key not in target:
                    ops.append(remove(path + "/" + escape(key)))

            // Keys added or modified
            for key in target.keys():
                child_path = path + "/" + escape(key)
                if key not in source:
                    ops.append(add(child_path, target[key]))
                else:
                    ops.extend(diff(source[key], target[key], child_path))
            return ops

        case array:
            // Simple approach: compare by index
            ops = []
            min_len = min(source.size(), target.size())

            for i in range(min_len):
                ops.extend(diff(source[i], target[i], path + "/" + str(i)))

            // Handle length differences
            if target.size() > source.size():
                for i in range(min_len, target.size()):
                    ops.append(add(path + "/" + str(i), target[i]))
            else:
                // Remove from end to avoid index shifting
                for i in range(source.size() - 1, min_len - 1, -1):
                    ops.append(remove(path + "/" + str(i)))

            return ops
```

#### JSON Pointer Escaping (RFC 6901)

```cpp
// Escape special characters in JSON Pointer tokens
inline std::string escape_json_ptr(std::string_view token)
{
   std::string result;
   result.reserve(token.size());
   for (char c : token) {
      if (c == '~') result += "~0";
      else if (c == '/') result += "~1";
      else result += c;
   }
   return result;
}

// Unescape JSON Pointer tokens
// Returns error for malformed sequences (e.g., "~" at end, "~2")
// RFC 6901: "~" MUST be followed by "0" or "1"
inline expected<std::string, error_ctx> unescape_json_ptr(std::string_view token)
{
   std::string result;
   result.reserve(token.size());
   for (size_t i = 0; i < token.size(); ++i) {
      if (token[i] == '~') {
         if (i + 1 >= token.size()) {
            return unexpected(error_ctx{error_code::invalid_json_pointer});
         }
         if (token[i + 1] == '0') {
            result += '~';
            ++i;
         }
         else if (token[i + 1] == '1') {
            result += '/';
            ++i;
         }
         else {
            return unexpected(error_ctx{error_code::invalid_json_pointer});
         }
      }
      else {
         result += token[i];
      }
   }
   return result;
}
```

### 2. Patch Algorithm

The patch algorithm applies operations sequentially, with optional atomic rollback.

#### Operation Implementations

```cpp
// Add: Insert value at path
// - If path points to array index, insert at that position
// - If path points to object key, set that key
// - If path ends with "-", append to array
// - Parent must exist (unless create_intermediate option is set)
error_ctx apply_add(generic& doc, sv path, const generic& value, const patch_opts& opts);

// Remove: Delete value at path
// - Path must exist
error_ctx apply_remove(generic& doc, sv path);

// Replace: Set value at existing path
// - Path must exist
error_ctx apply_replace(generic& doc, sv path, const generic& value);

// Move: Remove from 'from', add to 'path'
// - Equivalent to remove(from) + add(path, removed_value)
// - 'from' must exist
// - Cannot move a value into itself (path cannot be prefix of from)
error_ctx apply_move(generic& doc, sv from, sv path, const patch_opts& opts);

// Copy: Copy value from 'from' to 'path'
// - Equivalent to add(path, get(from))
// - 'from' must exist
error_ctx apply_copy(generic& doc, sv from, sv path, const patch_opts& opts);

// Test: Verify value at path equals expected
// - Returns error if not equal or path doesn't exist
error_ctx apply_test(const generic& doc, sv path, const generic& expected);
```

#### Atomic Application

```cpp
error_ctx patch(generic& document, const patch_document& ops, patch_opts opts)
{
   if (opts.atomic) {
      // Deep copy for rollback - requires O(n) space
      generic backup = document;

      for (size_t i = 0; i < ops.size(); ++i) {
         auto ec = apply_operation(document, ops[i], opts);
         if (ec) {
            document = std::move(backup);  // Rollback
            ec.operation_index = i;  // Include which operation failed
            return ec;
         }
      }
   }
   else {
      for (size_t i = 0; i < ops.size(); ++i) {
         auto ec = apply_operation(document, ops[i], opts);
         if (ec) {
            ec.operation_index = i;
            return ec;
         }
      }
   }
   return {};
}
```

### 3. Helper Functions Required

New helper functions needed for `glz::generic`:

```cpp
namespace glz
{
   // Navigate to parent and get the final key/index
   // Returns {parent_ref, key} or error if path is invalid
   expected<std::pair<std::reference_wrapper<generic>, std::string>, error_ctx>
   navigate_to_parent(generic& root, sv json_ptr);

   // Insert a value at a JSON Pointer path
   // Parent must exist unless create_intermediate is true
   error_ctx insert_at(generic& root, sv path, generic value, bool create_intermediate = false);

   // Remove a value at a JSON Pointer path
   // Returns the removed value, or error if path doesn't exist
   expected<generic, error_ctx> remove_at(generic& root, sv path);

   // Deep equality comparison for generic values
   bool equal(const generic& a, const generic& b);
}
```

---

## Error Handling

### Error Codes

New error codes to add to `error_code` enum:

```cpp
enum struct error_code : uint32_t {
   // ... existing codes ...

   // JSON Pointer errors
   invalid_json_pointer,         // Malformed JSON pointer (e.g., "~" at end, "~2")

   // JSON Patch errors
   patch_test_failed,            // Test operation value mismatch
   patch_path_not_found,         // Path does not exist for remove/replace/test
   patch_from_not_found,         // 'from' path does not exist for move/copy
   patch_invalid_array_index,    // Array index out of bounds or invalid
   patch_move_into_self,         // Cannot move a value into itself
   patch_invalid_operation,      // Unknown operation type
   patch_missing_value,          // 'value' field missing for add/replace/test
   patch_missing_from,           // 'from' field missing for move/copy
};
```

### Error Context

Errors should include:
- The operation index that failed
- The path that caused the error
- For test failures: expected vs actual values (in error message)

---

## File Organization

```
include/glaze/json/
├── patch.hpp           # Main patch/diff API
└── generic.hpp         # (existing) Add helper functions

tests/json_test/
└── json_patch_test.cpp # Comprehensive test suite
```

---

## Testing Strategy

### Unit Tests

1. **Basic operations** - Each operation type in isolation
2. **Round-trip** - `patch(source, diff(source, target))` yields `target`
3. **RFC 6902 test suite** - Use official JSON Patch test cases from [json-patch-tests](https://github.com/json-patch/json-patch-tests)
4. **Edge cases**:
   - Empty documents
   - Nested arrays/objects
   - Special characters in keys (escaping)
   - Array index "-" (append)
   - Deep nesting
   - Large documents

5. **Malformed JSON Pointers**:
   - `"/a~"` - tilde at end
   - `"/a~2b"` - invalid escape sequence
   - `"//a"` - empty segment
   - `"a/b"` - missing leading slash

6. **Type mismatches**:
   - Add to a string value
   - Remove from a number
   - Array index on object

7. **Unicode keys**:
   - `{"日本語": 1}` and proper escaping
   - Emoji keys
   - Multi-byte UTF-8

8. **Root document operations**:
   - Path `""` (empty string = root)
   - Replace entire document

9. **Array index edge cases**:
   - Empty array operations
   - Index beyond bounds
   - Negative indices (should fail)
   - Leading zeros (e.g., `"/01"`)

10. **Self-referential operations**:
    - Move where `from` is prefix of `path`
    - Copy to same location

### Example Test Cases

```cpp
ut::test("diff replace primitive") = [] {
   auto source = glz::read_json<glz::generic>(R"({"a": 1})");
   auto target = glz::read_json<glz::generic>(R"({"a": 2})");

   auto patch = glz::diff(*source, *target);
   ut::expect(patch.has_value());
   ut::expect(patch->size() == 1);
   ut::expect((*patch)[0].op == glz::patch_op_type::replace);
   ut::expect((*patch)[0].path == "/a");
};

ut::test("patch round-trip") = [] {
   auto source = glz::read_json<glz::generic>(R"({"a": [1, 2], "b": "hello"})");
   auto target = glz::read_json<glz::generic>(R"({"a": [1, 3], "c": true})");

   auto patch = glz::diff(*source, *target);
   ut::expect(patch.has_value());

   auto result = *source;
   auto ec = glz::patch(result, *patch);
   ut::expect(!ec);
   ut::expect(glz::equal(result, *target));
};

ut::test("remove operation serialization") = [] {
   glz::patch_op op{};
   op.op = glz::patch_op_type::remove;
   op.path = "/foo";
   // value and from are std::nullopt

   auto json = glz::write_json(op);
   ut::expect(json == R"({"op":"remove","path":"/foo"})");
};

ut::test("malformed json pointer") = [] {
   auto result = glz::unescape_json_ptr("a~");
   ut::expect(!result.has_value());
   ut::expect(result.error().ec == glz::error_code::invalid_json_pointer);
};
```

---

## Performance Considerations

1. **Avoid unnecessary copies** - Use move semantics where possible
2. **Pre-allocate** - Reserve vector capacity based on document size estimates
3. **Reuse existing infrastructure** - Build on `navigate_to()`, `seek()`, etc.
4. **Lazy evaluation** - For large documents, consider streaming diff generation

### Complexity

| Operation | Time Complexity | Space Complexity |
|-----------|-----------------|------------------|
| diff (objects) | O(n + m) | O(d) where d = diff size |
| diff (arrays) | O(max(n, m)) | O(d) |
| patch | O(p × log(n)) | O(1) non-atomic, O(n) atomic |

Where n, m = document sizes, p = patch size, d = diff size

**Note:** Atomic mode requires a full deep copy of the document for rollback, which uses O(n) additional space. For very large documents where memory is a concern, users can disable atomic mode.

---

## Future Enhancements

1. **JSON Merge Patch (RFC 7386)** - Simpler but less expressive format
2. **Compile-time struct diff** - Leverage glaze's reflection for typed diffs
3. **Streaming patch application** - For very large documents
4. **Bidirectional diff** - Generate both forward and reverse patches
5. **Conflict detection** - For three-way merge scenarios

---

## Design Decisions

These questions have been resolved based on RFC compliance and practical considerations:

| Question | Decision | Rationale |
|----------|----------|-----------|
| Array diffing | Index-based default | O(n) vs O(n×m) for LCS; LCS rarely needed |
| Move/copy detection | Off by default | Adds hashing overhead; matches nlohmann behavior |
| Patch format | Array-only | RFC 6902 specifies array format only |
| Error recovery | Strict abort | RFC 6902 requires abort on first error |
| `create_intermediate` | `false` default | RFC 6902 requires parent to exist |

---

## References

- [RFC 6902 - JSON Patch](https://datatracker.ietf.org/doc/html/rfc6902)
- [RFC 6901 - JSON Pointer](https://datatracker.ietf.org/doc/html/rfc6901)
- [RFC 7386 - JSON Merge Patch](https://datatracker.ietf.org/doc/html/rfc7386)
- [nlohmann/json patch implementation](https://json.nlohmann.me/features/json_patch/)
- [JSON Patch test suite](https://github.com/json-patch/json-patch-tests)
