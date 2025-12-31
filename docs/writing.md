# Error Context and Buffer Handling

Glaze provides a unified return type for all read and write operations that gives you both error information and the byte count.

## The `error_ctx` Type

All read and write functions return `glz::error_ctx`:

```cpp
struct error_ctx {
    size_t count{};                          // Bytes processed (read or written)
    error_code ec{};                         // Error code (none on success)
    std::string_view custom_error_message{}; // Optional error details

    operator bool() const noexcept;          // Returns true when there IS an error
    bool operator==(error_code e) const noexcept;
};
```

### Key Properties

- **`count`**: Always contains the number of bytes processed, even on error (useful for debugging)
- **`ec`**: The error code (`error_code::none` on success)
- **`operator bool()`**: Returns `true` when there is an error (matches `std::error_code` semantics)

## Basic Usage

### Writing to Resizable Buffers

```cpp
my_struct obj{};
std::string buffer{};
auto ec = glz::write_json(obj, buffer);
if (ec) {
    // Error occurred
    std::cerr << glz::format_error(ec, buffer) << '\n';
    return;
}
// Success: for resizable buffers, buffer.size() == ec.count
std::cout << "Wrote " << ec.count << " bytes\n";
```

### Writing to Fixed-Size Buffers

Fixed-size buffers like `std::array` and `std::span` now have automatic bounds checking:

```cpp
std::array<char, 256> buffer;
auto ec = glz::write_json(obj, buffer);
if (ec) {
    if (ec.ec == glz::error_code::buffer_overflow) {
        // Buffer was too small
        std::cerr << "Buffer overflow after " << ec.count << " bytes\n";

        // Use a larger buffer or the string-returning overload:
        auto result = glz::write_json(obj);
        if (result) {
            size_t required_size = result->size();
        }
    }
    return;
}
// Success: ec.count contains bytes written
std::string_view json(buffer.data(), ec.count);
```

### Writing to `std::span`

`std::span` is ideal for writing to external memory (shared memory, DMA buffers, memory-mapped files):

```cpp
std::span<char> shared_memory(ptr, size);
auto ec = glz::write_json(obj, shared_memory);
if (ec) {
    if (ec.ec == glz::error_code::buffer_overflow) {
        return error::insufficient_shared_memory;
    }
    return error::serialization_failed;
}
// Notify consumers of bytes written
notify_consumers(ec.count);
```

### Writing to Raw Pointers

Raw `char*` pointers are trusted to have sufficient space (no bounds checking):

```cpp
char* dma_buffer = get_dma_buffer();  // Caller guarantees sufficient space
auto ec = glz::write_json(obj, dma_buffer);
if (!ec) {
    trigger_dma_transfer(ec.count);
}
```

### Writing to Streams (`basic_ostream_buffer`)

For writing directly to files or other output streams, use `glz::basic_ostream_buffer`:

```cpp
#include "glaze/core/ostream_buffer.hpp"

// Write to file with concrete type (enables devirtualization)
std::ofstream file("output.json");
glz::basic_ostream_buffer<std::ofstream> buffer(file);
auto ec = glz::write_json(obj, buffer);
if (ec || !file.good()) {
    // Handle error
}

// Write to any std::ostream (polymorphic)
std::ostringstream oss;
glz::ostream_buffer<> buffer2(oss);  // Alias for basic_ostream_buffer<std::ostream>
glz::write_json(obj, buffer2);
std::string result = oss.str();
```

The buffer accumulates serialized data internally and flushes incrementally to the stream during serialization. This enables bounded memory usage for arbitrarily large outputs.

**Template parameters:**

```cpp
// basic_ostream_buffer<Stream, Capacity>
// - Stream: Output stream type (must satisfy byte_output_stream concept)
// - Capacity: Initial buffer size in bytes (default 64KB)

// Concrete stream type for potential devirtualization
glz::basic_ostream_buffer<std::ofstream> buf1(file);

// Concrete type with custom capacity
glz::basic_ostream_buffer<std::ofstream, 4096> buf2(file);  // 4KB

// Polymorphic (works with any std::ostream derivative)
glz::ostream_buffer<> buf3(any_ostream);           // 64KB default
glz::ostream_buffer<4096> buf4(any_ostream);       // 4KB
glz::ostream_buffer<262144> buf5(any_ostream);     // 256KB
```

**The `byte_output_stream` concept:**

