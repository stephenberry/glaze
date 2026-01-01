# Writing

Glaze provides a unified return type for all write operations that gives you both error information and the byte count written.

## The `error_ctx` Type

All write functions return `glz::error_ctx`:

```cpp
struct error_ctx {
    size_t count{};                          // Bytes written to output
    error_code ec{};                         // Error code (none on success)
    std::string_view custom_error_message{}; // Optional error details

    operator bool() const noexcept;          // Returns true when there IS an error
    bool operator==(error_code e) const noexcept;
};
```

### Key Properties

- **`count`**: Number of bytes written, even on error (useful for debugging)
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

Fixed-size buffers like `std::array` and `std::span` have automatic bounds checking:

```cpp
std::array<char, 512> buffer;  // Minimum 512 bytes recommended
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

### Writing to Streams

For writing directly to files or output streams with bounded memory, see [Streaming I/O](streaming.md).

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

### Minimum Buffer Size Requirement

Bounded buffers (`std::array`, `std::span`, etc.) must be **at least 512 bytes** for reliable serialization. This requirement exists because Glaze uses internal padding for efficient writes. Buffers smaller than 512 bytes may return `buffer_overflow` even when the serialized output would fit.

```cpp
// Good: 512+ bytes
std::array<char, 512> buffer;
auto ec = glz::write_json(obj, buffer);

// May fail unexpectedly for small outputs
std::array<char, 64> small_buffer;  // Not recommended
```

For very small payloads where memory is extremely constrained, use a resizable buffer (`std::string`) and copy the result, or use `raw char*` if you can guarantee sufficient space.

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

## See Also

- [Reading](reading.md) - Reading from buffers
- [Streaming I/O](streaming.md) - Reading/writing with streams
- [Options](options.md) - Compile time options including `assume_sufficient_buffer`
- [Binary Format (BEVE)](binary.md) - Binary serialization
