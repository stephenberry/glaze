# HTTP Client

The Glaze HTTP client provides a simple and efficient way to make HTTP requests with connection pooling and asynchronous operations.

> **Prerequisites:** This feature requires ASIO. See the [ASIO Setup Guide](asio-setup.md) for installation instructions. For HTTPS connections, OpenSSL is also required.

## Basic Usage

```cpp
#include "glaze/net/http_client.hpp"

int main() {
    glz::http_client client;

    auto response = client.get("https://example.com");

    if (response) {
        std::cout << "Status: " << response->status_code << std::endl;
        std::cout << "Body: " << response->response_body << std::endl;
        
        // Access response headers
        for (const auto& [name, value] : response->response_headers) {
            std::cout << name << ": " << value << std::endl;
        }
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
```

## Features

- **Connection Pooling**: Automatically reuses connections for better performance
- **Chunked Transfer-Encoding**: Transparent decoding of chunked responses across synchronous, asynchronous, and streaming paths
- **Asynchronous Operations**: Non-blocking requests with futures or completion handlers
- **JSON Support**: Built-in JSON serialization for POST requests
- **Thread-Safe**: Multiple threads can safely use the same client instance
- **Error Handling**: Uses `std::expected` for clean error handling

## HTTPS Trust Store Configuration

When using HTTPS, certificate verification depends on your OpenSSL trust store configuration.
To simplify setup, `http_client` provides:

```cpp
std::expected<void, std::error_code> configure_system_ca_certificates(
    std::optional<std::string_view> cert_bundle_file = std::nullopt
);
```

Fallback order:

1. Explicit `cert_bundle_file` argument (if provided)
2. `SSL_CERT_FILE` environment variable
3. `SSL_CERT_DIR` environment variable
4. OpenSSL default verify paths (`set_default_verify_paths`)

Example:

```cpp
glz::http_client client;

if (auto ec = client.configure_system_ca_certificates("/path/to/cert.pem"); !ec) {
    std::cerr << "Failed to configure CA trust roots: " << ec.error().message() << '\n';
    return;
}

auto response = client.get("https://example.com");
```

Notes:

- On macOS with Homebrew OpenSSL, you may need to point `SSL_CERT_FILE` at Homebrew's CA bundle
  (commonly `/opt/homebrew/etc/ca-certificates/cert.pem`).
- OpenSSL certificate lookup behavior is platform/package-manager dependent; explicit configuration is recommended
  for reproducible deployments.

## Synchronous Methods

### GET Request
```cpp
std::expected<response, std::error_code> get(
    std::string_view url,
    const std::unordered_map<std::string, std::string>& headers = {}
);
```

### POST Request
```cpp
std::expected<response, std::error_code> post(
    std::string_view url, 
    std::string_view body,
    const std::unordered_map<std::string, std::string>& headers = {}
);
```

### JSON POST Request
```cpp
template<class T>
std::expected<response, std::error_code> post_json(
    std::string_view url, 
    const T& data,
    const std::unordered_map<std::string, std::string>& headers = {}
);
```

## Asynchronous Methods

All asynchronous methods come in two variants:

1. **Future-based**: Returns a `std::future` for the response
2. **Callback-based**: Takes a completion handler that's called when the operation completes

### Async GET Request

**Future-based:**
```cpp
std::future<std::expected<response, std::error_code>> get_async(
    std::string_view url,
    const std::unordered_map<std::string, std::string>& headers = {}
);
```

**Callback-based:**
```cpp
template<typename CompletionHandler>
void get_async(
    std::string_view url,
    const std::unordered_map<std::string, std::string>& headers,
    CompletionHandler&& handler
);
```

### Async POST Request

**Future-based:**
```cpp
std::future<std::expected<response, std::error_code>> post_async(
    std::string_view url, 
    std::string_view body,
    const std::unordered_map<std::string, std::string>& headers = {}
);
```

**Callback-based:**
```cpp
template<typename CompletionHandler>
void post_async(
    std::string_view url, 
    std::string_view body,
    const std::unordered_map<std::string, std::string>& headers,
    CompletionHandler&& handler
);
```

### Async JSON POST Request

**Future-based:**
```cpp
template<class T>
std::future<std::expected<response, std::error_code>> post_json_async(
    std::string_view url, 
    const T& data,
    const std::unordered_map<std::string, std::string>& headers = {}
);
```

