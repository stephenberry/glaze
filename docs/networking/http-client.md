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

- **Connection Pooling**: Automatically reuses connections for better performance, with stale-connection detection (timestamp eviction + active TCP peek)
- **Redirect Following**: Opt-in automatic following of 3xx responses, with RFC 9110 method rewriting and cross-origin credential stripping
- **Transparent Retry**: Idempotent requests (GET, HEAD, OPTIONS, PUT, DELETE, TRACE) are retried once on connection-level failures when the server has not yet started responding. POST and PATCH are never auto-retried.
- **Chunked Transfer-Encoding**: Transparent decoding of chunked responses across synchronous, asynchronous, and streaming paths
- **Asynchronous Operations**: Non-blocking requests with futures or completion handlers
- **JSON Support**: Built-in JSON serialization for POST requests
- **Thread-Safe**: Multiple threads can safely use the same client instance
- **Error Handling**: Uses `std::expected` for clean error handling

## HTTPS Trust Store Configuration

A new `http_client` starts with certificate verification on (`verify_peer`) and is seeded with the platform's trust anchors: OpenSSL's default verify paths everywhere, plus the Windows `ROOT` certificate store on Windows. In most deployments HTTPS works with no configuration at all.

### Adding your own CA certificates

When you need to trust something the platform does not (a private CA, a self-signed development server, or a CA bundle you ship with your application), add it. All three are additive, so the platform anchors loaded at construction are kept:

```cpp
glz::http_client client;

// From a PEM bundle file on disk
if (auto result = client.add_ca_certificate_file("cacert.pem"); !result) {
    std::cerr << "Failed to add CA: " << result.error().message() << '\n';
}

// From an in-memory PEM bundle, e.g. one embedded in the binary.
// Accepts any number of concatenated PEM certificates.
client.add_ca_certificates_pem(embedded_cacert_pem);

// From an OpenSSL hashed directory (must be indexed with `openssl rehash`)
client.add_ca_certificate_directory("/etc/ssl/certs");
```

`add_ca_certificate_file`, `add_ca_certificates_pem` and `add_os_ca_certificates` are safe to call at any time, including while other threads are issuing requests. `add_ca_certificate_directory` is not: OpenSSL appends to its lookup list without a lock and reads that list unlocked during verification, so call it before the first request.

Prefer these over `set_ssl_verify_mode(asio::ssl::verify_none)`. Disabling verification turns off certificate checking entirely and leaves the connection open to man-in-the-middle attacks; adding an anchor keeps verification intact and simply teaches the client what to trust.

One asymmetry to be aware of: `add_ca_certificate_file` validates the path immediately and reports a missing or malformed bundle, while `add_ca_certificate_directory` cannot. OpenSSL registers directories for lazy lookup during the handshake, so a bad directory path returns success here and surfaces later as a verification failure.

### Troubleshooting `certificate verify failed`

A handshake failing with `certificate verify failed` against an ordinary public endpoint almost always means the trust store is empty rather than that the server is untrustworthy. The `std::error_code` value is `167772294` (`0x0A000086`), which unpacks into OpenSSL library `20` (`ERR_LIB_SSL`) and reason `134` (`SSL_R_CERTIFICATE_VERIFY_FAILED`).

This is most common with OpenSSL from a package manager such as vcpkg or Conan. Those builds bake in an `OPENSSLDIR` pointing at the machine that built them, and ship no CA bundle, so `set_default_verify_paths()` reports success while resolving to a directory that does not exist on your machine. On Windows this used to leave the client with no anchors at all, because OpenSSL never consults the operating system's certificate store on its own.

Glaze loads the Windows `ROOT` store directly (via `crypt32`) when the client is constructed, which covers this case for most machines. Two limits are worth knowing:

- **The `ROOT` store is a cache, not the full Microsoft root program.** Windows ships a seed set and fetches the remaining roots on demand when SChannel needs them. Glaze reads whatever is cached at construction, and OpenSSL cannot trigger the on-demand fetch, so a fresh, offline, or tightly locked-down machine may still be missing an anchor for some public endpoints.
- **The load happens once.** Roots installed afterwards (by Windows Update or an administrator) are not picked up by an already-constructed client.

If either bites, supply the bundle explicitly with `add_ca_certificate_file` or `add_ca_certificates_pem`, which is also the more reproducible choice for deployed software.

Related controls:

- `add_os_ca_certificates()` re-loads the OS anchors, returning how many were added. It returns `0` on platforms where OpenSSL's default verify paths already are the system store (Linux, macOS, the BSDs).
- Define `GLZ_DISABLE_WINDOWS_CERT_STORE` to compile the Windows path out, for example when you want trust pinned to a bundle you supply. The `crypt32` system library remains on the link line, since a consumer-defined macro is not visible to Glaze's CMake.

