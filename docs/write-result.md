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
// Success: buffer.size() == ec.count
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
