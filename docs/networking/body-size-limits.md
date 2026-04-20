# Body Size Limits

Glaze provides configurable maximum body size limits for both the HTTP server and client to prevent memory exhaustion from oversized or malicious payloads. Both default to **100 MB**.

## Server: `max_request_body_size`

Limits incoming request body sizes. Requests whose `Content-Length` exceeds the limit are rejected with **HTTP 413 (Payload Too Large)** before the body is allocated or read.

```cpp
glz::http_server server;
server.max_request_body_size(1 * 1024 * 1024); // 1 MB limit
```

Or via `connection_config`:

```cpp
server.connection_settings({
   .max_request_body_size = 1 * 1024 * 1024
});
```

Set to `0` for no limit. Default: `glz::http_default_max_body_size` (100 MB).

> **Note:** This applies to `Content-Length`-based requests. The server does not currently support chunked request `Transfer-Encoding`.

## Client: `max_response_body_size`

Limits incoming response body sizes. Responses exceeding the limit return `glz::http_client_error::response_too_large`.

```cpp
glz::http_client client;
client.max_response_body_size(10 * 1024 * 1024); // 10 MB limit

auto result = client.get("http://example.com/large-file");
if (!result && result.error() == glz::http_client_error::response_too_large) {
   // handle oversized response
}
```

Set to `0` for no limit. Default: `glz::http_default_max_body_size` (100 MB).

The limit is enforced in all non-streaming response paths:

- **Content-Length responses** — checked before the body is read, avoiding allocation entirely.
- **Chunked responses** — checked before each chunk is read, capping accumulated size.

The streaming API (`stream_request_v2`) delivers data via callbacks and does not accumulate a body internally, so this limit does not apply. Use the existing `max_buffer_size` parameter in `stream_request_params_v2` to control internal buffer growth for streaming connections.