Roots that Windows restricts to non-TLS purposes are skipped, so a root trusted only for, say, code signing does not become a TLS anchor.

On other platforms, point the client at a bundle explicitly:

```cpp
glz::http_client client;
client.add_ca_certificate_file("cacert.pem");  // e.g. from https://curl.se/docs/caextract.html
```

### Environment-driven configuration

`configure_system_ca_certificates` resolves a bundle through a fallback chain, which is useful when the location differs per deployment:

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

```cpp
glz::http_client client;

if (auto result = client.configure_system_ca_certificates("/path/to/cert.pem"); !result) {
    std::cerr << "Failed to configure CA trust roots: " << result.error().message() << '\n';
    return;
}

auto response = client.get("https://example.com");
```

Note that step 4 succeeds whether or not those paths contain anything, since OpenSSL only registers them for later lookup. If you need certainty that real anchors are loaded, use `add_ca_certificate_file` with a bundle you control.

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
    const glz::http_headers& headers = {}
);
```

### POST Request
```cpp
std::expected<response, std::error_code> post(
    std::string_view url,
    std::string_view body,
    const glz::http_headers& headers = {}
);
```

### PUT Request

```cpp
std::expected<response, std::error_code> put(
    std::string_view url,
    const std::string& body,
    const glz::http_headers& headers = {}
);
```

### PATCH Request

```cpp
std::expected<response, std::error_code> patch(
    std::string_view url,
    const std::string& body,
    const glz::http_headers& headers = {}
);
```

### JSON POST Request
```cpp
template<class T>
std::expected<response, std::error_code> post_json(
    std::string_view url,
    const T& data,
    const glz::http_headers& headers = {}
);
```

### JSON PUT Request
```cpp
template<class T>
std::expected<response, std::error_code> put_json(
    std::string_view url,
    const T& data,
    const glz::http_headers& headers = {}
);
```


### JSON PATCH Request
```cpp
template<class T>
std::expected<response, std::error_code> patch_json(
    std::string_view url,
    const T& data,
    const glz::http_headers& headers = {}
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
    const glz::http_headers& headers = {}
);
```

**Callback-based:**
```cpp
template<typename CompletionHandler>
void get_async(
    std::string_view url,
    const glz::http_headers& headers,
    CompletionHandler&& handler
);
```

### Async POST Request

**Future-based:**
```cpp
std::future<std::expected<response, std::error_code>> post_async(
    std::string_view url,
    std::string_view body,
    const glz::http_headers& headers = {}
);
```

**Callback-based:**
```cpp
template<typename CompletionHandler>
void post_async(
    std::string_view url,
    std::string_view body,
    const glz::http_headers& headers,
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
    const glz::http_headers& headers = {}
);
```

**Callback-based:**
```cpp
template<class T, typename CompletionHandler>
void post_json_async(
    std::string_view url,
    const T& data,
    const glz::http_headers& headers,
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
    glz::http_headers headers;
    http_data_handler on_data;
    http_error_handler on_error;
    http_progress_handler on_progress;
    http_connect_handler on_connect;
    http_disconnect_handler on_disconnect;
    std::function<bool(int)> status_is_error{[](int status){ return status >= 400; }};
};
```

- `method`: The HTTP method to use. (default is "GET")
- `url`: The URL to request.
- `timeout`: Set connection timeout. (default is 30s)
- `strategy`: Can be `bulk_transfer` (default, larger chunks, better throughput) or `immediate_delivery` (smaller chunks, lower latency)
- `max_buffer_size`: Larger buffer can decrease dropouts and increase throughput at cost of memory usage. (default is 1 MiB)
- `body`: The HTTP Body to send.
- `headers`: The HTTP headers to send.
- `on_data`: A callback that's called when data is received; returning `false` cancels the transfer.
  - A callback that returns nothing (or any non-boolean value) will be assumed to always be successful.
- `on_error`: A callback that's called when an error occurs.
- `on_progress`: An optional callback reporting download progress; returning `false` cancels the transfer. See [Progress and Cancellation](#progress-and-cancellation).
- `on_connect`: A callback that's called when the connection is established and the headers are received.
- `on_disconnect`: A callback that's called when the connection is closed.
- `status_is_error`: Optional predicate to decide whether a status code should trigger `on_error` (defaults to checking for codes ≥ 400).

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

### Progress and Cancellation

`on_progress` reports how much of the response body has arrived, which is what a download needs to drive a progress bar and to let the user cancel:

```cpp
using http_progress_handler = std::function<bool(size_t transferred, size_t total)>;
```

-   `transferred`: body bytes handed to `on_data` so far.
-   `total`: the size the response framed itself with, or `0` when no total is knowable in advance - a chunked body, or one framed by connection close. A response framed as empty also reports `0`. In every case, treat `0` as "no determinate total to show" and fall back to an indeterminate display.
-   Return `false` to cancel. The stream stops where it is, the socket is closed rather than returned to the pool, and `on_disconnect` follows exactly as it would for a caller-initiated `disconnect()`.

The callback runs once with `(0, total)` as soon as the response headers are parsed, so the total is in hand before any body arrives, and again after every span passed to `on_data`. It runs on the client's I/O thread, so keep it cheap and marshal to your UI thread rather than blocking in it.

Writing a download straight to disk, with a cancellable progress display:

```cpp
std::ofstream file{"download.bin", std::ios::binary};
std::atomic<bool> cancelled{false}; // set from wherever the user can cancel

