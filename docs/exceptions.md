# Glaze Exceptions

If C++ exceptions are being used in your codebase, it can be cleaner to call functions that throw.

Glaze provides a header `glaze_exceptions.hpp`, which provides an API to use Glaze with exceptions. This header will be disabled if `-fno-exceptions` is specified.

> Glaze functions that throw exceptions use the namespace `glz::ex`

```c++
std::string buffer{};
glz::ex::read_json(obj, buffer); // will throw on a read error
```

## Building without exceptions

Glaze compiles cleanly with `-fno-exceptions`. Core read/write functions never throw; they report problems through `glz::error_ctx`/`glz::expected`. The RPC `glz::registry` (REPE, JSON-RPC, and REST) and `glz::http_router` are also usable with `-fno-exceptions`. Errors are surfaced through return values rather than thrown or swallowed.

### Reporting route registration conflicts

`http_router::route()` throws `std::runtime_error` on a structural route conflict (a duplicate `:param`/`*wildcard` name at the same position, or a wildcard that is not the final segment) when exceptions are enabled. For exception-free builds, or whenever you want to handle a conflict explicitly, use the non-throwing variants:

```c++
glz::http_router router{};
if (auto ec = router.try_route(glz::http_method::GET, "/users/:id", handler); !ec) {
   // ec.error() is a human-readable conflict message
}

glz::registry<glz::opts{}, glz::REST> registry{};
if (auto ec = registry.try_on(api); !ec) {
   std::fprintf(stderr, "registration failed: %s\n", ec.error().c_str());
}
```

`try_route`/`try_on` return `glz::expected<void, std::string>` and never throw; `try_stream` and `try_websocket` give streaming and WebSocket routes the same return-value channel. The router also records the first conflict, queryable via `router.has_route_error()` / `router.route_error()`.

### Failing a request without throwing

A registered handler can fail a request by returning `glz::expected<T, E>`. The registry translates the error into the active protocol's error representation, so the same handler works with or without exceptions:

```c++
struct calculator {
   // string error -> REPE error_code::parse_error / JSON-RPC internal error (-32603) / REST 500
   std::function<glz::expected<int, std::string>(div_params)> divide = [](div_params p)
      -> glz::expected<int, std::string> {
      if (p.denominator == 0) return glz::unexpected(std::string("division by zero"));
      return p.numerator / p.denominator;
   };
};
```

For full JSON-RPC fidelity (custom code, message, and `data`), return `glz::expected<T, glz::rpc::error>`. REPE handlers may return `glz::expected<T, glz::error_code>` to choose the response error code. When exceptions are enabled, a handler that throws is still caught and reported as before.

#### REST

For REST the error is translated into an HTTP response rather than an RPC error envelope: the success value is serialized as the response body, while an error sets a non-2xx status and serializes a `glz::http_error` body (`{"status":N,"message":...}`, which a client can read straight back into a `glz::http_error`). The status is derived from the error type:

- `glz::error_code` maps to a sensible HTTP status (`invalid_body`/`parse_error`/`invalid_query`/`syntax_error`/`constraint_violated` → 400, `method_not_found`/`key_not_found`/`nonexistent_json_ptr` → 404, `timeout` → 504, otherwise 500).
- A `std::string` or `glz::error_ctx` error maps to 500 (treated as a server-side failure).
- Return `glz::expected<T, glz::http_error>` to choose the status and message explicitly:

```c++
std::function<glz::expected<user, glz::http_error>(int)> find_user = [](int id)
   -> glz::expected<user, glz::http_error> {
   if (auto* u = lookup(id)) return *u;
   return glz::unexpected(glz::http_error{404, "user not found"});
};
```

`glz::rpc::error` is JSON-RPC-specific and is **not** a REST error type, since `glaze/net` does not depend on the JSON-RPC extension; use `glz::http_error` for explicit status control. Raw `glz::http_router` handlers that take a `glz::response&` can of course call `res.status(...)` directly instead of returning `glz::expected`.

> **Behavior change:** Previously a reflected handler returning `glz::expected<T, E>` had its result serialized directly, so an error came back as a *successful* response whose body was `{"unexpected": <E>}` (a REPE/JSON-RPC result, or an HTTP 200 for REST). The registry now intercepts `glz::expected` as the success/error channel across all three protocols: a present value is still unwrapped and serialized exactly as before, but an error is translated into a protocol error (a REPE/JSON-RPC error response, or a non-2xx HTTP status as described above) instead of a success payload. Only the error path changed. A handler that intentionally wants the old `{"unexpected": ...}` payload can return a non-`expected` type that serializes that shape rather than `glz::expected`.

