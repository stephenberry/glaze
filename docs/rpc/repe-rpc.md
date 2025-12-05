# REPE RPC

> [!WARNING]
>
> **The `glz::asio_server`, `glz::asio_client`, and `glz::repe::registry`, are all under active development and should not be considered stable.**

Glaze provides support for the [REPE](https://github.com/stephenberry/repe) RPC format along with client/server implementations (currently [asio](http://think-async.com/Asio/)).

- [REPE specification](https://github.com/stephenberry/repe)

JSON Pointer syntax is used to refer to fields in C++ structures.

## REPE Registry

The `glz::repe::registry` produces an API for function invocation and variable access for server implementations.

> The registry does not currently support adding methods from RPC calls or adding methods once RPC calls can be made.

## REPE registry locking and thread safety

The `glz::repe::registry` allows users to expose C++ classes directly to the registry as an interface for network RPC calls and POST/GET calls.

> [!IMPORTANT]
>
> Like most registry implementations, no locks are acquired for reading/writing to the registry and all thread safety must be managed by the user. This allows flexible, performant interfaces to be developed on top of the registry. It is recommended to register safe types, such as `std::atomic<int>`. Glaze provides some higher level thread safe classes to be used in these asynchronous interfaces.

## Thread Safe Classes

- `glz::async_string` in `glaze/thread/async_string.hpp`
  - Provides a thread safe `std::string` wrapper, which Glaze can read/write from safely in asynchronous calls.

## asio

> [!NOTE]
>
> `glz::asio_server` and `glz::asio_client` require the [standalone asio](https://think-async.com/Asio/AsioStandalone.html) to build. Glaze does not include this dependency within its CMake files. The developer is expected to include this header-only library.

### Error Handling

The `glz::asio_server` provides an `error_handler` callback to manage exceptions thrown during request processing. By default, exceptions are caught, and an error response is sent to the client. To log or monitor these errors on the server, assign a callback:

```cpp
server.error_handler = [](const std::string& error_msg) {
   std::cerr << "Server error: " << error_msg << '\n';
};
```

### Custom Call Handler

The `glz::asio_server` provides an optional `call` member that allows intercepting all incoming REPE calls before they reach the registry. This enables custom routing, middleware patterns, and plugin dispatch.

The handler uses a **zero-copy API** where the request is provided as a span and the response is written directly to a buffer:

```cpp
glz::asio_server server{.port = 8080, .concurrency = 4};

my_api api{};
server.on(api);

// Set custom call handler (zero-copy API)
server.call = [&](std::span<const char> request, std::string& response_buffer) {
   // Zero-copy parse - query and body are views into the request buffer
   auto result = glz::repe::parse_request(request);
   if (!result) {
      glz::repe::encode_error_buffer(glz::error_code::parse_error, response_buffer, "Failed to parse request");
      return;
   }

   const auto& req = result.request;
   glz::repe::response_builder resp{response_buffer};

   // Custom routing based on path
   if (req.query.starts_with("/custom/")) {
      resp.reset(req);
      resp.set_body_raw(R"({"handled": "custom"})", glz::repe::body_format::JSON);
   }
   else {
      // Delegate to registry (also zero-copy)
      server.registry.call(request, response_buffer);
   }
};

server.run();
```

When `call` is set, it is invoked instead of `registry.call()` for every request. This allows:

- **Custom routing**: Route requests to different handlers based on path prefixes
- **Middleware**: Add logging, authentication, or metrics before/after registry calls
- **Multi-registry patterns**: Dispatch to different registries based on request metadata
- **Plugin systems**: Forward messages to dynamically loaded plugins

If `call` is not set (the default), requests are processed directly by the registry.

### Zero-Copy Types

The zero-copy API uses these types:

- **`glz::repe::parse_request(span)`**: Parses a request with zero-copy. Returns a `parse_result` containing a `request_view`.
- **`glz::repe::request_view`**: Views into the original request buffer (query and body are `std::string_view`).
- **`glz::repe::response_builder`**: Writes responses directly to a buffer without intermediate copies.

See [REPE Buffer Handling](repe-buffer.md) for detailed documentation of these types.

## Registering Multiple Objects with `glz::merge`

By default, when you register an object with `server.on(obj)`, the root path `""` returns that object's JSON representation. If you call `server.on()` multiple times with different objects, only the last registered object will be returned at the root path `""`.

To combine multiple objects into a single merged view at the root path, use `glz::merge`:

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

// Create a merged view of both objects
auto merged = glz::merge{sensors, settings};

glz::registry server{};
server.on(merged);
```

Now queries work as follows:

- `""` returns the combined view: `{"temperature":25.0,"humidity":60.0,"refresh_rate":100,"mode":"auto"}`
- `/temperature` returns `25.0`
- `/refresh_rate` returns `100`
- Individual field writes work: writing to `/temperature` updates `sensors.temperature`

### Important Notes

**`glz::merge` stores references, not copies.** The original objects must remain alive for the duration of the registry's use. Changes to the original objects are reflected when queried.

**The merged root endpoint is read-only.** Writing to the `""` path returns an error. This limitation exists because efficiently parsing JSON into multiple distinct objects would require either:
- Multiple parse passes (O(N) where N is the number of merged objects)
- An intermediate representation with extra memory allocation
- Runtime dispatch overhead that loses glaze's compile-time optimizations

Individual field paths remain writable, so you can still update any field via its specific path (e.g., `/temperature`, `/mode`).