auto conn = client.stream_request_v2({
    .url = "https://example.com/large-file",
    .on_data = [&](std::string_view data) { file.write(data.data(), data.size()); },
    .on_error = [](std::error_code ec) { std::cerr << "Download failed: " << ec.message() << '\n'; },
    .on_progress = [&](size_t transferred, size_t total) {
        if (total) {
            std::cout << (transferred * 100 / total) << "%\r" << std::flush;
        }
        return !cancelled.load();
    },
    .on_disconnect = [&] { file.close(); }
});
```

Like `on_progress`, the `on_data` handler can also return `false` to cancel the request early; for instance, a file write operation failing:

```cpp
auto conn = client.stream_request_v2({
    // ...
    .on_data = [&](std::string_view data) {
        file.write(data.data(), data.size());
        return file.good();
    },
    // ...
});
```

### Body Framing

A streamed response ends where its framing says it does:

-   No body at all: a reply to `HEAD`, or a `1xx`, `204` or `304`, ends as soon as its headers are read. A `Content-Length` on such a reply states the length the equivalent `GET` would have had (RFC 9110 6.4.1), so it is reported as the total but no body is waited for.
-   `Content-Length`: the stream ends after exactly that many body bytes. `on_disconnect` fires at the body boundary without waiting for the peer to close, and the connection goes back to the pool for reuse.
-   `Transfer-Encoding: chunked`: the stream ends after the terminal chunk and its trailer section; the connection is reusable.
-   Neither: the body runs until the peer closes the connection, and the socket is not reusable afterwards. An HTTP/1.0 response that did not ask for `Connection: keep-alive` is single-use for the same reason (RFC 9112 9.3).

A peer that closes before the last declared byte has sent an incomplete message (RFC 9112 8); `on_error` reports it rather than letting a truncated download pass for a complete one.

A response carrying `Content-Length` fields that disagree frames no body at all (RFC 9112 6.3) and is refused with `glz::http_client_error::unframed_response` rather than framed by a guess.

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

## Following Redirects

Redirects are not followed by default: a 3xx response is returned to the caller as-is. Set `max_redirects` to the number of hops the client may follow automatically.

```cpp
glz::http_client client{};
client.max_redirects(10);

auto response = client.get("http://example.com/old-path");
// response is whatever the chain ends on
```

The option applies to the synchronous and asynchronous request methods. Streaming requests (`stream_request_v2`) always deliver the 3xx response itself.

Each hop takes the target of the response's `Location` field, resolved against the URL that produced it, so absolute, scheme-relative (`//host/path`), root-relative (`/path`), query-only (`?page=2`) and relative (`../v2/thing`) values all work.

**Method rewriting** follows RFC 9110 15.4:

| Status | Effect |
|---|---|
| 301, 302 | `POST` continues as `GET` with no body; every other method is kept |
| 303 | Continues as `GET` with no body, unless the request was `HEAD` |
| 307, 308 | Method and body are preserved |

When a hop drops the body, `Content-Type` is dropped with it.

300 (Multiple Choices) and 305 (Use Proxy) are never followed; they name no single target to continue onto.

**Credentials** (`Authorization`, `Proxy-Authorization`, `Cookie`) and a caller-supplied `Host` are dropped when a hop crosses to a different scheme, host or port. Other caller headers travel with the whole chain.

Two errors are specific to this path:

- `glz::http_client_error::too_many_redirects` when the chain exceeds `max_redirects()`
- `glz::http_client_error::invalid_redirect` when `Location` is missing, empty, does not resolve to an `http`/`https` target, or carries a control character

