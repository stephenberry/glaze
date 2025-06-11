# HTTP Client

The Glaze HTTP client provides a simple and efficient way to make HTTP requests.

## Basic Usage

```cpp
#include "glaze/net/http_client.hpp"

int main() {
    glz::http_client client;

    auto response = client.get("https://example.com");

    if (response) {
        std::cout << "Status: " << response->status_code << std::endl;
        std::cout << "Body: " << response->response_body << std::endl;
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
```

## Methods

*   `get(url, headers)`: Performs an HTTP GET request to the specified URL.
*   `post(url, body, headers)`: Performs an HTTP POST request to the specified URL with the given body and headers.
*   `put(url, body, headers)`: Performs an HTTP PUT request to the specified URL with the given body and headers.
*   `del(url, headers)`: Performs an HTTP DELETE request to the specified URL with the given headers.
*   `patch(url, body, headers)`: Performs an HTTP PATCH request to the specified URL with the given body and headers.

## Asynchronous Requests

The Glaze HTTP client also supports asynchronous requests, allowing you to perform non-blocking operations.

*   `get_async(url, headers)`: Performs an asynchronous HTTP GET request to the specified URL. Returns a `std::future` that will hold the response.
*   `post_async(url, body, headers)`: Performs an asynchronous HTTP POST request to the specified URL with the given body and headers. Returns a `std::future` that will hold the response.

```cpp
#include "glaze/net/http_client.hpp"
#include <future>

int main() {
    glz::http_client client;

    auto future_response = client.get_async("https://example.com");

    // Do other work

    auto response = future_response.get(); // Get the response when ready

    if (response) {
        std::cout << "Status: " << response->status_code << std::endl;
        std::cout << "Body: " << response->response_body << std::endl;
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
```

## Error Handling

The HTTP client returns a `std::expected` object, which contains either the response or an error code. You can check for errors using the `has_value()` method or by accessing the `error()` method.

```cpp
#include "glaze/net/http_client.hpp"

int main() {
    glz::http_client client;

    auto response = client.get("https://example.com");

    if (response) {
        // Request was successful
    } else {
        std::error_code ec = response.error();
        std::cerr << "Error: " << ec.message() << std::endl;
    }

    return 0;
}
```

## Examples

### GET Request

```cpp
#include "glaze/net/http_client.hpp"

int main() {
    glz::http_client client;

    auto response = client.get("https://example.com/api/data");

    if (response) {
        std::cout << "Status: " << response->status_code << std::endl;
        std::cout << "Body: " << response->response_body << std::endl;
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
```

### POST Request with JSON

```cpp
#include "glaze/net/http_client.hpp"
#include "glaze/glaze.hpp"

struct MyData {
    int id;
    std::string name;
};

int main() {
    glz::http_client client;

    MyData data{1, "example"};
    std::string body;
    glz::write_json(data, body);

    std::unordered_map<std::string, std::string> headers = {
        {"Content-Type", "application/json"}
    };

    auto response = client.post("https://example.com/api/items", body, headers);

    if (response) {
        std::cout << "Status: " << response->status_code << std::endl;
        std::cout << "Body: " << response->response_body << std::endl;
    } else {
        std::cerr << "Error: " << response.error().message() << std::endl;
    }

    return 0;
}
