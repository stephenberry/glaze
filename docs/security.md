# Security Considerations

Glaze is designed with security in mind, particularly for parsing untrusted data from network sources. This document describes the security measures in place and best practices for handling potentially malicious input.

## Binary Format Security (BEVE, MessagePack, CBOR)

### Memory Bomb (DoS) Protection

Binary formats like BEVE encode length headers that indicate how many elements or bytes follow. A malicious actor could craft a message with a huge length header (e.g., claiming 1 billion elements) but minimal actual data. Without proper protection, this could cause:

- **Memory exhaustion**: The parser calls `resize()` with the malicious size, consuming all available memory
- **Process termination**: The system's OOM killer terminates the process
- **Denial of Service**: The server becomes unresponsive

#### How Glaze Protects Against This

Glaze validates length headers against the remaining buffer size **before** any memory allocation. If a length header claims more data than exists in the buffer, parsing fails immediately with `error_code::invalid_length`.

```cpp
// Example: Malicious buffer claiming 1 billion strings
std::vector<std::string> result;
auto ec = glz::read_beve(result, malicious_buffer);
// ec.ec == glz::error_code::invalid_length
// No memory was allocated for the claimed 1 billion strings
```

This protection applies to:

- **Strings**: Length must not exceed remaining buffer bytes
- **Typed arrays** (numeric, boolean, string, complex): Element count validated against buffer size
- **Generic arrays**: Element count validated against buffer size
- **Maps/Objects**: Entry count validated against buffer size

#### Protection Guarantees

| Data Type | Validation |
|-----------|------------|
| `std::string` | Length ≤ remaining buffer bytes |
| `std::vector<T>` (numeric) | Count × sizeof(T) ≤ remaining buffer bytes |
| `std::vector<bool>` | (Count + 7) / 8 ≤ remaining buffer bytes |
| `std::vector<std::string>` | Count ≤ remaining buffer bytes (minimum 1 byte per string header) |
| Generic arrays | Count ≤ remaining buffer bytes (minimum 1 byte per element) |

#### 32-bit System Considerations

On 32-bit systems, BEVE length headers using 8-byte encoding (for values > 2^30) are rejected with `invalid_length` since these values cannot be addressed in 32-bit memory space.

### User-Configurable Allocation Limits

For applications that need stricter control over memory allocation, Glaze provides compile-time options to limit the maximum size of strings and arrays during parsing.

#### Global Limits via Custom Options

Apply limits to all strings/arrays in a parse operation:

```cpp
// Define custom options with allocation limits
struct secure_opts : glz::opts
{
   uint32_t format = glz::BEVE;
   size_t max_string_length = 1024;    // Max 1KB per string
   size_t max_array_size = 10000;      // Max 10,000 elements per array
};

std::vector<std::string> data;
auto ec = glz::read<secure_opts{}>(data, buffer);
if (ec.ec == glz::error_code::invalid_length) {
    // A string or array exceeded the configured limit
}
```

#### Per-Field Limits via Wrapper

Apply limits to specific fields using `glz::max_length`:

```cpp
struct UserInput
{
   std::string username;       // Should be limited
   std::string bio;            // Can be longer
   std::vector<int> scores;    // Should be limited
};

template <>
struct glz::meta<UserInput>
{
   using T = UserInput;
   static constexpr auto value = object(
      "username", glz::max_length<&T::username, 64>,   // Max 64 chars
      "bio", &T::bio,                                   // No limit
      "scores", glz::max_length<&T::scores, 100>       // Max 100 elements
   );
};
```

These options work together with buffer-based validation:

1. **Buffer validation**: Rejects claims that exceed buffer capacity (always enabled)
2. **Allocation limits**: Rejects allocations that exceed user-defined limits (optional)

This allows you to accept legitimately large data while protecting against excessive memory usage.

#### Runtime Limits via Custom Context

For applications that need to set limits dynamically at runtime (e.g., based on user permissions or request type), inherit from `glz::context` and add constraint fields:

```cpp
struct secure_context : glz::context
{
   size_t max_string_length = 0;  // 0 = no limit
   size_t max_array_size = 0;
   size_t max_map_size = 0;
};

// Set limits dynamically based on user tier
secure_context ctx;
if (user.is_premium()) {
   ctx.max_array_size = 100000;
} else {
   ctx.max_array_size = 1000;
}

std::vector<Item> items;
auto ec = glz::read<glz::opts{.format = glz::BEVE}>(items, buffer, ctx);
if (ec.ec == glz::error_code::invalid_length) {
   // Limit exceeded
}
```

Runtime constraints use `if constexpr` to detect the presence of constraint fields, so there is **zero binary overhead** when using the base `glz::context`.

**When to use each approach:**

| Approach | Use Case |
|----------|----------|
| Compile-time (opts) | Fixed schema limits, per-field limits via `glz::meta` |
| Runtime (context) | Dynamic limits based on user, request type, or configuration |

Both mechanisms can be used together—either can reject a value that exceeds its limit.

### Best Practices for Network Applications

1. **Limit input buffer size**: Control the maximum message size your application accepts at the network layer, before passing data to Glaze.

```cpp
constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1 MB limit

void handle_message(const std::span<const std::byte> data) {
    if (data.size() > MAX_MESSAGE_SIZE) {
        // Reject oversized messages before parsing
        return;
    }

    MyStruct obj;
    auto ec = glz::read_beve(obj, data);
    if (ec) {
        // Handle parse error
    }
}
```

2. **Use appropriate container types**: Consider using fixed-size containers like `std::array` when the maximum size is known at compile time.

3. **Validate after parsing**: Use `glz::read_constraint` for business logic validation after successful parsing.

