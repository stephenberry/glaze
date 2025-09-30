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

struct UserService {
    std::vector<User> getAllUsers() { /* ... */ }
    User getUserById(int id) { /* ... */ }
    User createUser(const User& user) { /* ... */ }
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
    std::unordered_map<std::string, std::string> headers;  // HTTP headers
    std::string body;                                      // Request body
    std::string remote_ip;                                 // Client IP
    uint16_t remote_port;                                  // Client port
};
```

### Response Builder

```cpp
struct response {
    int status_code = 200;
    std::unordered_map<std::string, std::string> response_headers;
    std::string response_body;
    
    // Fluent interface
    response& status(int code);
    response& header(std::string_view name, std::string_view value);
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

- **ASIO** for networking
- **OpenSSL** (optional) for SSL/TLS support
- **C++23** compiler support

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
