# ZMEM (Zero-Copy Memory Format)

ZMEM is a high-performance binary serialization format designed for maximum performance with **zero overhead for simple structs** and efficient handling of complex types with vectors and strings.

## Key Features

| Feature | Description |
|---------|-------------|
| **Zero overhead** | Simple structs serialize with exact `sizeof()` bytes - no headers, no tags |
| **Zero-copy reads** | Simple types can be read directly from the buffer without copying |
| **Deterministic** | Same logical value always produces identical bytes |
| **Little-endian** | Explicit wire format specification for cross-platform compatibility |
| **Pre-allocation** | Compute exact serialized size before writing to eliminate resize checks |

## When to Use ZMEM

ZMEM is ideal for:

- **High-frequency data**: Game physics, sensor data, network packets
- **Embedded systems**: Memory-constrained environments where every byte counts
- **Real-time systems**: Predictable performance with no allocations during serialization
- **Scientific/engineering data**: Structured numerical data with minimal overhead
- **Shared memory IPC**: Direct memory mapping with known layouts

For self-describing formats with schema evolution, consider [BEVE](./binary.md) or [MessagePack](./msgpack.md) instead.

## Basic Usage

```cpp
#include "glaze/zmem.hpp"

// Simple struct - trivially copyable (zero overhead)
struct Point {
   float x;
   float y;
};

Point p{1.0f, 2.0f};
std::string buffer;

// Write ZMEM
auto ec = glz::write_zmem(p, buffer);
if (!ec) {
   // Success: buffer.size() == sizeof(Point) == 8 bytes
}

// Read ZMEM
Point result{};
ec = glz::read_zmem(result, buffer);
if (!ec) {
   // Success: result.x == 1.0f, result.y == 2.0f
}
```

## Simple vs Complex Types

ZMEM distinguishes between two categories of types:

### Simple Types (Zero Overhead)

Simple types are trivially copyable and serialize with **zero overhead** - just their raw bytes:

- Primitives: `bool`, `int8_t` through `int64_t`, `uint8_t` through `uint64_t`, `float`, `double`
- Enums
- Fixed-size arrays: `T[N]`, `std::array<T, N>`
- Structs containing only simple types

```cpp
struct Transform {
   float position[3];
   float rotation[3];
   float scale[3];
};
static_assert(sizeof(Transform) == 36);

Transform t{{1,2,3}, {4,5,6}, {7,8,9}};
std::string buffer;
glz::write_zmem(t, buffer);
// buffer.size() == 36 (exact sizeof)
```

### Complex Types (With Headers)

Complex types contain variable-length data and require headers:

- `std::vector<T>`: 8-byte count + elements (+ offset table for complex elements)
- `std::string`: 8-byte length + character data
- `std::map<K, V>`: Count + entries (+ offset table for complex values)
- Structs containing vectors or strings: 8-byte size header + inline section + variable section

```cpp
struct Entity {
   uint64_t id;
   std::vector<float> weights;
   std::string name;
};

Entity e{42, {1.0f, 2.0f, 3.0f}, "Player"};
std::string buffer;
glz::write_zmem(e, buffer);
// Format: [size:8][id:8][weights_ref:16][name_ref:16][weights_data][name_data]
```

## Size Pre-computation

ZMEM supports computing the exact serialized size before writing, enabling pre-allocation to eliminate all resize checks during serialization:

```cpp
#include "glaze/zmem.hpp"

struct SensorData {
   uint64_t timestamp;
   std::vector<double> readings;
};

SensorData data{12345, {1.0, 2.0, 3.0, 4.0, 5.0}};

// Compute exact size
size_t size = glz::size_zmem(data);

// Pre-allocate buffer
std::string buffer;
buffer.resize(size);

// Write with no resize checks
size_t bytes_written = 0;
glz::write_zmem_unchecked(data, buffer, bytes_written);
```

### Pre-allocated Write (Recommended for Performance)

The `write_zmem_preallocated` function combines size computation and unchecked writing:

```cpp
SensorData data{12345, {1.0, 2.0, 3.0}};

// Automatic: compute size, allocate, write unchecked
std::string buffer;
auto ec = glz::write_zmem_preallocated(data, buffer);

// Or get the buffer directly
auto result = glz::write_zmem_preallocated(data);
if (result) {
   std::string buffer = std::move(*result);
}
```

This is the fastest way to serialize when you don't have a reusable buffer.

## File I/O

```cpp
#include "glaze/zmem.hpp"

struct Config {
   int version;
   std::string name;
   std::vector<double> values;
};

Config config{1, "settings", {1.0, 2.0, 3.0}};
std::string buffer;

// Write to file
auto ec = glz::write_file_zmem(config, "config.zmem", buffer);

// Read from file
Config loaded{};
ec = glz::read_file_zmem(loaded, "config.zmem", buffer);
```

## ZMEM Optional

ZMEM provides `glz::zmem::optional<T>` with a guaranteed memory layout for wire compatibility:

```cpp
// Layout: [present:1][padding:alignof(T)-1][value:sizeof(T)]
glz::zmem::optional<uint32_t> opt(42);  // sizeof == 8, alignof == 4
glz::zmem::optional<uint64_t> opt64;    // sizeof == 16, alignof == 8

// Serialize
std::string buffer;
glz::write_zmem(opt, buffer);

// std::optional also supported (converted to zmem::optional on wire)
std::optional<float> std_opt = 3.14f;
glz::write_zmem(std_opt, buffer);
```

