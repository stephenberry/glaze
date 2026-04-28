# OpenRPC

Glaze can generate [OpenRPC 1.3.2](https://spec.open-rpc.org/) specification documents from a `glz::registry`, providing machine-readable API discovery for RPC services.

- [OpenRPC specification](https://spec.open-rpc.org/)

The OpenRPC spec is generated from the same type information that Glaze uses for serialization, so the spec always matches the actual API.

## Generating a Spec

Set API metadata on the registry's `open_rpc_info` member, then call `open_rpc_spec()` to get the document:

```cpp
#include "glaze/rpc/registry.hpp"

struct my_api_t
{
   int count{};
   std::string name{"default"};
   std::function<int()> get_count = [this] { return count; };
   std::function<void(int)> set_count = [this](int v) { count = v; };
   std::function<double(std::vector<double>&)> max_value = [](std::vector<double>& vec) {
      return (std::ranges::max)(vec);
   };
};

glz::registry server{};
server.open_rpc_info.title = "My API";
server.open_rpc_info.version = "1.0.0";
server.open_rpc_info.description = "Example service";

my_api_t api{};
server.on(api);

auto spec = server.open_rpc_spec(); // returns glz::open_rpc
auto json = glz::write<glz::opts{.prettify = true}>(spec);
```

This produces:

```json
{
   "openrpc": "1.3.2",
   "info": {
      "title": "My API",
      "version": "1.0.0",
      "description": "Example service"
   },
   "methods": [
      {
         "name": "/count",
         "params": [{ "name": "params", "schema": { "type": ["integer"], ... } }],
         "result": { "name": "result", "schema": { "type": ["integer"], ... } }
      },
      {
         "name": "/get_count",
         "params": [],
         "result": { "name": "result", "schema": { "type": ["integer"], ... } }
      },
      {
         "name": "/set_count",
         "params": [{ "name": "params", "schema": { "type": ["integer"], ... }, "required": true }]
      },
      ...
   ]
}
```

## Serving the Spec via an Endpoint

Call `register_open_rpc()` to add a `/open_rpc` endpoint that serves the spec directly:

```cpp
server.register_open_rpc();
```

This works with both REPE and JSON-RPC protocols:

```cpp
// REPE
glz::registry<> repe_server{};
repe_server.on(api);
repe_server.register_open_rpc();
// Query /open_rpc via REPE to get the spec

// JSON-RPC
glz::registry<glz::opts{}, glz::JSONRPC> jsonrpc_server{};
jsonrpc_server.on(api);
jsonrpc_server.register_open_rpc();
auto response = jsonrpc_server.call(R"({"jsonrpc":"2.0","method":"/open_rpc","id":1})");
// response contains the OpenRPC spec in the "result" field
```

## Method Mapping

Every registered endpoint appears as a method in the spec. JSON Schema descriptions are generated for parameter and result types using Glaze's `write_json_schema` infrastructure.

| Endpoint Type | Params | Result | Params Required |
|---------------|--------|--------|-----------------|
| Variable (`int`, `std::string`, ...) | Type schema | Type schema | No |
| No-param function | Empty | Return type schema | — |
| Function with params | Param type schema | Return type schema | Yes |
| Void function with params | Param type schema | None | Yes |
| Object (nested struct) | Object schema | Object schema | No |
| Root endpoint | Object schema | Object schema | No |

For variables and objects, params are not marked as required because reading (no body) and writing (with body) use the same endpoint path.

## Data Structures

The OpenRPC types are defined in `glaze/rpc/openrpc.hpp`:

```cpp
struct openrpc_info {
   std::string title = "API";
   std::string version = "1.0.0";
   std::optional<std::string> description{};
};

struct openrpc_content_descriptor {
   std::string name{};
   std::optional<std::string> description{};
   raw_json schema{"{}"}; // Pre-serialized JSON Schema
   std::optional<bool> required{};
};

struct openrpc_method {
   std::string name{};
   std::optional<std::string> description{};
   std::vector<openrpc_content_descriptor> params{};
   std::optional<openrpc_content_descriptor> result{};
};

struct open_rpc {
   std::string openrpc = "1.3.2";
   openrpc_info info{};
   std::vector<openrpc_method> methods{};
};
```

## See Also

- [REPE RPC](repe-rpc.md) - REPE protocol and registry
- [JSON-RPC 2.0 Registry](jsonrpc-registry.md) - JSON-RPC protocol registry
- [JSON Schema](../json-schema.md) - Schema generation used for OpenRPC method descriptions