**Callback-based:**
```cpp
template<class T, typename CompletionHandler>
void post_json_async(
    std::string_view url, 
    const T& data,
    const std::unordered_map<std::string, std::string>& headers,
    CompletionHandler&& handler
);
```

## Streaming Requests

The HTTP client supports streaming requests, which allow you to receive data in chunks.

```cpp
std::shared_ptr<http_stream_connection> stream_request_v2(const stream_request_params_v2& params);
```

The `stream_request_params_v2` struct contains the following fields:

```cpp
struct stream_request_params_v2 {
    std::string method{"GET"};
    std::string url;
    std::chrono::seconds timeout{30s};
    stream_read_strategy strategy{stream_read_strategy::bulk_transfer};
    size_t max_buffer_size{1024 * 1024};
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    http_connect_handler on_connect;
    http_disconnect_handler on_disconnect;
    http_data_handler on_data;
    http_error_handler on_error;
    std::function<bool(int)> status_is_error{[](int status){ return status >= 400; }};
};
```

-   `method`: The HTTP method to use. (default is "GET")
-   `url`: The URL to request.
-   `timeout`: Set connection timeout. (default is 30s)
-   `strategy`: Can be `bulk_transfer` (default, larger chunks, better throughput) or `immediate_delivery` (smaller chunks, lower latency)
-   `max_buffer_size`: Larger buffer can decrease dropouts and increase throughtput at cost of memory usage. (default is 1 MiB)
-   `body`: The HTTP Body to send.
-   `headers`: The HTTP headers to send.
-   `on_connect`: A callback that's called when the connection is established and the headers are received.
-   `on_disconnect`: A callback that's called when the connection is closed.
-   `on_data`: A callback that's called when data is received.
-   `on_error`: A callback that's called when an error occurs.
-   `status_is_error`: Optional predicate to decide whether a status code should trigger `on_error` (defaults to checking for codes ≥ 400).

To override the default behaviour you can supply a predicate:

```cpp
auto conn = client.stream_request_v2({
    .url = "http://localhost/typesense",
    .on_data = on_data,
    .on_error = on_error,
    .status_is_error = [](int status) { return status >= 500; } // Ignore 4xx responses
});
```

The `http_stream_connection` object contains a `disconnect()` method that can be used to close the connection.

### Handling HTTP Errors During Streaming

When the server responds with an HTTP error (status code ≥ 400) the client immediately invokes `on_error` with an
`std::error_code` whose category is `glz::http_status_category()`. You can extract the numeric status code by comparing
the category directly or by using the helper `glz::http_status_from(ec)`:

```cpp
auto on_error = [](std::error_code ec) {
    if (auto status = glz::http_status_from(ec)) {
        std::cerr << "Server failed with HTTP status " << *status << "\n";
        return;
    }

    // Fallback for transport errors
    std::cerr << "Stream error: " << ec.message() << "\n";
};
```

## Response Structure

The `response` object contains:

```cpp
struct response {
    uint16_t status_code;                                      // HTTP status code
    std::unordered_map<std::string, std::string> response_headers; // Response headers
    std::string response_body;                                 // Response body
};
```

## Error Handling

The HTTP client returns a `std::expected` object for synchronous and asynchronous requests, which contains either the response or an error code. You can check for errors using the `has_value()` method or by accessing the `error()` method.

```cpp
auto response = client.get("https://example.com");

if (response) {
    // Request was successful
    std::cout << "Status: " << response->status_code << std::endl;
} else {
    std::error_code ec = response.error();
    std::cerr << "Error: " << ec.message() << std::endl;
}
```

For streaming requests, errors are reported via the `on_error` callback in the `stream_options` struct. The client translates HTTP error statuses (4xx/5xx) into `std::errc::connection_refused` errors.

## Examples

### Simple GET Request

```cpp
#include "glaze/net/http_client.hpp"

int main() {
    glz::http_client client;

    auto response = client.get("https://api.github.com/users/octocat");

    if (response) {
        std::cout << "Status: " << response->status_code << std::endl;
        std::cout << "Content-Type: " << response->response_headers["Content-Type"] << std::endl;
        std::cout << "Body: " << response->response_body << std::endl;
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
```