A space in the target is percent-encoded rather than rejected, matching browsers and curl.

Both are returned in place of the response, so the last 3xx of an over-long chain is not handed back. Set `max_redirects(0)` and follow the chain yourself if you need to see each hop.

## Response Structure

The `response` object contains:

```cpp
struct response {
    uint16_t status_code;                                      // HTTP status code
    glz::http_headers response_headers; // Response headers
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
        std::cout << "Content-Type: " << response->response_headers.first_value("Content-Type").value_or("") << std::endl;
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

    glz::http_headers headers = {
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

## Connection Pool

`http_client` keeps idle connections for reuse, keyed on `(host, port, scheme)`. A pooled connection is reused only if it passes two checks at acquire time:

1. **Idle timeout.** Entries returned to the pool more than `pool_idle_timeout` ago are evicted before reuse. The default is 4 seconds, chosen to be just under uvicorn's default 5s `--timeout-keep-alive`. Tune to slightly less than your server's keep-alive idle timeout if it differs.
2. **Active liveness check.** For plain TCP sockets, a non-blocking `MSG_PEEK` checks whether the peer has already sent FIN or RST. SSL sockets skip the active peek (it would only see ciphertext) and rely on timestamp eviction plus the transparent-retry path.

```cpp
glz::http_client client;

// Sized for a server with a 10-second keep-alive idle timeout
client.set_pool_idle_timeout(std::chrono::seconds(8));

// Allow more idle connections to a single host (default 10, in line with urllib3)
client.set_pool_max_connections_per_host(64);

// Disable the active peek check (relies on timestamp + retry instead). Mainly
// useful for testing or for trading a syscall per acquire for slightly higher
// recovery latency on stale connections.
client.set_pool_active_liveness_check(false);

// Drop and close every pooled connection. Useful in tests, or when a host has
// signalled (e.g. via 503) that all current sessions should be abandoned.
client.clear_connection_pool();
```

### Performance considerations

- **Pool capacity.** The default cap of 10 connections per host matches urllib3 / requests; OkHttp uses 5, Java HttpClient uses 6 per the HTTP/1.1 RFC's recommended 6 per origin. If your peak per-host concurrency exceeds the cap, returns above the cap are closed rather than pooled, and every request beyond the K-th active connection pays the full connect + TLS handshake cost. For sustained concurrency above the default, raise the cap to at least your observed steady-state per-host concurrency.
- **Worker threads.** When constructed without an external executor, `http_client` runs worker threads internally to drive its own `io_context`. When constructed with an external executor, the caller is responsible for running it (e.g. `io_ctx.run()`).

## Transparent Retry

Connection-level failures (`EOF`, `ECONNRESET`, `EPIPE`, `ECONNABORTED`, `ENOTCONN`, `ESHUTDOWN`) are retried exactly once on a fresh connection when **all** of the following hold:

- The method is idempotent: `GET`, `HEAD`, `OPTIONS`, `PUT`, `DELETE`, `TRACE`.
- The server has not yet started sending a response on this attempt (the header read has not completed).

This behavior covers the two real failure modes you'll hit in production:

- **Stale pooled connection.** A kept-alive connection idles past the server's keep-alive timeout (uvicorn defaults to 5s). The local TCP stack still reports the socket as open, the request write goes into the OS send buffer, and the read fails with EOF or RST. Retry on a fresh connection succeeds.
- **Fresh-socket failure during write.** The server accepts the connection but cannot process the request (listener overload, server crash between `accept` and `read`, immediate close after accept for rate limiting). The write succeeds locally; the read fails. Retry on a fresh connection succeeds.

Once any response bytes have been received on the wire, retry is suppressed even on idempotent methods, so a mid-body RST does not cause the client to silently re-issue a request the server already processed.

### POST and PATCH

`POST` and `PATCH` are never auto-retried. The client cannot distinguish between "request not delivered" and "request delivered, response lost," so retrying could silently double-execute a non-idempotent operation. If you need retry semantics for POST/PATCH, build them at the application layer with idempotency keys or explicit retry tokens.

```cpp
auto resp = client.post("https://api.example.com/payments", body);
if (!resp) {
    // Handle the error explicitly. The request may or may not have been
    // processed by the server; consult application-level idempotency state
    // before deciding to retry.
}
```

### Disabling automatic retry

There is no global "disable retry" switch by design: the retry conditions are conservative enough that disabling them would only change behavior in cases where retrying is correct. If the test scenario requires it, you can raise the bar by lowering `pool_idle_timeout` so stale entries are evicted (rather than reused and retried) and the failure modes that drive retry don't arise in the first place.