```cpp
template <>
struct glz::meta<UserInput> {
    using T = UserInput;
    static constexpr auto value = object(
        "username", glz::read_constraint<&T::username, [](const auto& s) {
            return s.size() <= 64;
        }>
    );
};
```

### Raw Pointer Allocation Safety

By default, Glaze refuses to allocate memory for null raw pointers during deserialization. This prevents a class of memory leaks where the parser would call `new` without any mechanism to ensure the memory is freed.

```cpp
struct example { int x, y, z; };

std::vector<example*> vec;
auto ec = glz::read_beve(vec, buffer);
// ec.ec == glz::error_code::invalid_nullable_read (safe default)
```

If your application requires raw pointer allocation, you can enable it with `allocate_raw_pointers = true`:

```cpp
struct alloc_opts : glz::opts {
   bool allocate_raw_pointers = true;
};

std::vector<example*> vec;
auto ec = glz::read<alloc_opts{}>(vec, buffer);
// Works, but you MUST manually delete allocated pointers
for (auto* p : vec) delete p;
```

#### Runtime Raw Pointer Allocation Control

For applications that need to toggle raw pointer allocation at runtime (e.g., based on data source trustworthiness), use a custom context:

```cpp
struct secure_context : glz::context
{
   bool allocate_raw_pointers = false;
};

template <typename T>
T my_deserializer(const std::vector<std::byte>& buffer, bool is_trusted_source)
{
   T obj{};
   secure_context ctx;
   ctx.allocate_raw_pointers = is_trusted_source;

   auto ec = glz::read<glz::opts{.format = glz::BEVE}>(obj, buffer, ctx);
   if (ec) {
      // Handle error
   }
   return obj;
}
```

This allows building unified deserializers that scale security based on the trust level of the source—for example, allowing raw pointer allocation for local save files while disabling it for network packets.

> [!WARNING]
> When enabling `allocate_raw_pointers`, your application is responsible for tracking and freeing all allocated memory. Consider using smart pointers (`std::unique_ptr`, `std::shared_ptr`) instead, which Glaze handles automatically and safely.

## Converting Untrusted Binary to JSON

`glz::beve_to_json`, `glz::cbor_to_json`, `glz::bson_to_json`, `glz::eetf_to_json`, and `glz::jsonb_to_json` turn a document you received into JSON text. Their input is someone else's bytes, so a few of the writer's defaults do not apply.

Every one of these formats allows a control character (0x00 to 0x1F) inside a string, and JSON does not. Most have a two-character escape and pass through normally, but the rest cannot be written without a Unicode escape. Rather than emit bytes that will not re-parse, the conversion fails with `error_code::invalid_control_character`.

```cpp
// a BEVE string holding a 0x01 byte
std::string json{};
auto ec = glz::beve_to_json(beve, json);
// ec.ec == glz::error_code::invalid_control_character
```

Set `escape_control_characters` to carry such bytes across instead. This is worth an explicit decision rather than a default: escaping produces valid JSON that every parser accepts, so a NUL in the source becomes a NUL in whatever reads the result, where an embedded null can truncate a C API or split a log line. Refusing gives the operator a signal; escaping gives fidelity for values that legitimately hold control bytes.

For Erlang binaries, which hold arbitrary bytes by definition, `binary_as_base64` is usually the better answer than escaping. It carries any payload without putting raw bytes in a JSON string at all.

`raw_string` and `unquoted` are ignored by these converters. Both would produce output that is not JSON regardless of what the caller asks for.

UTF-8 is **not** checked by the beve, cbor, bson, or eetf converters — that is the reader's job, and `validate_utf8` is on by default there. `glz::jsonb_to_json` is the exception and validates on convert.

## JSON Format Security

### Safe Parsing Defaults

- **No buffer overruns**: All parsing operations validate bounds before accessing data
- **No null pointer dereferences**: Null checks are performed where applicable
- **Integer overflow protection**: Numeric parsing handles overflow conditions

### Untrusted Input

When parsing JSON from untrusted sources:

1. **Recursion depth is bounded**: the readers, skippers, and format converters cap nesting at `max_recursive_depth_limit` (256 levels) and return `error_code::exceeded_max_recursive_depth`, so deeply nested input cannot overflow the stack. Every object and every array counts as a level, so a struct holding a vector of itself reaches the cap at 128 struct levels. Flatten your data structures if you legitimately need to exceed that.

2. **Limit string sizes**: Very large strings can consume excessive memory. Control this through input buffer size limits.

3. **Handle unknown keys**: Use `error_on_unknown_keys = false` if you want to ignore unexpected fields, or keep it `true` (default) to reject messages with unknown structure.

```cpp
// Reject messages with unknown keys (default behavior)
constexpr glz::opts strict_opts{.error_on_unknown_keys = true};

// Or allow unknown keys to be ignored
constexpr glz::opts lenient_opts{.error_on_unknown_keys = false};
```

## Error Handling

Always check return values when parsing untrusted data:

```cpp
auto ec = glz::read_beve(obj, buffer);
if (ec) {
    // Parsing failed - do not use obj
    std::cerr << glz::format_error(ec, buffer) << '\n';
    return;
}
// Safe to use obj
```

Error codes that may indicate malicious input:

| Error Code | Description |
|------------|-------------|
| `invalid_length` | Length exceeds allowed limit (buffer size or user-configured max) |
| `unexpected_end` | Buffer truncated during parsing |
| `syntax_error` | Invalid data structure or type mismatch |
| `parse_error` | Malformed data that doesn't match expected format |

## Reporting Security Issues

If you discover a security vulnerability in Glaze, please report it responsibly by opening an issue at [https://github.com/stephenberry/glaze/issues](https://github.com/stephenberry/glaze/issues).