## Maps

ZMEM supports `std::map` with sorted keys (required by the format):

```cpp
// Simple value type - no offset table needed
std::map<int32_t, float> simple_map = {{1, 1.0f}, {2, 2.0f}, {3, 3.0f}};
glz::write_zmem(simple_map, buffer);

// Complex value type - includes offset table for O(log N) binary search
std::map<int32_t, std::string> complex_map = {
   {1, "one"},
   {2, "two"},
   {3, "three"}
};
glz::write_zmem(complex_map, buffer);
```

`std::unordered_map` is also supported - keys are sorted before serialization.

## Glaze Metadata Support

ZMEM works with both automatic reflection and explicit `glz::meta` metadata:

```cpp
// Automatic reflection (C++20 aggregate)
struct AutoPoint {
   float x;
   float y;
};

// Explicit metadata
struct MetaPoint {
   float x_coord;
   float y_coord;
};

template <>
struct glz::meta<MetaPoint> {
   using T = MetaPoint;
   static constexpr auto value = object(
      "x", &T::x_coord,
      "y", &T::y_coord
   );
};

// Both work identically with ZMEM
AutoPoint ap{1.0f, 2.0f};
MetaPoint mp{1.0f, 2.0f};

glz::write_zmem(ap, buffer1);  // 8 bytes
glz::write_zmem(mp, buffer2);  // 8 bytes
```

## Wire Format Specification

### Simple Types

| Type | Wire Size | Format |
|------|-----------|--------|
| `bool` | 1 byte | 0x00 or 0x01 |
| `int8_t`/`uint8_t` | 1 byte | Raw byte |
| `int16_t`/`uint16_t` | 2 bytes | Little-endian |
| `int32_t`/`uint32_t`/`float` | 4 bytes | Little-endian |
| `int64_t`/`uint64_t`/`double` | 8 bytes | Little-endian |
| `T[N]` | `N * sizeof(T)` | Contiguous elements |
| Simple struct | `sizeof(T)` | Direct memory layout |

### Complex Types

| Type | Wire Format |
|------|-------------|
| `std::vector<T>` (simple T) | `[count:8][elements...]` |
| `std::vector<T>` (complex T) | `[count:8][offset_table:(count+1)*8][elements...]` |
| `std::string` | `[length:8][chars...]` |
| Complex struct | `[size:8][inline_section][variable_section]` |
| `std::map<K,V>` (simple V) | `[count:8][entries...]` |
| `std::map<K,V>` (complex V) | `[size:8][count:8][offset_table:(count+1)*8][entries...]` |

### Complex Struct Layout

For structs containing vectors or strings:

```
[size:8]                    # Total size of struct data (excluding this header)
[inline section]            # Fixed-size fields + references to variable data
  - Simple fields: raw bytes
  - Vector fields: [offset:8][count:8]
  - String fields: [offset:8][length:8]
[variable section]          # Variable-length data
  - Vector data
  - String data
```

Offsets are relative to byte 8 (after the size header).

## Error Handling

```cpp
struct Data {
   int value;
   std::string name;
};

Data d{42, "test"};
std::string buffer;

auto ec = glz::write_zmem(d, buffer);
if (ec) {
   // Handle error
   std::cerr << "Write error: " << glz::format_error(ec, buffer) << "\n";
}

Data result{};
ec = glz::read_zmem(result, buffer);
if (ec) {
   // Handle error (e.g., unexpected_end for truncated data)
   std::cerr << "Read error: " << glz::format_error(ec, buffer) << "\n";
}
```

## Performance Characteristics

| Operation | Simple Types | Complex Types |
|-----------|--------------|---------------|
| Write | Single `memcpy` | Header + inline pass + variable pass |
| Read | Single `memcpy` | Parse headers + extract fields |
| Size computation | `sizeof(T)` | Traverse structure |

For simple structs, ZMEM achieves the theoretical minimum serialization overhead: zero bytes beyond the data itself.

## API Reference

### Write Functions

| Function | Description |
|----------|-------------|
| `write_zmem(value, buffer)` | Write ZMEM to buffer, returns `error_ctx` |
| `write_zmem(value)` | Write ZMEM to new string, returns `expected<string, error_ctx>` |
| `write_zmem_preallocated(value, buffer)` | Compute size, allocate, write unchecked |
| `write_zmem_preallocated(value)` | Returns `expected<string, error_ctx>` |
| `write_zmem_unchecked(value, buffer, bytes_written)` | Write without resize checks (buffer must be pre-sized) |
| `write_file_zmem(value, filename, buffer)` | Write to file |

### Read Functions

| Function | Description |
|----------|-------------|
| `read_zmem(value, buffer)` | Read ZMEM from buffer, returns `error_ctx` |
| `read_zmem<T>(buffer)` | Read and return value, returns `expected<T, error_ctx>` |
| `read_file_zmem(value, filename, buffer)` | Read from file |

### Utility Functions

| Function | Description |
|----------|-------------|
| `size_zmem(value)` | Compute exact serialized size |
| `set_zmem<Opts>()` | Create options with ZMEM format |
