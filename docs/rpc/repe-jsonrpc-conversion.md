# REPE to JSON-RPC Conversion

Utilities to convert between REPE messages with JSON bodies and JSON-RPC 2.0 messages, enabling interoperability between the REPE and JSON-RPC protocols.

## Quick Start

```cpp
#include "glaze/rpc/repe/repe_to_jsonrpc.hpp"

// REPE → JSON-RPC
repe::message msg{};
msg.query = "/add";
msg.body = R"([1,2,3])";
msg.header.id = 42;
msg.header.body_format = repe::body_format::JSON;

std::string jsonrpc = repe::to_jsonrpc_request(msg);
// {"jsonrpc":"2.0","method":"add","params":[1,2,3],"id":42}
```

## API Reference

### Converting REPE to JSON-RPC

#### Request Conversion

**Function:** `std::string to_jsonrpc_request(const message& msg)`

Converts a REPE request message to a JSON-RPC request string.

```cpp
#include "glaze/rpc/repe/repe_to_jsonrpc.hpp"

repe::message msg{};
msg.query = "/add";                          // Method name (with or without leading slash)
msg.body = R"([1,2,3])";                     // JSON parameters
msg.header.id = 42;                          // Request ID
msg.header.body_format = repe::body_format::JSON;  // Must be JSON format
msg.header.notify = false;                   // false for request, true for notification

std::string jsonrpc_request = repe::to_jsonrpc_request(msg);
// Result: {"jsonrpc":"2.0","method":"add","params":[1,2,3],"id":42}
```

#### Response Conversion

**Function:** `std::string to_jsonrpc_response(const message& msg)`

Converts a REPE response message to a JSON-RPC response string.

```cpp
// Success response
repe::message success_msg{};
success_msg.body = R"({"result":"success"})";
success_msg.header.id = 42;
success_msg.header.body_format = repe::body_format::JSON;
success_msg.header.ec = glz::error_code::none;

std::string jsonrpc_response = repe::to_jsonrpc_response(success_msg);
// Result: {"jsonrpc":"2.0","result":{"result":"success"},"id":42}

// Error response
repe::message error_msg{};
error_msg.body = "Method not found";
error_msg.header.id = 42;
error_msg.header.body_format = repe::body_format::UTF8;
error_msg.header.ec = glz::error_code::method_not_found;

std::string jsonrpc_error = repe::to_jsonrpc_response(error_msg);
// Result: {"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Method not found"},"id":42}
```

### Converting JSON-RPC to REPE

#### Request Conversion

**Function:** `expected<message, std::string> from_jsonrpc_request(std::string_view json_request)`

Converts a JSON-RPC request string to a REPE message.

```cpp
std::string jsonrpc = R"({"jsonrpc":"2.0","method":"add","params":[1,2,3],"id":42})";

auto msg = repe::from_jsonrpc_request(jsonrpc);
if (msg.has_value()) {
    // msg->query == "/add"
    // msg->body == "[1,2,3]"
    // msg->header.id == 42
    // msg->header.body_format == repe::body_format::JSON
}
```

#### Response Conversion

**Function:** `expected<message, std::string> from_jsonrpc_response(std::string_view json_response)`

Converts a JSON-RPC response string to a REPE message.

```cpp
// Success response
std::string jsonrpc_success = R"({"jsonrpc":"2.0","result":{"value":42},"id":10})";

auto success = repe::from_jsonrpc_response(jsonrpc_success);
if (success.has_value()) {
    // success->header.id == 10
    // success->header.ec == glz::error_code::none
    // success->body contains {"value":42}
}

// Error response
std::string jsonrpc_error = R"({"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Details"},"id":42})";

auto error = repe::from_jsonrpc_response(jsonrpc_error);
if (error.has_value()) {
    // error->header.id == 42
    // error->header.ec == glz::error_code::method_not_found
    // error->body == "Details"
}
```

### Error Code Mapping

| REPE Error | JSON-RPC Error | Code |
|------------|----------------|------|
| `error_code::none` | `error_e::no_error` | 0 |
| `error_code::parse_error` | `error_e::parse_error` | -32700 |
| `error_code::syntax_error` | `error_e::parse_error` | -32700 |
| `error_code::invalid_header` | `error_e::invalid_request` | -32600 |
| `error_code::version_mismatch` | `error_e::invalid_request` | -32600 |
| `error_code::method_not_found` | `error_e::method_not_found` | -32601 |
| Other errors | `error_e::internal` | -32603 |

### Notifications

REPE notifications (messages with `notify = true`) are converted to JSON-RPC notifications (requests with `id = null`):

```cpp
repe::message notification{};
notification.query = "/notify";
notification.body = R"({"message":"hello"})";
notification.header.notify = true;

std::string jsonrpc = repe::to_jsonrpc_request(notification);
// Result: {"jsonrpc":"2.0","method":"notify","params":{"message":"hello"},"id":null}
```

### ID Handling

REPE uses `uint64_t` for IDs, while JSON-RPC supports strings, numbers, or null:

- Numeric IDs are directly mapped between protocols
- Numeric strings (e.g., `"123"`) are parsed as numbers
- Non-numeric strings are hashed to create a REPE ID

```cpp
// Numeric string ID
std::string jsonrpc = R"({"jsonrpc":"2.0","method":"test","params":[],"id":"999"})";
auto msg = repe::from_jsonrpc_request(jsonrpc);
// msg->header.id == 999

// Non-numeric string ID
std::string jsonrpc2 = R"({"jsonrpc":"2.0","method":"test","params":[],"id":"test-123"})";
auto msg2 = repe::from_jsonrpc_request(jsonrpc2);
// msg2->header.id == hash("test-123")
```

### Requirements

- REPE message body must be in JSON format for conversion to JSON-RPC requests
- REPE message body should be in JSON or UTF8 format for conversion to JSON-RPC responses
- The query field in REPE requests is treated as the method name (leading slash is optional)

### Bridging Protocols Example

```cpp
#include "glaze/rpc/repe/repe_to_jsonrpc.hpp"

// Receive JSON-RPC request from client
std::string jsonrpc_request = R"({"jsonrpc":"2.0","method":"add","params":[1,2,3],"id":42})";

// Convert to REPE
auto repe_request = repe::from_jsonrpc_request(jsonrpc_request);

if (repe_request.has_value()) {
    // Process with REPE server
    repe::message repe_response = process(repe_request.value());

    // Convert response back to JSON-RPC
    std::string jsonrpc_response = repe::to_jsonrpc_response(repe_response);
}
```
