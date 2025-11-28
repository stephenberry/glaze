# JSON-RPC 2.0 Registry

Glaze provides native JSON-RPC 2.0 protocol support through the `glz::registry` template. This allows you to expose C++ objects and functions via JSON-RPC 2.0 with automatic serialization and method dispatch.

- [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification)

## Choosing Between JSON-RPC Implementations

Glaze offers two JSON-RPC 2.0 implementations. Choose based on your needs:

| Feature | Registry (this page) | [Client/Server](json-rpc.md) |
|---------|---------------------|------------------------------|
| **Method registration** | Automatic via reflection | Explicit compile-time declaration |
| **Client support** | Server only | Client and server (client works with either server) |
| **Type safety** | Runtime (JSON parsing) | Compile-time enforced |
| **Setup complexity** | Minimal (`server.on(obj)`) | Requires method declarations |
| **Variable access** | Read/write member variables | Methods only |
| **Nested objects** | Automatic path traversal | Manual |
| **Use case** | Expose existing C++ objects | Define strict API contracts |

### When to Use the Registry

Use the JSON-RPC registry when you want to:

- **Expose existing objects** - Turn any C++ struct into a JSON-RPC API with one line
- **Access member variables** - Read and write fields directly, not just call methods
- **Minimize boilerplate** - No need to declare method signatures separately
- **Navigate nested structures** - Access deep members via path-style method names

### When to Use the Client/Server

Use the [compile-time typed approach](json-rpc.md) when you need:

- **A JSON-RPC client** - Glaze's only client implementation (works with either server approach)
- **Compile-time type checking** - Method signatures enforced at compile time
- **Callback-based responses** - Async client with request tracking

## Overview

The JSON-RPC registry is similar to the [REPE registry](repe-rpc.md) but uses the JSON-RPC 2.0 protocol format. It provides:

- Automatic method registration from C++ objects
- Full JSON-RPC 2.0 compliance (requests, responses, notifications, batches)
- Support for all ID types (string, integer, null)
- Standard error codes and error responses
- Nested object traversal via method names

## Basic Usage

```cpp
#include "glaze/rpc/registry.hpp"

struct my_api
{
   int counter = 0;
   std::string greet() { return "Hello, World!"; }
   int add(int value) { counter += value; return counter; }
   void reset() { counter = 0; }
};

int main()
{
   // Create a JSON-RPC registry
   glz::registry<glz::opts{}, glz::JSONRPC> server{};

   my_api api{};
   server.on(api);

   // Handle requests
   std::string response;

   // Call a function with no parameters
   response = server.call(R"({"jsonrpc":"2.0","method":"greet","id":1})");
   // Returns: {"jsonrpc":"2.0","result":"Hello, World!","id":1}

   // Read a variable
   response = server.call(R"({"jsonrpc":"2.0","method":"counter","id":2})");
   // Returns: {"jsonrpc":"2.0","result":0,"id":2}

   // Call a function with parameters
   response = server.call(R"({"jsonrpc":"2.0","method":"add","params":10,"id":3})");
   // Returns: {"jsonrpc":"2.0","result":10,"id":3}

   // Write to a variable
   response = server.call(R"({"jsonrpc":"2.0","method":"counter","params":100,"id":4})");
   // Returns: {"jsonrpc":"2.0","result":null,"id":4}

   // Read updated value
   response = server.call(R"({"jsonrpc":"2.0","method":"counter","id":5})");
   // Returns: {"jsonrpc":"2.0","result":100,"id":5}
}
```

## Method Names

JSON-RPC method names map to registry paths. The registry uses JSON Pointer-style paths internally, but the leading `/` is optional in method names:

| Method Name | Registry Path |
|-------------|---------------|
| `"counter"` | `/counter` |
| `"greet"` | `/greet` |
| `"nested/value"` | `/nested/value` |
| `""` (empty) | `` (root - returns entire object) |

### Nested Objects

```cpp
struct inner_t
{
   int value = 42;
};

struct outer_t
{
   inner_t inner{};
   std::string name = "test";
};

outer_t obj{};
server.on(obj);

// Access nested members
server.call(R"({"jsonrpc":"2.0","method":"inner/value","id":1})");
// Returns: {"jsonrpc":"2.0","result":42,"id":1}

// Access root object
server.call(R"({"jsonrpc":"2.0","method":"","id":2})");
// Returns: {"jsonrpc":"2.0","result":{"inner":{"value":42},"name":"test"},"id":2}
```

## Notifications

JSON-RPC notifications are requests without an `id` field (or with `id: null`). The server processes these but does not return a response.

```cpp
// Notification - no response returned
std::string response = server.call(R"({"jsonrpc":"2.0","method":"reset","id":null})");
// response is empty string

// The function was still executed
response = server.call(R"({"jsonrpc":"2.0","method":"counter","id":1})");
// Returns: {"jsonrpc":"2.0","result":0,"id":1}
```

## Batch Requests

Multiple requests can be sent in a single JSON array:

```cpp
std::string response = server.call(R"([
   {"jsonrpc":"2.0","method":"greet","id":1},
   {"jsonrpc":"2.0","method":"counter","id":2},
   {"jsonrpc":"2.0","method":"add","params":5,"id":3}
])");
// Returns array of responses:
// [{"jsonrpc":"2.0","result":"Hello, World!","id":1},
//  {"jsonrpc":"2.0","result":0,"id":2},
//  {"jsonrpc":"2.0","result":5,"id":3}]
```

