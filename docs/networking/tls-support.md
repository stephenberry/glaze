# TLS/SSL Support in Glaze HTTP Server

Glaze supports TLS/SSL encryption for HTTPS servers through a template-based approach that provides zero overhead for HTTP-only applications.

## Overview

The `http_server` has been enhanced with a template parameter `<bool EnableTLS>` that allows you to create both HTTP and HTTPS servers:

- `http_server<false>` or `http_server<>` - Standard HTTP server (default)
- `http_server<true>` or `https_server` - HTTPS server with TLS support

## Building with TLS Support

### Prerequisites

- OpenSSL development libraries
- CMake 3.21 or later
- C++23 compatible compiler

### CMake Configuration

Enable TLS support when configuring your build:

```bash
cmake -Dglaze_ENABLE_SSL=ON ..
```

This will:
- Find and link OpenSSL libraries
- Define `GLZ_ENABLE_SSL` preprocessor macro
- Enable TLS functionality in the http_server template

## Usage

### Basic HTTPS Server

```cpp
#include "glaze/net/http_server.hpp"

int main() {
    // Create HTTPS server using alias
    glz::https_server server;
    
    // Load SSL certificate and private key
    server.load_certificate("path/to/cert.pem", "path/to/private_key.pem");
    
    // Configure routes
    server.get("/", [](const glz::request& req, glz::response& res) {
        res.body("Hello, HTTPS World!");
    });
    
    // Start server on port 8443
    server.bind(8443).start();
    
    return 0;
}
```

### Using Template Parameter

```cpp
// Explicit template parameter
glz::http_server<true> https_server;
glz::http_server<false> http_server;  // or just http_server<>
```

### SSL Configuration

```cpp
glz::https_server server;

// Load certificate and key
server.load_certificate("cert.pem", "key.pem");

// Set SSL verification mode (optional)
server.set_ssl_verify_mode(SSL_VERIFY_NONE);  // For development
// server.set_ssl_verify_mode(SSL_VERIFY_PEER);  // For production
```

## Certificate Generation

For development and testing, you can generate self-signed certificates:

```bash
# Generate private key
openssl genrsa -out key.pem 2048

# Generate self-signed certificate
openssl req -new -x509 -key key.pem -out cert.pem -days 365
```

## API Reference

### Template Parameters

- `EnableTLS` (bool): Enable TLS support
  - `false` (default): HTTP server
  - `true`: HTTPS server

### Type Aliases

- `https_server`: Alias for `http_server<true>`

### HTTPS-Specific Methods

#### `load_certificate(cert_file, key_file)`
Load SSL certificate and private key files (PEM format).

```cpp
server.load_certificate("path/to/cert.pem", "path/to/key.pem");
```

#### `set_ssl_verify_mode(mode)`
Set SSL peer verification mode.

```cpp
server.set_ssl_verify_mode(SSL_VERIFY_NONE);    // No verification
server.set_ssl_verify_mode(SSL_VERIFY_PEER);    // Verify peer certificate
```

## Design Benefits

### Zero Overhead for HTTP
- HTTP servers (`EnableTLS = false`) have no TLS-related code or memory overhead
- SSL headers and context are only included when `EnableTLS = true`
- Compile-time optimization ensures maximum performance

### Type Safety
- Template parameter provides compile-time differentiation
- Prevents accidental mixing of HTTP and HTTPS configurations
- Clear API distinction between server types

### Backward Compatibility
- Existing HTTP server code continues to work unchanged
- Default template parameter maintains HTTP behavior
- No breaking changes to existing APIs

## Example: Mixed HTTP/HTTPS Setup

```cpp
#include "glaze/net/http_server.hpp"

int main() {
    // HTTP server for public content
    glz::http_server<> http_server;
    http_server.get("/", [](const glz::request& req, glz::response& res) {
        res.body("Public HTTP content");
    });
    
    // HTTPS server for secure content
    glz::https_server https_server;
    https_server.load_certificate("cert.pem", "key.pem")
                .get("/secure", [](const glz::request& req, glz::response& res) {
                    res.body("Secure HTTPS content");
                });
    
    // Start both servers
    std::thread http_thread([&]() { 
        http_server.bind(8080).start(); 
    });
    
    std::thread https_thread([&]() { 
        https_server.bind(8443).start(); 
    });
    
    http_thread.join();
    https_thread.join();
    
    return 0;
}
```

## Troubleshooting

### OpenSSL Not Found
If CMake cannot find OpenSSL:

```bash
# On Ubuntu/Debian
sudo apt-get install libssl-dev

# On macOS with Homebrew
brew install openssl
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig"

# On Windows
vcpkg install openssl
```

### Certificate Issues
- Ensure certificate and key files are in PEM format
- Check file permissions and paths
- For production, use certificates from a trusted CA
- For development, self-signed certificates are acceptable

### Compilation Errors
- Ensure `GLZ_ENABLE_SSL` is defined when using TLS features
- Verify OpenSSL libraries are properly linked
- Check that C++23 standard is enabled
