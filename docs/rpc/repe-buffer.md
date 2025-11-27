# REPE Buffer Handling

The `glaze/rpc/repe/buffer.hpp` header provides ASIO-independent functions for serializing and deserializing REPE messages to and from raw byte buffers. This enables use of REPE messages in contexts beyond direct socket I/O, such as:

- Plugin systems with C ABI boundaries
- Message queues and brokers
- In-memory RPC routing
- Testing and debugging
- WebSocket/HTTP transport layers

## Include

```cpp
#include "glaze/rpc/repe/buffer.hpp"
```

## Constants

### `repe_magic`

The REPE protocol magic bytes used to identify valid REPE messages.

```cpp
inline constexpr uint16_t repe_magic = 0x1507; // 5383
```

## Header Utilities

### `finalize_header`

After modifying a message's `query` or `body`, call `finalize_header` to update the header length fields:

```cpp
glz::repe::message msg{};
msg.query = "/api/endpoint";
msg.body = R"({"key": "value"})";

glz::repe::finalize_header(msg);
// msg.header.query_length, body_length, and length are now set correctly
```

## Error Handling

### `encode_error`

Encodes an error into a REPE message:

```cpp
glz::repe::message response{};

// Simple error (clears body)
glz::repe::encode_error(glz::error_code::parse_error, response);

// Error with message
glz::repe::encode_error(glz::error_code::invalid_header, response, "Custom error description");
```

### `decode_error`

Extracts a formatted error string from a REPE message:

```cpp
glz::repe::message msg{};
// ... receive message with error ...

if (msg.error()) {
    std::string error_str = glz::repe::decode_error(msg);
    // Returns: "REPE error: <error_code> | <error_message>"
}
```

### `decode_message`

Deserializes a message body into a C++ structure with error handling:

```cpp
struct Response {
    int value;
    std::string name;
};

glz::repe::message msg{};
// ... receive message ...

Response result{};
auto error = glz::repe::decode_message(result, msg);

if (error) {
    std::cerr << *error << '\n';  // Contains formatted error
} else {
    // result is populated
}
```

## Serialization

### `to_buffer`

Serializes a `repe::message` to wire-format bytes:

```cpp
glz::repe::message msg{};
msg.query = "/api/call";
msg.body = R"({"param": 42})";
glz::repe::finalize_header(msg);

// Returns a new string
std::string wire_data = glz::repe::to_buffer(msg);

// Or serialize into existing buffer (reuses memory)
std::string buffer;
glz::repe::to_buffer(msg, buffer);
```

## Deserialization

### `from_buffer`

Deserializes wire-format bytes into a `repe::message`:

```cpp
std::string wire_data = /* received bytes */;

glz::repe::message msg{};
auto ec = glz::repe::from_buffer(wire_data, msg);

if (ec == glz::error_code::none) {
    // msg.header, msg.query, and msg.body are populated
}
```

The function validates:
- Buffer contains at least a complete header
- Magic bytes match `repe_magic` (0x1507)
- Version is supported (currently version 1)
- Buffer contains complete query and body data

**Error codes:**
- `error_code::none` - Success
- `error_code::invalid_header` - Buffer too small or invalid magic bytes
- `error_code::version_mismatch` - Unsupported REPE version
- `error_code::invalid_body` - Buffer truncated (incomplete query/body)

Overloads:
```cpp
// From raw pointer + size
glz::error_code from_buffer(const char* data, size_t size, message& msg);

// From string_view
glz::error_code from_buffer(std::string_view data, message& msg);
```

## Header-Only Parsing

For routing scenarios where you need to inspect message metadata without fully deserializing:

### `parse_header`

Extracts only the header from raw bytes:

```cpp
std::string wire_data = /* received bytes */;

glz::repe::header hdr{};
auto ec = glz::repe::parse_header(wire_data, hdr);

if (ec == glz::error_code::none) {
    // Access hdr.id, hdr.query_length, hdr.body_length, etc.
}
```

### `extract_query`

Extracts the query string without full deserialization:

```cpp
std::string wire_data = /* received bytes */;

std::string_view query = glz::repe::extract_query(wire_data);

if (!query.empty()) {
    // Route based on query path
    auto* handler = router.find(query);
    handler->forward(wire_data);
}
```

Returns an empty `string_view` on error (invalid header, truncated data).

## Example: Message Router

```cpp
#include "glaze/rpc/repe/buffer.hpp"

void route_message(const char* data, size_t size) {
    // Extract query without full deserialization
    auto query = glz::repe::extract_query(data, size);

    if (query.empty()) {
        // Invalid message
        return;
    }

    // Route based on query prefix
    if (query.starts_with("/users/")) {
        users_service.forward(data, size);
    } else if (query.starts_with("/orders/")) {
        orders_service.forward(data, size);
    } else {
        // Unknown route
    }
}
```

## Example: Roundtrip Test

```cpp
#include "glaze/rpc/repe/buffer.hpp"

void test_roundtrip() {
    // Create message
    glz::repe::message original{};
    original.query = "/api/test";
    original.body = R"({"value": 42})";
    original.header.id = 12345;
    original.header.body_format = glz::repe::body_format::JSON;
    glz::repe::finalize_header(original);

    // Serialize
    std::string wire_data = glz::repe::to_buffer(original);

    // Deserialize
    glz::repe::message restored{};
    auto ec = glz::repe::from_buffer(wire_data, restored);

    assert(ec == glz::error_code::none);
    assert(restored.query == original.query);
    assert(restored.body == original.body);
    assert(restored.header.id == original.header.id);
}
```