Only byte-oriented streams are supported. Wide character streams (`std::wostream`, `std::wofstream`) are rejected at compile time:

```cpp
// OK - byte streams
static_assert(glz::byte_output_stream<std::ostream>);
static_assert(glz::byte_output_stream<std::ofstream>);
static_assert(glz::byte_output_stream<std::ostringstream>);

// Compile error - wide streams not supported (JSON is UTF-8)
// glz::basic_ostream_buffer<std::wostream> bad(wstream);  // Error!
```

**Checking stream state:**

```cpp
glz::basic_ostream_buffer<std::ofstream> buffer(file);
auto ec = glz::write_json(obj, buffer);

// Check if stream is still healthy after write
if (!ec && buffer.good()) {
    // Success
}
```

### Reading from Streams (`basic_istream_buffer`)

For reading directly from files or other input streams, use `glz::basic_istream_buffer`:

```cpp
#include "glaze/core/istream_buffer.hpp"

// Read from file with concrete type (enables devirtualization)
std::ifstream file("input.json");
glz::basic_istream_buffer<std::ifstream> buffer(file);
my_struct obj;
auto ec = glz::read_json(obj, buffer);  // Convenience overload for streaming
if (ec) {
    // Handle error
}

// Read from any std::istream (polymorphic)
std::istringstream iss(json_string);
glz::istream_buffer<> buffer2(iss);  // Alias for basic_istream_buffer<std::istream>
glz::read_json(obj, buffer2);

// Or use read_streaming directly with custom options
glz::read_streaming<glz::opts{}>(obj, buffer2);
```

The buffer reads chunks from the stream and presents them as contiguous memory to the parser. When more data is needed, it refills automatically at safe points (between array elements, object key-value pairs, etc.).

**Key feature: Incremental streaming** - The buffer can be much smaller than the JSON input. This enables parsing arbitrarily large files (gigabytes) with bounded memory (e.g., 64KB buffer).

**Template parameters:**

```cpp
// basic_istream_buffer<Stream, Capacity>
// - Stream: Input stream type (must satisfy byte_input_stream concept)
// - Capacity: Buffer size in bytes (default 64KB)

// Concrete stream type for potential devirtualization
glz::basic_istream_buffer<std::ifstream> buf1(file);

// Concrete type with custom capacity
glz::basic_istream_buffer<std::ifstream, 4096> buf2(file);  // 4KB

// Polymorphic (works with any std::istream derivative)
glz::istream_buffer<> buf3(any_istream);           // 64KB default
glz::istream_buffer<4096> buf4(any_istream);       // 4KB
```

**The `byte_input_stream` concept:**

Only byte-oriented streams are supported. Wide character streams (`std::wistream`, `std::wifstream`) are rejected at compile time:

```cpp
// OK - byte streams
static_assert(glz::byte_input_stream<std::istream>);
static_assert(glz::byte_input_stream<std::ifstream>);
static_assert(glz::byte_input_stream<std::istringstream>);

// Compile error - wide streams not supported (JSON is UTF-8)
// glz::basic_istream_buffer<std::wistream> bad(wstream);  // Error!
```

### Streaming JSON/NDJSON with `json_stream_reader`

For processing streams of JSON objects (NDJSON or multiple JSON values), use `json_stream_reader`:

```cpp
#include "glaze/json/json_stream.hpp"

struct Event {
    int id;
    std::string type;
};

// Manual iteration
std::ifstream file("events.ndjson");
glz::json_stream_reader<Event> reader(file);
Event event;
while (!reader.read_next(event)) {
    process(event);
}

// Range-based for loop
std::ifstream file2("events.ndjson");
for (auto&& event : glz::json_stream_reader<Event>(file2)) {
    process(event);
}

// Using the ndjson_stream alias
for (auto&& event : glz::ndjson_stream<Event>(file)) {
    process(event);
}
```

**Convenience function for reading all values:**

```cpp
std::ifstream file("events.ndjson");
std::vector<Event> events;
auto ec = glz::read_json_stream(events, file);
```

**Key features:**

- Processes one complete value at a time (bounded memory)
- Supports NDJSON (newline-delimited) and consecutive JSON values
- Iterator interface for range-based for loops
- Automatic whitespace/newline skipping between values

### Streaming Format Support