Notifications in a batch are processed but excluded from the response array. If all requests in a batch are notifications, an empty string is returned.

## Error Handling

The registry returns standard JSON-RPC 2.0 error codes:

| Code | Message | Description |
|------|---------|-------------|
| -32700 | Parse error | Invalid JSON was received |
| -32600 | Invalid Request | The JSON is not a valid Request object |
| -32601 | Method not found | The method does not exist |
| -32602 | Invalid params | Invalid method parameter(s) |
| -32603 | Internal error | Internal JSON-RPC error |

### Examples

```cpp
// Parse error
server.call(R"({invalid json)");
// Returns: {"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error",...},"id":null}

// Method not found
server.call(R"({"jsonrpc":"2.0","method":"nonexistent","id":1})");
// Returns: {"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"nonexistent"},"id":1}

// Invalid params (wrong type)
server.call(R"({"jsonrpc":"2.0","method":"counter","params":"not_an_int","id":1})");
// Returns: {"jsonrpc":"2.0","error":{"code":-32602,"message":"Invalid params",...},"id":1}

// Invalid version
server.call(R"({"jsonrpc":"1.0","method":"greet","id":1})");
// Returns: {"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid Request",...},"id":1}
```

## ID Types

JSON-RPC 2.0 supports string, integer, and null IDs. All are preserved in responses:

```cpp
// String ID
server.call(R"({"jsonrpc":"2.0","method":"greet","id":"my-request-123"})");
// Returns: {"jsonrpc":"2.0","result":"Hello, World!","id":"my-request-123"}

// Integer ID
server.call(R"({"jsonrpc":"2.0","method":"greet","id":42})");
// Returns: {"jsonrpc":"2.0","result":"Hello, World!","id":42}

// Null ID (notification)
server.call(R"({"jsonrpc":"2.0","method":"greet","id":null})");
// Returns: "" (empty - no response for notifications)
```

## Member Functions

Member functions are automatically registered and callable:

```cpp
struct calculator_t
{
   int value = 0;

   int get_value() { return value; }
   void set_value(int v) { value = v; }
   int add(int x) { return value += x; }
};

calculator_t calc{};
server.on(calc);

// Call member function with no params
server.call(R"({"jsonrpc":"2.0","method":"get_value","id":1})");
// Returns: {"jsonrpc":"2.0","result":0,"id":1}

// Call member function with params
server.call(R"({"jsonrpc":"2.0","method":"set_value","params":100,"id":2})");
// Returns: {"jsonrpc":"2.0","result":null,"id":2}

server.call(R"({"jsonrpc":"2.0","method":"add","params":50,"id":3})");
// Returns: {"jsonrpc":"2.0","result":150,"id":3}
```

## Using with `glz::merge`

Multiple objects can be merged into a single registry:

```cpp
struct sensors_t {
   double temperature = 25.0;
   double humidity = 60.0;
};

struct settings_t {
   int refresh_rate = 100;
   std::string mode = "auto";
};

sensors_t sensors{};
settings_t settings{};

auto merged = glz::merge{sensors, settings};

glz::registry<glz::opts{}, glz::JSONRPC> server{};
server.on(merged);

// Access merged fields
server.call(R"({"jsonrpc":"2.0","method":"temperature","id":1})");
// Returns: {"jsonrpc":"2.0","result":25.0,"id":1}

server.call(R"({"jsonrpc":"2.0","method":"refresh_rate","id":2})");
// Returns: {"jsonrpc":"2.0","result":100,"id":2}

// Root returns combined view
server.call(R"({"jsonrpc":"2.0","method":"","id":3})");
// Returns: {"jsonrpc":"2.0","result":{"temperature":25.0,"humidity":60.0,"refresh_rate":100,"mode":"auto"},"id":3}
```

> Note: Writing to the merged root endpoint (`""`) returns an error. Individual field paths remain writable.

## Comparison with Other Protocols

Glaze's registry supports multiple protocols through template parameters:

| Protocol | Template Parameter | Call Signature |
|----------|-------------------|----------------|
| REPE | `glz::REPE` (default) | `call(repe::message&, repe::message&)` |
| REST | `glz::REST` | HTTP router integration |
| JSON-RPC | `glz::JSONRPC` | `call(std::string_view) -> std::string` |

```cpp
// REPE registry (default)
glz::registry<> repe_server{};

// REST registry
glz::registry<glz::opts{}, glz::REST> rest_server{};

// JSON-RPC registry
glz::registry<glz::opts{}, glz::JSONRPC> jsonrpc_server{};
```

## Thread Safety

Like other registry implementations, the JSON-RPC registry does not acquire locks for reading/writing. Thread safety must be managed by the user. Consider using thread-safe types like `std::atomic<T>` or `glz::async_string` for concurrent access.

## See Also

- [JSON-RPC 2.0 Client/Server](json-rpc.md) - Compile-time typed JSON-RPC with client and server support
- [REPE RPC](repe-rpc.md) - Binary RPC protocol with similar registry API
- [REPE/JSON-RPC Conversion](repe-jsonrpc-conversion.md) - Protocol bridging utilities
