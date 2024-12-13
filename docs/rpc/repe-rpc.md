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