| Format | Output Streaming | Input Streaming |
|--------|-----------------|-----------------|
| JSON   | ✅ `ostream_buffer` | ✅ `istream_buffer` |
| BEVE   | ✅ `ostream_buffer` | ✅ `istream_buffer` |
| NDJSON | ✅ `ostream_buffer` | ✅ `json_stream_reader` |

## Buffer Overflow Handling

When writing to a fixed-size buffer that's too small, Glaze returns `error_code::buffer_overflow`:

```cpp
std::array<char, 64> small_buffer;
auto ec = glz::write_json(large_object, small_buffer);
if (ec.ec == glz::error_code::buffer_overflow) {
    // ec.count contains bytes written before overflow
    // This is a LOWER BOUND on required size (not total required)

    // The partial content is NOT valid JSON - do not parse or transmit
    // Useful only for debugging:
    std::string_view partial(small_buffer.data(), ec.count);
    log_debug("Overflow after {} bytes: {}", ec.count, partial);

    // To get actual required size, use the string-returning overload:
    auto full = glz::write_json(large_object);
    if (full) {
        size_t required = full->size();
    }
}
```

### Important Notes on Buffer Overflow

1. **Content validity**: The first `ec.count` bytes contain valid serialized output
2. **Partial data is NOT valid**: The content is truncated mid-serialization and cannot be parsed
3. **`ec.count` is a lower bound**: It's the bytes written before failure, not total required size
4. **Content beyond `ec.count`**: Unspecified (may be uninitialized or previous data)

## Performance Optimization

### Skipping Bounds Checking

For performance-critical paths where you've pre-validated buffer size:

```cpp
struct fast_opts : glz::opts {
    bool assume_sufficient_buffer = true;
};

std::array<char, 8192> large_buffer;  // Known to be sufficient
auto ec = glz::write<fast_opts{}>(obj, large_buffer);
// No bounds checking overhead
```

> **Warning**: Only use `assume_sufficient_buffer` when you've verified the buffer is large enough. Buffer overflow with this option leads to undefined behavior.

## Format Support

The `error_ctx` type is used consistently across all serialization formats:

```cpp
// JSON
auto ec = glz::write_json(obj, buffer);

// BEVE (Binary)
auto ec = glz::write_beve(obj, buffer);

// CBOR
auto ec = glz::write_cbor(obj, buffer);

// MessagePack
auto ec = glz::write_msgpack(obj, buffer);

// Generic with options
auto ec = glz::write<glz::opts{.format = glz::JSON}>(obj, buffer);
```

## Extending Buffer Support

Glaze uses a traits system that can be specialized for custom buffer types:

```cpp
namespace glz {
    template <size_t N>
    struct buffer_traits<my_lib::ring_buffer<N>> {
        static constexpr bool is_resizable = false;
        static constexpr bool has_bounded_capacity = true;

        static size_t capacity(const my_lib::ring_buffer<N>& b) noexcept {
            return b.available_write_space();
        }

        static bool ensure_capacity(my_lib::ring_buffer<N>& b, size_t needed) noexcept {
            return b.available_write_space() >= needed;
        }

        static void finalize(my_lib::ring_buffer<N>& b, size_t written) noexcept {
            b.commit(written);
        }
    };
}

// Now works seamlessly
my_lib::ring_buffer<4096> ring;
auto ec = glz::write_json(obj, ring);
```

## Read and Write Return Types

Glaze uses `glz::error_ctx` for both read and write operations:

| Operation | Byte Position Field | Purpose |
|-----------|---------------------|---------|
| **Read** (`read_json`, etc.) | `count` | Bytes consumed from input |
| **Write** (`write_json`, etc.) | `count` | Bytes written to output |

### Reading Bytes Consumed

```cpp
std::string input = R"({"x":1}{"y":2})";  // Two JSON objects
my_struct obj1{}, obj2{};

auto ec = glz::read_json(obj1, input);
if (!ec) {
    size_t consumed = ec.count;  // Bytes consumed by first read

    // Read second object from remaining buffer
    ec = glz::read_json(obj2, std::string_view(input).substr(consumed));
}
```

### Writing Bytes Produced

```cpp
std::array<char, 1024> buffer;
auto ec = glz::write_json(obj, buffer);
if (!ec) {
    size_t produced = ec.count;  // Bytes written
    std::string_view json(buffer.data(), produced);
}
```

## See Also

- [Options](options.md) - Compile time options including `assume_sufficient_buffer`
- [Binary Format (BEVE)](binary.md) - Binary serialization
- [JSON](json.md) - JSON serialization
