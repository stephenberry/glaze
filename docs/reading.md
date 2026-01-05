# Reading

Glaze provides a unified return type for all read operations that gives you both error information and the byte count consumed.

## The `error_ctx` Type

All read functions return `glz::error_ctx`:

```cpp
struct error_ctx {
    size_t count{};                          // Bytes consumed from input
    error_code ec{};                         // Error code (none on success)
    std::string_view custom_error_message{}; // Optional error details

    operator bool() const noexcept;          // Returns true when there IS an error
    bool operator==(error_code e) const noexcept;
};
```

### Key Properties

- **`count`**: Number of bytes consumed from the input, even on error (useful for debugging)
- **`ec`**: The error code (`error_code::none` on success)
- **`operator bool()`**: Returns `true` when there is an error (matches `std::error_code` semantics)

## Basic Usage

### Reading from Strings

```cpp
std::string json = R"({"name":"Alice","age":30})";
my_struct obj{};
auto ec = glz::read_json(obj, json);
if (ec) {
    std::cerr << glz::format_error(ec, json) << '\n';
    return;
}
// Success: ec.count contains bytes consumed
```

### Reading from String Views

```cpp
std::string_view json = get_json_data();
my_struct obj{};
auto ec = glz::read_json(obj, json);
if (!ec) {
    std::cout << "Consumed " << ec.count << " bytes\n";
}
```

### Reading Multiple Values

The `count` field enables reading multiple values from a single buffer:

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

### Reading with Return Value

For convenience, Glaze provides overloads that return the parsed value:

```cpp
// Returns expected<T, error_ctx>
auto result = glz::read_json<my_struct>(json);
if (result) {
    my_struct obj = *result;
} else {
    std::cerr << glz::format_error(result.error(), json) << '\n';
}
```

## Error Handling

### Checking for Errors

```cpp
auto ec = glz::read_json(obj, json);

// Method 1: Boolean check
if (ec) {
    // Error occurred
}

// Method 2: Check specific error
if (ec.ec == glz::error_code::parse_error) {
    // Handle parse error
}

// Method 3: Use negation for success
if (!ec) {
    // Success
}
```

### Formatting Errors

```cpp
auto ec = glz::read_json(obj, json);
if (ec) {
    // Get formatted error message with context
    std::string error_msg = glz::format_error(ec, json);
    std::cerr << error_msg << '\n';
}
```

## Format Support

The `error_ctx` type is used consistently across all deserialization formats:

```cpp
// JSON
auto ec = glz::read_json(obj, buffer);

// BEVE (Binary)
auto ec = glz::read_beve(obj, buffer);

// CBOR
auto ec = glz::read_cbor(obj, buffer);

// MessagePack
auto ec = glz::read_msgpack(obj, buffer);

// Generic with options
auto ec = glz::read<glz::opts{.format = glz::JSON}>(obj, buffer);
```

## Reading from Streams

For reading directly from files or other input streams with bounded memory, see [Streaming I/O](streaming.md).

## See Also

- [Writing](writing.md) - Writing to buffers and error handling
- [Lazy JSON](lazy-json.md) - On-demand parsing for selective field access
- [Streaming I/O](streaming.md) - Reading/writing with streams
- [Options](options.md) - Compile time options
