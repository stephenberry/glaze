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

`try_route`/`try_on` return `glz::expected<void, std::string>` and never throw. The router also records the first conflict, queryable via `router.has_route_error()` / `router.route_error()`.

### Failing a request without throwing

A registered handler can fail a request by returning `glz::expected<T, E>`. The registry translates the error into the active protocol's error representation, so the same handler works with or without exceptions:

```c++
struct calculator {
   // string error  -> REPE error_code::parse_error / JSON-RPC internal error (-32603)
   std::function<glz::expected<int, std::string>(div_params)> divide = [](div_params p)
      -> glz::expected<int, std::string> {
      if (p.denominator == 0) return glz::unexpected(std::string("division by zero"));
      return p.numerator / p.denominator;
   };
};
```

For full JSON-RPC fidelity (custom code, message, and `data`), return `glz::expected<T, glz::rpc::error>`. REPE handlers may return `glz::expected<T, glz::error_code>` to choose the response error code. When exceptions are enabled, a handler that throws is still caught and reported as before.

