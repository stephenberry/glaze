# Glaze HTTP/REST Support

Glaze provides HTTP server and client capabilities with modern C++ features, including automatic REST API generation, WebSocket support, and SSL/TLS encryption.

## Overview

The Glaze HTTP library offers:

- **High-performance HTTP server** with async I/O using ASIO
- **Automatic REST API generation** from C++ classes using reflection
- **Advanced routing** with parameters, wildcards, and constraints
- **Middleware support** for cross-cutting concerns
- **CORS support** with flexible configuration
- **WebSocket support** for real-time communication
- **SSL/TLS support** for secure connections
- **HTTP client** for making requests

## Quick Start

### Basic HTTP Server

```cpp
#include "glaze/net/http_server.hpp"
#include <iostream>

int main() {
    glz::http_server server;
    
    server.get("/hello", [](const glz::request& /*req*/, glz::response& res) {
        res.body("Hello, World!");
    });
    
    // Note: start() is non-blocking; block the main thread until shutdown
    server.bind("127.0.0.1", 8080)
          .with_signals(); // enable Ctrl+C (SIGINT) handling

    std::cout << "Server running on http://127.0.0.1:8080\n";
    std::cout << "Press Ctrl+C to stop\n";

    server.start();
    server.wait_for_signal();
    return 0;
}
```

### REST API from C++ Class

```cpp
#include "glaze/rpc/registry.hpp"
#include "glaze/net/http_server.hpp"

struct User {
    int id{};
    std::string name{};
};

struct UserService {
    std::vector<User> getAllUsers() { /* ... */ }
    User getUserById(int id) { /* ... */ }
    User createUser(const User& user) { /* ... */ }
};

template <>
struct glz::meta<UserService> {
    using T = UserService;
    static constexpr auto value = glz::object(
        &T::getAllUsers,
        &T::getUserById,
        &T::createUser
    );
};

int main() {
    glz::http_server server;
    UserService userService;
    
    // Auto-generate REST endpoints
    glz::registry<glz::opts{}, glz::REST> registry;
    registry.on(userService);
    
    server.mount("/api", registry.endpoints);
    server.bind(8080).with_signals();
    server.start();
    server.wait_for_signal();
    return 0;
}
```

`registry.on(service)` requires reflection metadata (`glz::meta<T>` or a reflectable aggregate).

## Core Components

| Component | Description | Documentation |
|-----------|-------------|---------------|
| **HTTP Server** | ASIO-based async HTTP server | [Server Guide](http-server.md) |
| **HTTP Router** | Advanced routing with parameters | [Routing Guide](http-router.md) |
| **REST Registry** | Auto-generate APIs from C++ classes | [Registry Guide](rest-registry.md) |
| **HTTP Client** | Client for making HTTP requests | [Client Guide](http-client.md) |
| **Advanced Networking** | CORS, WebSockets, SSL | [Advanced Guide](advanced-networking.md) |

## Request/Response Model

### Request Object

```cpp
struct request {
    http_method method;                                    // GET, POST, etc.
    std::string target;                                    // "/users/123"
    std::unordered_map<std::string, std::string> params;   // Route parameters
    glz::http_headers headers;                             // HTTP headers
    std::string body;                                      // Request body
    std::string remote_ip;                                 // Client IP
    uint16_t remote_port;                                  // Client port
};
```

### Response Builder

```cpp
struct response {
    int status_code = 200;
    glz::http_headers response_headers;
    std::string response_body;
    
    // Fluent interface
    response& status(int code);
    response& header(std::string_view name, std::string_view value);     // replaces any existing field
    response& add_header(std::string_view name, std::string_view value); // appends, for Set-Cookie and friends
    response& body(std::string_view content);
    response& content_type(std::string_view type);
  
		template <class T = glz::generic>
	    response& json(T&& value); // Auto-serialize any type T to JSON
  
  	template <auto Opts, class T>
    response& body(T&& value) // Auto-serialize any type T to any format in Opts
};
```