### POST Request with Custom Headers

```cpp
#include "glaze/net/http_client.hpp"

int main() {
    glz::http_client client;

    std::unordered_map<std::string, std::string> headers = {
        {"Content-Type", "text/plain"},
        {"Authorization", "Bearer your-token"}
    };

    auto response = client.post("https://api.example.com/data", "Hello, World!", headers);

    if (response) {
        std::cout << "Status: " << response->status_code << std::endl;
        std::cout << "Response: " << response->response_body << std::endl;
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
```

### JSON POST Request

```cpp
#include "glaze/net/http_client.hpp"
#include "glaze/glaze.hpp"

struct User {
    int id;
    std::string name;
    std::string email;
};

int main() {
    glz::http_client client;

    User user{123, "John Doe", "john@example.com"};

    // Using the convenient post_json method
    auto response = client.post_json("https://api.example.com/users", user);

    if (response) {
        std::cout << "User created! Status: " << response->status_code << std::endl;
        std::cout << "Response: " << response->response_body << std::endl;
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
```

### Asynchronous Requests with Futures

```cpp
#include "glaze/net/http_client.hpp"
#include <future>
#include <vector>

int main() {
    glz::http_client client;

    // Launch multiple async requests
    std::vector<std::future<std::expected<glz::response, std::error_code>>> futures;
    
    futures.push_back(client.get_async("https://api.github.com/users/octocat"));
    futures.push_back(client.get_async("https://api.github.com/users/defunkt"));
    futures.push_back(client.get_async("https://api.github.com/users/pjhyett"));

    // Wait for all requests to complete
    for (auto& future : futures) {
        auto response = future.get();
        if (response) {
            std::cout << "Status: " << response->status_code << std::endl;
        } else {
            std::cerr << "Error: " << response.error().message() << std::endl;
        }
    }

    return 0;
}
```

### Asynchronous Requests with Callbacks

```cpp
#include "glaze/net/http_client.hpp"
#include <iostream>

int main() {
    glz::http_client client;

    // Async GET with callback
    client.get_async("https://api.github.com/users/octocat", {},
        [](std::expected<glz::response, std::error_code> result) {
            if (result) {
                std::cout << "Async GET completed! Status: " << result->status_code << std::endl;
            } else {
                std::cerr << "Async GET failed: " << result.error().message() << std::endl;
            }
        });

    // Async JSON POST with callback
    struct Data { int value = 42; };
    Data data;
    
    client.post_json_async("https://httpbin.org/post", data, {},
        [](std::expected<glz::response, std::error_code> result) {
            if (result) {
                std::cout << "Async JSON POST completed! Status: " << result->status_code << std::endl;
            } else {
                std::cerr << "Async JSON POST failed: " << result.error().message() << std::endl;
            }
        });

    // Keep the main thread alive long enough for async operations to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return 0;
}
```

## URL Parsing

The client includes a URL parsing utility:

```cpp
#include "glaze/net/http_client.hpp"

auto url_parts = glz::parse_url("https://api.example.com:8080/v1/users");
if (url_parts) {
    std::cout << "Protocol: " << url_parts->protocol << std::endl; // "https"
    std::cout << "Host: " << url_parts->host << std::endl;         // "api.example.com"
    std::cout << "Port: " << url_parts->port << std::endl;         // 8080
    std::cout << "Path: " << url_parts->path << std::endl;         // "/v1/users"
}
```

## Chunked Transfer-Encoding

Responses using `Transfer-Encoding: chunked` are automatically decoded. This is transparent to the caller -- `response_body` contains the fully assembled body regardless of whether the server used `Content-Length` or chunked encoding.

- Chunk extensions (`;key=value`) are ignored per RFC 7230
- Trailer headers after the terminal chunk are consumed and discarded
- Malformed chunk sizes return a `protocol_error`

For streaming requests (`stream_request_v2`), chunked data is delivered incrementally via the `on_data` callback as each chunk arrives.

## Performance Notes

- The client automatically pools connections for better performance when making multiple requests to the same host
- Multiple worker threads are used internally to handle concurrent requests
- The connection pool has a configurable limit (default: 10 connections per host), which can be adjusted using the `http_client::options::max_connections` option.
- Connections are automatically returned to the pool when requests complete successfully
