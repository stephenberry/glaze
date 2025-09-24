# HTTP Client

The Glaze HTTP client provides a simple and efficient way to make HTTP requests with connection pooling and asynchronous operations.

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
- **Asynchronous Operations**: Non-blocking requests with futures or completion handlers
- **JSON Support**: Built-in JSON serialization for POST requests
- **Thread-Safe**: Multiple threads can safely use the same client instance
- **Error Handling**: Uses `std::expected` for clean error handling

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
template<typename OnData, typename OnError, typename OnConnect, typename OnDisconnect>
stream_connection::pointer stream_request(stream_options options);
```

The `stream_options` struct contains the following fields:

```cpp
struct stream_options {
    std::string url;
    std::string method = "GET";
    std::unordered_map<std::string, std::string> headers;
    OnData on_data;
    OnError on_error;
    OnConnect on_connect;
    OnDisconnect on_disconnect;
    std::function<bool(int)> status_is_error;
};
```

-   `url`: The URL to request.
-   `method`: The HTTP method to use (default: "GET").
-   `headers`: The HTTP headers to send.
-   `on_data`: A callback that's called when data is received.
-   `on_error`: A callback that's called when an error occurs.
-   `on_connect`: A callback that's called when the connection is established and the headers are received.
-   `on_disconnect`: A callback that's called when the connection is closed.
-   `status_is_error`: Optional predicate to decide whether a status code should trigger `on_error` (defaults to
    checking for codes ≥ 400).

To override the default behaviour you can supply a predicate:

```cpp
auto conn = client.stream_request({
    .url = "http://localhost/typesense",
    .on_data = on_data,
    .on_error = on_error,
    .status_is_error = [](int status) { return status >= 500; } // Ignore 4xx responses
});
```

The `stream_connection::pointer` object contains a `disconnect()` method that can be used to close the connection.

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

## Performance Notes

- The client automatically pools connections for better performance when making multiple requests to the same host
- Multiple worker threads are used internally to handle concurrent requests
- The connection pool has a configurable limit (default: 10 connections per host), which can be adjusted using the `http_client::options::max_connections` option.
- Connections are automatically returned to the pool when requests complete successfully