`glz::generic` is the dynamic JSON-compatible type (formerly `glz::json_t`) and remains the default payload for
helpers like `response::json` when you need flexible data that can be serialized to JSON or equivalent formats.

### Field Name Casing

Field names keep the case they were written with. `request::headers` holds the names exactly as the client sent them,
and `response::header` serializes fields exactly in the case the handler passed them in.

Lookups are case-insensitive: `find`, `contains`, `first_value`, `values`, `count`, `contains_token`, `set` and `erase`
all match regardless of case, so `req.headers.find("content-type")` finds a field the client sent as `Content-Type`.

Code that iterates the container and compares names on its own has to do the same:

```cpp
for (const auto& field : req.headers) {
    if (field.name == "content-type") {                // misses "Content-Type"
    }
    if (glz::striequal(field.name, "content-type")) {  // matches any casing
    }
}
```

Both forms compile, so the first one fails silently.

### The `glz::http_headers` Container

`request::headers` and `response::response_headers` are both `glz::http_headers`: `{name, value}` fields held in arrival order, where a name is allowed to repeat. Names match case-insensitively throughout, as described above.

Reading:

| Call | Result |
| --- | --- |
| `contains(name)` | whether any field carries that name |
| `count(name)` | how many fields carry it |
| `first_value(name)` | `std::optional<std::string_view>` of the first match |
| `values(name)` | range over the value of every match |
| `fields(name)` | range over every matching `http_header`, name included |
| `contains_token(name, token)` | whether any match lists `token` in its comma-separated value |
| `find(name)` | iterator to the first match, or `end()` |
| `names()`, `values()` | range over every field in the container |

Writing:

| Call | Effect |
| --- | --- |
| `add(name, value)` | appends a field, always, even when the name is already present |
| `set(name, value)` | leaves the name present exactly once (see below) |
| `erase(name)` | removes every field with that name, returns `void` |
| `append(other)` | appends every field of another container, without merging names |
| `clear()` | removes every field |

`set` is not an overwrite of the first match. It replaces the first field with that name and erases every other one:

```cpp
headers.add("Set-Cookie", "session=abc");
headers.add("Set-Cookie", "theme=dark");
headers.set("Set-Cookie", "session=xyz"); // both earlier cookies are gone
```

That is what a single-valued field like `Content-Type` wants and the opposite of what a repeatable field wants, where `add` is the correct call. `set` also adopts the casing of the name handed to it, so `set("content-type", v)` makes a field the client sent as `Content-Type` serialize lowercase.

`first_value`, `values`, `fields`, and `names` all borrow from the container, so copy into a `std::string` before adding to, setting, erasing from, or destroying it.

### Migrating from `std::unordered_map`

`request::headers` and `response::response_headers` were `std::unordered_map<std::string, std::string>`. `glz::http_headers` keeps fields in arrival order and lets a name repeat, so the map operations that assumed one value per key are gone. Every replacement below is case-insensitive on the name.

| Was | Now |
| --- | --- |
| `headers["Accept"]` (read) | `headers.first_value("Accept").value_or("")` |
| `headers.at("Accept")` | `headers.first_value("Accept").value()` |
| `headers["Accept"] = v` (on a response) | `res.header("Accept", v)` |
| `headers.count("Accept")` as a presence test | `headers.contains("Accept")` |
| `headers.count("Accept")` as a tally | `headers.count("Accept")` (same call, now counts repeats) |
| `it->first` / `it->second` | `it->name` / `it->value` |
| `headers.erase("Accept")` | `headers.erase("Accept")` (removes every match, but returns `void`) |

Use `.value()` rather than `operator*` when replacing `at()`: `at()` threw `std::out_of_range` on a missing key, `.value()` throws `std::bad_optional_access`, but `operator*` on a missing field is undefined behavior.

`first_value` returns `std::optional<std::string_view>` that borrows from the container, so copy into a `std::string` before the container is modified, moved, or goes out of scope.

Reading every value of a repeated field:

```cpp
for (std::string_view cookie : req.headers.values("Cookie")) {
    // ...
}
```

Testing one item of a comma-separated list, rather than substring-matching the whole value:

```cpp
if (req.headers.contains_token("Connection", "upgrade")) {  // also matches "keep-alive, Upgrade"
}
```

On a response, `header()` replaces any existing field of that name and `add_header()` appends one, which is what
`Set-Cookie` and other repeatable fields need:

```cpp
res.add_header("Set-Cookie", "session=abc; Path=/; HttpOnly")
   .add_header("Set-Cookie", "theme=dark; Path=/");
```

`Content-Length` and `Transfer-Encoding` are replaced even by `add_header`: a second, disagreeing copy would leave the
body length ambiguous (RFC 9112 §6.3). That guard lives in `header()` and `add_header()`, so writing straight to the
`response_headers` container with `.add()` bypasses it and can still put two conflicting fields on the wire.

### Body Framing on Client Requests

`http_client` owns the framing of the requests it sends. It writes the `Content-Length` matching the body it is about to
send, and drops any `Content-Length` or `Transfer-Encoding` supplied in the caller's headers, which could only
contradict it (RFC 9112 §6.3). Requests whose method anticipates content (`POST`, `PUT`, `PATCH`) always carry a
`Content-Length`, including `Content-Length: 0` for an empty body.

The client also rejects a *response* it cannot frame to a single body length: `Content-Length` fields that disagree, or
a value that is not a bare decimal, fail the request with `glz::http_client_error::unframed_response` rather than
guessing a boundary. Fields repeated with the same length are accepted.

A chunked response is exempt. `Transfer-Encoding` overrides `Content-Length` (RFC 9112 §6.3), so the body comes from the
chunk sizes and any `Content-Length` alongside it is never read — including one that would otherwise be rejected.

## HTTP Methods

Glaze supports all standard HTTP methods:

```cpp
server.get("/users", handler);        // GET
server.post("/users", handler);       // POST  
server.put("/users/:id", handler);    // PUT
server.del("/users/:id", handler);    // DELETE
server.patch("/users/:id", handler);  // PATCH
```

## Dependencies

- **ASIO** for networking (standalone ASIO or Boost.Asio)
- **OpenSSL** (optional) for SSL/TLS support
- **C++23** compiler support

> **Important:** The core Glaze serialization library has no external dependencies. ASIO is only required when using networking features.

See the **[ASIO Setup Guide](asio-setup.md)** for detailed installation and configuration instructions, including:
- Installing standalone ASIO or Boost.Asio
- CMake integration examples
- SSL/TLS configuration
- Platform-specific notes
- Troubleshooting common issues

## Installation

Glaze is a header-only library. Include the necessary headers:

```cpp
#include "glaze/net/http_server.hpp"  // For HTTP server
#include "glaze/net/http_client.hpp"  // For HTTP client
```

## Examples

See the [HTTP Examples](http-examples.md) page for complete working examples:

- **Basic Server** - Simple HTTP server with manual routes
- **REST API** - Auto-generated REST API from C++ classes  
- **HTTPS Server** - Secure server with SSL certificates
- **WebSocket Chat** - Real-time chat using WebSockets
- **Microservice** - Production-ready microservice template

## Performance

Glaze HTTP server is designed for high performance:

- **Async I/O** using ASIO for non-blocking operations
- **Thread pool** for handling multiple connections
- **Efficient routing** using radix tree data structure
- **Zero-copy** operations where possible
- **Memory efficient** with minimal allocations

## Next Steps

1. **[HTTP Server](http-server.md)** - Server setup and configuration
2. **[HTTP Router](http-router.md)** - Routing and middleware
3. **[REST Registry](rest-registry.md)** - Auto-generate APIs from C++ classes
4. **[Client Guide](http-client.md)** - Client setup and use
5. **[Advanced Networking](advanced-networking.md)** - CORS, WebSockets, and SSL
6. **[Examples](http-examples.md)** - Practical examples and templates
