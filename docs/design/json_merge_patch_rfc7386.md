# RFC 7386 JSON Merge Patch Implementation Design

## Overview

This document outlines the design for implementing [RFC 7386 JSON Merge Patch](https://datatracker.ietf.org/doc/html/rfc7386) support in glaze. JSON Merge Patch provides a simpler alternative to JSON Patch (RFC 6902) for describing modifications to JSON documents.

### Related Issue

[GitHub Issue #975](https://github.com/stephenberry/glaze/issues/975) - Document current support and add more support for JSON Merge Patch

### Goals

1. **RFC 7386 Compliance** - Full support for the JSON Merge Patch algorithm
2. **Idiomatic glaze API** - Follow patterns established by JSON Patch implementation
3. **Performance** - Minimize allocations, support in-place modifications
4. **Integration** - Work seamlessly with `glz::generic` and optionally typed structs

### Non-Goals

1. Three-way merge functionality (potential future enhancement)
2. Conflict detection and resolution

---

## When to Use JSON Merge Patch vs JSON Patch

| Use Case | Recommendation |
|----------|----------------|
| Partial update with simple values | Merge Patch |
| Need to set explicit null values | JSON Patch |
| Array element manipulation | JSON Patch |
| Move/copy operations | JSON Patch |
| Simple API for consumers | Merge Patch |
| Conditional updates (test operation) | JSON Patch |
| Maximum expressiveness | JSON Patch |

### Format Comparison

| Aspect | JSON Patch (RFC 6902) | JSON Merge Patch (RFC 7386) |
|--------|----------------------|----------------------------|
| Format | Array of operation objects | Single JSON value |
| Null handling | Null is a value | Null means "remove" |
| Array operations | Index-based add/remove | Full replacement only |
| Complexity | More expressive | Simpler, more intuitive |
| MIME type | `application/json-patch+json` | `application/merge-patch+json` |

---

## JSON Merge Patch Algorithm (RFC 7386 Section 2)

```
define MergePatch(Target, Patch):
  if Patch is an Object:
    if Target is not an Object:
      Target = {} // Ignore the contents and set it to an empty Object
    for each Name/Value pair in Patch:
      if Value is null:
        if Name exists in Target:
          remove the Name/Value pair from Target
      else:
        Target[Name] = MergePatch(Target[Name], Value)
    return Target
  else:
    return Patch
```

### Key Semantics

1. **Objects are merged recursively** - Patch object members are applied to corresponding target members
2. **Null removes members** - A null value in the patch removes that key from the target
3. **Arrays are replaced entirely** - Unlike RFC 6902, there's no way to patch individual array elements
4. **Non-object patches replace** - If the patch is not an object, it completely replaces the target
5. **Non-object targets become objects** - If target isn't an object but patch is, target becomes an empty object first

---

## API Design

### Core Functions

```cpp
namespace glz
{
   // ============================================================================
   // RFC 7386 JSON Merge Patch
   // ============================================================================

   // Apply a merge patch to a JSON value (in-place modification)
   [[nodiscard]] error_ctx merge_patch(
      generic& target,
      const generic& patch
   );

   // Apply a merge patch, returning a new value (non-mutating)
   [[nodiscard]] expected<generic, error_ctx> merge_patched(
      const generic& target,
      const generic& patch
   );

   // Convenience overloads for JSON string input
   [[nodiscard]] error_ctx merge_patch(
      generic& target,
      std::string_view patch_json
   );

   [[nodiscard]] expected<std::string, error_ctx> merge_patch_json(
      std::string_view target_json,
      std::string_view patch_json
   );

   // String-to-generic convenience (parse both, return generic result)
   [[nodiscard]] expected<generic, error_ctx> merge_patched(
      std::string_view target_json,
      std::string_view patch_json
   );

   // Generate a merge patch that transforms 'source' into 'target'
   // Note: Due to null semantics, this cannot perfectly round-trip if
   // the target contains explicit null values (they would be interpreted as removals)
   [[nodiscard]] expected<generic, error_ctx> merge_diff(
      const generic& source,
      const generic& target
   );

   // String overload - returns JSON string
   [[nodiscard]] expected<std::string, error_ctx> merge_diff_json(
      std::string_view source_json,
      std::string_view target_json
   );
}
```

---

## Error Handling

### Possible Errors

| Error Code | Description |
|------------|-------------|
| `parse_error` | Invalid JSON in string input |
| `exceeded_max_recursive_depth` | Recursion depth exceeded the compile-time limit (256) |

Note: Unlike JSON Patch (RFC 6902), merge patch operations cannot fail due to missing paths or type mismatches - the algorithm handles all cases by design. Parse errors occur only when using string-input convenience overloads.

### Thread Safety

- All functions are **thread-safe** for distinct target documents
- Concurrent modification of the same `generic` instance requires external synchronization
- The `patch` parameter is read-only and can be shared across threads

---

## Implementation Details

### 1. Core Algorithm Implementation

```cpp
namespace glz::detail
{
   // Recursive merge patch implementation
   // Modifies target in-place according to RFC 7386 algorithm
   inline error_ctx apply_merge_patch_impl(
      generic& target,
      const generic& patch,
      uint32_t depth = 0
   )
   {
      if (depth >= max_recursive_depth_limit) [[unlikely]] {
         return error_ctx{error_code::exceeded_max_recursive_depth};
      }

      if (patch.is_object()) {
         // If target is not an object, replace with empty object
         if (!target.is_object()) {
            target.data = generic::object_t{};
         }

         auto& target_obj = target.get_object();
         const auto& patch_obj = patch.get_object();

         for (const auto& [key, value] : patch_obj) {
            if (value.is_null()) {
               // Null means remove
               target_obj.erase(key);
            }
            else if (value.is_object()) {
               // Recursively merge objects
               // Use try_emplace to avoid double lookup
               auto [it, inserted] = target_obj.try_emplace(key, generic{});
               auto ec = apply_merge_patch_impl(it->second, value, depth + 1);
               if (ec) {
                  return ec;
               }
            }
            else {
               // Non-object value - direct assignment (no recursion needed)
               target_obj[key] = value;
            }
         }
      }
      else {
         // Non-object patch replaces target entirely
         target = patch;
      }

      return {};
   }
}

namespace glz
{
   [[nodiscard]] inline error_ctx merge_patch(
      generic& target,
      const generic& patch
   )
   {
      return detail::apply_merge_patch_impl(target, patch);
   }
}
```

### 2. Merge Diff Algorithm

Generating a merge patch from two documents requires special handling due to null semantics:

```cpp
namespace glz::detail
{
   inline void merge_diff_impl(
      const generic& source,
      const generic& target,
      generic& patch
   )
   {
      // If types differ or source is not an object, return target as patch
      if (!source.is_object() || !target.is_object()) {
         patch = target;
         return;
      }

      // Both are objects - compute diff
      patch.data = generic::object_t{};
      auto& patch_obj = patch.get_object();
      const auto& source_obj = source.get_object();
      const auto& target_obj = target.get_object();

      // Check for removed keys (in source but not in target)
      for (const auto& [key, value] : source_obj) {
         if (target_obj.find(key) == target_obj.end()) {
            // Key was removed - emit null
            patch_obj.emplace(key, nullptr);
         }
      }

      // Check for added or modified keys
      for (const auto& [key, target_value] : target_obj) {
         auto source_it = source_obj.find(key);
         if (source_it == source_obj.end()) {
            // Key was added
            patch_obj.emplace(key, target_value);
         }
         else if (!equal(source_it->second, target_value)) {
            // Key was modified
            if (source_it->second.is_object() && target_value.is_object()) {
               // Both objects - recurse
               generic child_patch;
               merge_diff_impl(source_it->second, target_value, child_patch);
               patch_obj.emplace(key, std::move(child_patch));
            }
            else {
               // Different types or non-objects - replace
               patch_obj.emplace(key, target_value);
            }
         }
         // If equal, no patch entry needed
      }
   }
}
```

### 3. Handling Edge Cases

#### Null in Target Document

RFC 7386 has a limitation: you cannot use merge patch to set a value to null, because null in the patch means "remove". This is documented in RFC 7386 Section 1:

> "This design means that merge patch documents are suitable for describing modifications to JSON documents that primarily use objects for their structure and do not make use of explicit null values."

**Design Decision**: Document this limitation clearly. For use cases requiring explicit null values, recommend using JSON Patch (RFC 6902) instead.

#### Array Handling

Arrays are replaced entirely. The algorithm does not attempt to merge array contents:

```cpp
// source: {"tags": ["a", "b", "c"]}
// patch:  {"tags": ["x", "y"]}
// result: {"tags": ["x", "y"]}  // Complete replacement
```

#### Root-Level Non-Object

If the patch is not an object, it replaces the entire document:

```cpp
// source: {"a": 1, "b": 2}
// patch:  42
// result: 42
```

---

## Testing Strategy

### Unit Tests

1. **Basic merge operations**
   - Add new key
   - Modify existing key
   - Remove key (null value)
   - Nested object merge

2. **Type coercion cases**
   - Object target with non-object patch (replacement)
   - Non-object target with object patch (becomes object)
   - Array replacement

3. **Edge cases**
   - Empty patch `{}` (should be no-op)
   - Empty target
   - Patch removing all keys (result is empty object)
   - Deep nesting (verify depth limit works)
   - Unicode keys
   - Empty string keys `{"": "value"}`
   - Numeric string keys `{"0": "value", "1": "other"}`

4. **Round-trip tests**
   - `merge_patch(source, merge_diff(source, target))` should equal `target`
   - Explicit test verifying null limitation behavior

5. **Error cases**
   - Exceeding max_recursive_depth_limit (256)
   - Invalid JSON in string overloads

6. **Performance tests** (optional)
   - Large documents
   - Deeply nested structures

### Example Test Cases

```cpp
"merge_patch basic add"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"a": 1})");
   auto patch = glz::read_json<glz::generic>(R"({"b": 2})");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
   expect(glz::equal(*target, *expected));
};

"merge_patch remove with null"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
   auto patch = glz::read_json<glz::generic>(R"({"b": null})");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({"a": 1})");
   expect(glz::equal(*target, *expected));
};

"merge_patch empty patch is no-op"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
   auto original = *target;
   auto patch = glz::read_json<glz::generic>(R"({})");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);
   expect(glz::equal(*target, original));
};

"merge_patch remove all keys"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
   auto patch = glz::read_json<glz::generic>(R"({"a": null, "b": null})");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({})");
   expect(glz::equal(*target, *expected));
};

"merge_patch nested merge"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({
      "a": {"b": 1, "c": 2}
   })");
   auto patch = glz::read_json<glz::generic>(R"({
      "a": {"b": 99, "d": 3}
   })");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({
      "a": {"b": 99, "c": 2, "d": 3}
   })");
   expect(glz::equal(*target, *expected));
};

"merge_patch array replacement"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"tags": [1, 2, 3]})");
   auto patch = glz::read_json<glz::generic>(R"({"tags": ["x"]})");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({"tags": ["x"]})");
   expect(glz::equal(*target, *expected));
};

"merge_patch non-object replaces"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"a": 1})");
   auto patch = glz::read_json<glz::generic>(R"(42)");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);
   expect(target->is_number());
   expect(target->get_number() == 42.0);
};

"merge_patch empty string key"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"": 1, "a": 2})");
   auto patch = glz::read_json<glz::generic>(R"({"": 99})");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({"": 99, "a": 2})");
   expect(glz::equal(*target, *expected));
};

"merge_patch numeric string keys"_test = [] {
   auto target = glz::read_json<glz::generic>(R"({"0": "a", "1": "b"})");
   auto patch = glz::read_json<glz::generic>(R"({"1": "x", "2": "c"})");

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({"0": "a", "1": "x", "2": "c"})");
   expect(glz::equal(*target, *expected));
};

"merge_patch max_depth exceeded"_test = [] {
   // Create deeply nested structure exceeding max_recursive_depth_limit (256)
   glz::generic target;
   target.data = glz::generic::object_t{};

   glz::generic patch;
   patch.data = glz::generic::object_t{};

   glz::generic* current = &patch;
   for (size_t i = 0; i < glz::max_recursive_depth_limit + 10; ++i) {
      current->get_object()["nested"].data = glz::generic::object_t{};
      current = &current->get_object()["nested"];
   }

   auto ec = glz::merge_patch(target, patch);
   expect(ec.ec == glz::error_code::exceeded_max_recursive_depth);
};

"merge_diff generates correct patch"_test = [] {
   auto source = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
   auto target = glz::read_json<glz::generic>(R"({"a": 1, "c": 3})");

   auto patch = glz::merge_diff(*source, *target);
   expect(patch.has_value());

   // patch should be {"b": null, "c": 3}
   expect(patch->is_object());
   expect(patch->get_object().size() == 2u);
   expect((*patch)["b"].is_null());
   expect((*patch)["c"].get_number() == 3.0);
};

"merge_diff round-trip"_test = [] {
   auto source = glz::read_json<glz::generic>(R"({"a": 1, "b": {"x": 1}})");
   auto target = glz::read_json<glz::generic>(R"({"a": 2, "b": {"y": 2}, "c": 3})");

   auto patch = glz::merge_diff(*source, *target);
   expect(patch.has_value());

   auto result = *source;
   auto ec = glz::merge_patch(result, *patch);
   expect(!ec);
   expect(glz::equal(result, *target));
};

"merge_diff null limitation"_test = [] {
   // Demonstrate that explicit null in target cannot be preserved
   auto source = glz::read_json<glz::generic>(R"({"a": 1})");
   auto target = glz::read_json<glz::generic>(R"({"a": null})");

   auto patch = glz::merge_diff(*source, *target);
   expect(patch.has_value());

   // The patch will contain {"a": null}, but this means "remove a"
   // not "set a to null"
   auto result = *source;
   auto ec = glz::merge_patch(result, *patch);
   expect(!ec);

   // Result will NOT have "a" at all, not have "a": null
   expect(!result.contains("a"));
   // This is the documented limitation of RFC 7386
};

// RFC 7386 Appendix A example
"rfc7386 appendix a example"_test = [] {
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

   auto ec = glz::merge_patch(*target, *patch);
   expect(!ec);

   auto expected = glz::read_json<glz::generic>(R"({
      "title": "Hello!",
      "author": {
         "givenName": "John"
      },
      "tags": ["example"],
      "content": "This will be unchanged",
      "phoneNumber": "+01-123-456-7890"
   })");
   expect(glz::equal(*target, *expected));
};
```

---

## File Organization

```
include/glaze/json/
├── patch.hpp           # Add merge patch functions (alongside existing JSON Patch)
└── generic.hpp         # (existing) - no changes needed

tests/json_test/
├── json_patch_test.cpp          # Existing JSON Patch tests
└── json_merge_patch_test.cpp    # New JSON Merge Patch tests

docs/
├── json-patch.md              # (existing) - add reference to merge patch
└── json-merge-patch.md        # New documentation
```

### Header Integration

Add to `patch.hpp` after the existing JSON Patch implementation:

```cpp
// ============================================================================
// RFC 7386 JSON Merge Patch
// ============================================================================

// ... merge patch functions ...
```

---

## Documentation

### json-merge-patch.md Structure

1. **Overview** - What is JSON Merge Patch, when to use it
2. **Basic Usage** - Simple examples
3. **Key Concepts**
   - Null semantics
   - Array replacement
   - Recursive merging
4. **API Reference** - All functions with examples
5. **Limitations** - Cannot set explicit null values
6. **HTTP Integration** - Using with `application/merge-patch+json` content type
7. **Comparison with JSON Patch** - Decision matrix for when to use each
8. **See Also** - Links to JSON Patch, generic JSON docs

---

## Implementation Steps

### Phase 1: Core Implementation
1. Add `merge_patch()` function for `glz::generic`
2. Add `merge_patched()` non-mutating variant
3. Add string convenience overloads
4. Use `max_recursive_depth_limit` for stack overflow protection

### Phase 2: Diff Generation
5. Add `merge_diff()` function
6. Add `merge_diff_json()` string convenience overload

### Phase 3: Testing
7. Create comprehensive test suite
8. Include RFC 7386 examples from Appendix A
9. Add depth limit tests

### Phase 4: Documentation
10. Create `json-merge-patch.md` documentation
11. Update `json-patch.md` with cross-references
12. Close GitHub issue #975

---

## Implemented Features

### Typed Struct Support (Implemented)

Merge patches can be applied directly to C++ structs:

```cpp
template <class T>
error_ctx merge_patch(T& target, const generic& patch);

template <class T>
error_ctx merge_patch(T& target, std::string_view patch_json);

template <class T>
expected<T, error_ctx> merge_patched(const T& target, const generic& patch);

template <class T>
expected<generic, error_ctx> merge_diff(const T& source, const T& target);

template <class T>
expected<std::string, error_ctx> merge_diff_json(const T& source, const T& target);
```

This leverages existing reflection to apply partial updates to typed structs.

---

## Future Enhancements

### 1. HTTP PATCH Support

Integration with HTTP client for `application/merge-patch+json`:

```cpp
// Example HTTP client integration
auto response = client.patch(url, merge_patch_document, {
   {"Content-Type", "application/merge-patch+json"}
});
```

### 2. Validation Options

Option to reject patches that would create invalid states:

```cpp
struct merge_patch_opts
{
   bool strict_types = false;  // Reject type changes if true
};
```

### 3. Preserve Nulls Mode (Non-Standard Extension)

Option to treat null as a value instead of removal indicator:

```cpp
struct merge_patch_opts
{
   bool preserve_nulls = false;  // Non-standard: null becomes a value
};
```

Note: This would be a non-standard extension and should be clearly documented as such.

Note: Recursion depth protection is handled by the compile-time `max_recursive_depth_limit` constant (256).

---

## References

- [RFC 7386 - JSON Merge Patch](https://datatracker.ietf.org/doc/html/rfc7386)
- [RFC 6902 - JSON Patch](https://datatracker.ietf.org/doc/html/rfc6902) (existing implementation)
- [GitHub Issue #975](https://github.com/stephenberry/glaze/issues/975)
- MIME type: `application/merge-patch+json` (RFC 7386 Section 3)
