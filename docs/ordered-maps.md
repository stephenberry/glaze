# Ordered Maps

Glaze provides two insertion-order-preserving map containers. Both maintain the order in which keys are inserted while providing fast lookups.

## glz::ordered_small_map

`#include "glaze/containers/ordered_small_map.hpp"`

A compact, string-keyed ordered map optimized for small to medium JSON objects. This is the default map backend for [`glz::generic`](./generic-json.md).

```cpp
glz::ordered_small_map<int> m;
m["first"] = 1;
m["second"] = 2;
m["third"] = 3;

// Iteration order matches insertion order
for (auto& [key, value] : m) {
   // "first" → 1, "second" → 2, "third" → 3
}
```

### Characteristics

- **Keys**: `std::string` only
- **Empty size**: 24 bytes
- **Lookup (≤8 entries)**: Linear scan (no index overhead)
- **Lookup (>8 entries)**: Sorted hash index with binary search, O(log n)
- **Heterogeneous lookup**: Supports `std::string_view` and types convertible to it
- **Bloom filter**: For maps with 9–128 entries, a 128-byte bloom filter accelerates duplicate detection during insertion

### API

```cpp
glz::ordered_small_map<T> m;

// Insertion
m["key"] = value;
m.emplace("key", value);
m.try_emplace("key", args...);
m.insert({"key", value});

// Lookup
auto it = m.find("key");       // accepts string_view
bool has = m.contains("key");
T& val = m.at("key");          // throws if missing

// Removal
m.erase(it);
m.erase("key");
m.clear();

// Capacity
m.size();
m.empty();
m.reserve(n);
m.shrink_to_fit();

// Iteration (insertion order)
for (auto& [key, value] : m) { ... }
```

## glz::ordered_map

`#include "glaze/containers/ordered_map.hpp"`

A general-purpose ordered map using Robin Hood hashing. Supports any key type, custom hash functions, and custom equality comparators.

```cpp
glz::ordered_map<std::string, int> m;
m["first"] = 1;
m["second"] = 2;
m["third"] = 3;

// Iteration order matches insertion order
for (auto& [key, value] : m) {
   // "first" → 1, "second" → 2, "third" → 3
}
```

### Characteristics

- **Keys**: Any type (generic)
- **Lookup**: O(1) average via Robin Hood hash table
- **Custom hash/equality**: Template parameters for `Hash` and `KeyEqual`
- **Transparent lookup**: Supported when `Hash::is_transparent` and `KeyEqual::is_transparent` are defined
- **Unordered erase**: O(1) amortized removal that trades insertion order for speed

### API

```cpp
glz::ordered_map<Key, T, Hash, KeyEqual, Allocator> m;

// Insertion
m["key"] = value;
m.emplace(key, value);
m.try_emplace(key, args...);
m.insert({key, value});
m.insert_or_assign(key, value);

// Lookup
auto it = m.find(key);
bool has = m.contains(key);
T& val = m.at(key);           // throws if missing

// Ordered removal (preserves insertion order, O(n))
m.erase(it);
m.erase(key);

// Unordered removal (breaks insertion order, O(1) amortized)
m.unordered_erase(it);
m.unordered_erase(key);

m.clear();

// Positional access
auto& [k, v] = m.front();
auto& [k, v] = m.back();
auto it = m.nth(2);           // iterator to 3rd element
auto* ptr = m.data();         // pointer to underlying array

// Hash policy
m.reserve(n);
m.rehash(bucket_count);
float lf = m.load_factor();
m.max_load_factor(0.8f);

// Iteration (insertion order)
for (auto& [key, value] : m) { ... }
```

## Choosing Between Them

| | ordered_small_map | ordered_map |
|---|---|---|
| **Best for** | JSON objects, string keys | Generic keys, large maps |
| **Key types** | `std::string` only | Any hashable type |
| **Lookup** | O(n) small, O(log n) large | O(1) average |
| **Empty overhead** | 24 bytes | Larger (vector + hash table) |
| **Small maps (≤8 keys)** | No index allocated | Hash table always present |
| **O(1) erase** | No | Yes (`unordered_erase`) |
| **Custom hash/equality** | No | Yes |

**Use `ordered_small_map` when:**
- Keys are strings (the typical JSON case)
- Maps are small to medium (most JSON objects)
- Memory efficiency matters
- You want the simplest, most compact option

**Use `ordered_map` when:**
- Keys are not strings (e.g., `int`, custom types)
- You need O(1) lookup for large maps
- You need `unordered_erase` for O(1) removal
- You need custom hash or equality functions

## Using with glz::generic

By default, `glz::generic` uses `ordered_small_map` for JSON objects. You can substitute any compatible map type:

```cpp
// Default: insertion-order preservation with ordered_small_map
glz::generic json{};

// Lexicographically sorted keys with std::map
template <class T>
using sorted_map = std::map<std::string, T, std::less<>>;

using sorted_generic = glz::generic_json<glz::num_mode::f64, sorted_map>;

// Robin Hood hashing with ordered_map
template <class T>
using robin_map = glz::ordered_map<std::string, T>;

using robin_generic = glz::generic_json<glz::num_mode::f64, robin_map>;
```

## See Also

- [Generic JSON](./generic-json.md) - Using generic JSON with configurable map backends
- [STL Support](./stl-support.md) - Standard library container support
