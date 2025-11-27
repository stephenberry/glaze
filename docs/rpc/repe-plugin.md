# REPE Plugin Interface

The REPE plugin interface provides a standardized way to build ABI-stable dynamic plugins that integrate with `glz::registry` and `glz::asio_server`. This enables plugin-based RPC systems where plugins can be compiled separately from the host application.

## Motivation

Building plugin-based RPC systems requires:

1. **ABI stability** - Plugins compiled with different compilers/versions must interoperate
2. **Simple C interface** - Avoids C++ ABI issues across shared library boundaries
3. **Integration with glz::registry** - Leverage existing REPE infrastructure
4. **Thread safety** - Support concurrent calls from server handlers
5. **Lifecycle management** - Clean initialization and shutdown of plugin resources

## Headers

```cpp
// Pure C interface (no Glaze dependencies)
#include "glaze/rpc/repe/plugin.h"

// C++ helper for implementing plugins with glz::registry
#include "glaze/rpc/repe/plugin_helper.hpp"
```

## C Interface (`plugin.h`)

The C header defines the ABI-stable plugin contract. It has no C++ or Glaze dependencies and can be used by both plugin implementations and host applications.

### Interface Version

```c
#define REPE_PLUGIN_INTERFACE_VERSION 1
```

Plugins and hosts should check version compatibility before use. When the plugin interface changes, this version is incremented.

### Types

```c
// ABI-stable buffer for request/response data
typedef struct {
    const char* data;
    uint64_t size;
} repe_buffer;

// Result codes for plugin operations
typedef enum {
    REPE_OK = 0,
    REPE_ERROR_INIT_FAILED = 1,
    REPE_ERROR_INVALID_CONFIG = 2,
    REPE_ERROR_ALREADY_INITIALIZED = 3
} repe_result;
```

### Required Plugin Exports

Plugins must export these symbols with C linkage:

```c
// Interface version for compatibility checking
uint32_t repe_plugin_interface_version(void);

// Plugin identification
const char* repe_plugin_name(void);
const char* repe_plugin_version(void);
const char* repe_plugin_root_path(void);

// Request processing
repe_buffer repe_plugin_call(const char* request, uint64_t request_size);
```

### Optional Plugin Exports

These may be NULL if not needed:

```c
// Initialize plugin with optional configuration
repe_result repe_plugin_init(const char* config, uint64_t config_size);

// Cleanup resources before unload
void repe_plugin_shutdown(void);
```

## C++ Helper (`plugin_helper.hpp`)

The C++ helper provides convenient functions for implementing plugins using `glz::registry`.

### Thread-Local Response Buffer

```cpp
namespace glz::repe {
    // Thread-local buffer for plugin responses
    // Grows as needed but does not shrink during thread lifetime
    inline thread_local std::string plugin_response_buffer;
}
```

### `plugin_error_response`

Creates a properly formatted REPE error response:

```cpp
namespace glz::repe {
    void plugin_error_response(
        error_code ec,
        const std::string& error_msg,
        uint64_t id = 0
    );
}
```

**Parameters:**
- `ec` - The error code to set in the response
- `error_msg` - Human-readable error message for the body
- `id` - Request ID to echo back (default: 0)

The response is written to `plugin_response_buffer`.

### `plugin_call`

Template function that handles the complete request/response cycle:

```cpp
namespace glz::repe {
    template <typename InitFunc, typename Registry>
    repe_buffer plugin_call(
        InitFunc&& init_func,
        Registry& registry,
        const char* request,
        uint64_t request_size
    );
}
```

**Parameters:**
- `init_func` - Callable for lazy initialization (should use `std::call_once` internally)
- `registry` - The `glz::registry<>` to dispatch calls to
- `request` - Raw REPE request bytes
- `request_size` - Size of request data

**Returns:** `repe_buffer` pointing to `plugin_response_buffer`

**Error Handling:**
- Init function exceptions → `error_code::invalid_call`
- Deserialization failures → `error_code::parse_error`
- Registry call exceptions → `error_code::invalid_call` (ID preserved)
- Unknown exceptions → `error_code::invalid_call`

## Plugin Implementation Example

```cpp
#include <glaze/rpc/repe/plugin_helper.hpp>
#include <mutex>

// Define your API struct
// Note: glz::registry supports functions with 0 or 1 parameter
struct calculator_api {
    double value = 0.0;
    double get_value() { return value; }
    void set_value(double v) { value = v; }
    double increment() { return ++value; }
};

template <>
struct glz::meta<calculator_api> {
    using T = calculator_api;
    static constexpr auto value = object(
        &T::value, &T::get_value, &T::set_value, &T::increment
    );
};

namespace {
    calculator_api api_instance;
    glz::registry<> internal_registry;
    std::once_flag init_flag;

    void ensure_initialized() {
        std::call_once(init_flag, []() {
            internal_registry.on<glz::root<"/calculator">>(api_instance);
        });
    }
}

// Plugin exports with C linkage
extern "C" {
    // Required: Interface version for ABI compatibility
    uint32_t repe_plugin_interface_version() {
        return REPE_PLUGIN_INTERFACE_VERSION;
    }

    // Required: Plugin identification
    const char* repe_plugin_name() { return "calculator"; }
    const char* repe_plugin_version() { return "1.0.0"; }
    const char* repe_plugin_root_path() { return "/calculator"; }

    // Optional: Explicit initialization with configuration
    repe_result repe_plugin_init(const char* /*config*/, uint64_t /*config_size*/) {
        try {
            ensure_initialized();
            return REPE_OK;
        }
        catch (...) {
            return REPE_ERROR_INIT_FAILED;
        }
    }

    // Optional: Cleanup resources on unload
    void repe_plugin_shutdown() {
        // Release any held resources here
    }

    // Required: Request processing
    repe_buffer repe_plugin_call(const char* request, uint64_t request_size) {
        return glz::repe::plugin_call(
            ensure_initialized, internal_registry, request, request_size
        );
    }
}
```

## Host Integration Example

### Loading Plugins (POSIX)

```cpp
#include <glaze/rpc/repe/plugin.h>
#include <dlfcn.h>
#include <optional>
#include <string>

struct loaded_plugin {
    std::string name;
    std::string root_path;
    void* handle = nullptr;

    // Function pointers
    uint32_t (*interface_version_fn)(void) = nullptr;
    repe_result (*init_fn)(const char*, uint64_t) = nullptr;
    void (*shutdown_fn)(void) = nullptr;
    repe_buffer (*call_fn)(const char*, uint64_t) = nullptr;

    ~loaded_plugin() {
        if (handle) {
            if (shutdown_fn) shutdown_fn();
            dlclose(handle);
        }
    }

    repe_buffer call(const char* req, uint64_t size) const {
        return call_fn(req, size);
    }
};

std::optional<loaded_plugin> load_plugin(const std::string& path) {
    loaded_plugin plugin;

    plugin.handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!plugin.handle) {
        return std::nullopt;
    }

    // Load required symbols
    plugin.interface_version_fn =
        (uint32_t(*)(void))dlsym(plugin.handle, "repe_plugin_interface_version");
    auto name_fn =
        (const char*(*)(void))dlsym(plugin.handle, "repe_plugin_name");
    auto root_fn =
        (const char*(*)(void))dlsym(plugin.handle, "repe_plugin_root_path");
    plugin.call_fn =
        (repe_buffer(*)(const char*, uint64_t))dlsym(plugin.handle, "repe_plugin_call");

    // Load optional symbols (may be NULL)
    plugin.init_fn =
        (repe_result(*)(const char*, uint64_t))dlsym(plugin.handle, "repe_plugin_init");
    plugin.shutdown_fn =
        (void(*)(void))dlsym(plugin.handle, "repe_plugin_shutdown");

    // Validate required symbols
    if (!plugin.interface_version_fn || !name_fn || !root_fn || !plugin.call_fn) {
        return std::nullopt;
    }

    // Check interface version compatibility
    if (plugin.interface_version_fn() != REPE_PLUGIN_INTERFACE_VERSION) {
        return std::nullopt;
    }

    plugin.name = name_fn();
    plugin.root_path = root_fn();

    // Initialize if init function is provided
    if (plugin.init_fn) {
        if (plugin.init_fn(nullptr, 0) != REPE_OK) {
            return std::nullopt;
        }
    }

    return plugin;
}
```

### Server Integration with `glz::asio_server`

```cpp
#include <glaze/ext/glaze_asio.hpp>
#include <glaze/rpc/repe/plugin.h>

int main() {
    std::vector<loaded_plugin> plugins;

    // Load plugins
    if (auto plugin = load_plugin("./plugins/libcalculator.so")) {
        plugins.push_back(std::move(*plugin));
    }

    glz::asio_server server{};
    server.port = 8080;

    // Custom call handler routes to plugins
    server.call = [&](glz::repe::message& request, glz::repe::message& response) {
        for (const auto& plugin : plugins) {
            if (request.query.starts_with(plugin.root_path)) {
                std::string buffer;
                glz::repe::to_buffer(request, buffer);

                auto result = plugin.call(buffer.data(), buffer.size());

                auto ec = glz::repe::from_buffer(result.data, result.size, response);
                if (ec != glz::error_code::none) {
                    glz::repe::encode_error(ec, response, "Failed to parse plugin response");
                    glz::repe::finalize_header(response);
                }
                return;
            }
        }

        glz::repe::encode_error(
            glz::repe::error_code::method_not_found, response,
            "No plugin registered for path"
        );
        glz::repe::finalize_header(response);
    };

    server.run();
}
```

## Thread Safety

### Thread-Local Buffer

The `plugin_response_buffer` is `thread_local`, meaning:

- Each thread has its own independent buffer
- Concurrent calls from different threads are safe
- The buffer is valid until the next call to `plugin_call` or `plugin_error_response` **on the same thread**

> [!WARNING]
> Do not store the returned `repe_buffer` pointer for later use. The memory will be overwritten by subsequent calls on the same thread.

### Registry Thread Safety

The `glz::registry` itself does not provide internal locking. If your plugin state can be accessed concurrently:

- Use `std::atomic` for simple values
- Use `glz::async_string` for strings
- Implement your own synchronization for complex state

See [REPE RPC](repe-rpc.md) for more details on thread-safe classes.

## Platform Considerations

### Shared Library Loading

| Platform    | Load           | Symbol Lookup    | Unload         |
|-------------|----------------|------------------|----------------|
| Linux/macOS | `dlopen()`     | `dlsym()`        | `dlclose()`    |
| Windows     | `LoadLibrary()`| `GetProcAddress()`| `FreeLibrary()`|

### Shared Library Naming

| Platform | Convention       | Example                |
|----------|------------------|------------------------|
| Linux    | `lib<name>.so`   | `libcalculator.so`     |
| macOS    | `lib<name>.dylib`| `libcalculator.dylib`  |
| Windows  | `<name>.dll`     | `calculator.dll`       |

## API Reference

### C Interface (`plugin.h`)

| Symbol | Required | Description |
|--------|----------|-------------|
| `REPE_PLUGIN_INTERFACE_VERSION` | - | Macro defining current interface version (1) |
| `repe_buffer` | - | POD struct: `{const char* data, uint64_t size}` |
| `repe_result` | - | Enum: `REPE_OK`, `REPE_ERROR_*` |
| `repe_plugin_interface_version()` | Yes | Returns interface version |
| `repe_plugin_name()` | Yes | Returns plugin name |
| `repe_plugin_version()` | Yes | Returns plugin version string |
| `repe_plugin_root_path()` | Yes | Returns root path for routing |
| `repe_plugin_init()` | No | Initialize with optional config |
| `repe_plugin_shutdown()` | No | Cleanup before unload |
| `repe_plugin_call()` | Yes | Process REPE request |

### C++ Helper (`plugin_helper.hpp`)

| Symbol | Description |
|--------|-------------|
| `glz::repe::plugin_response_buffer` | Thread-local response buffer |
| `glz::repe::plugin_error_response()` | Create formatted REPE error |
| `glz::repe::plugin_call()` | Complete request handling |

## Compatibility

- **C Standard:** C99 (for `plugin.h`)
- **C++ Standard:** C++23 (for `plugin_helper.hpp`, same as Glaze)
- **Platforms:** Linux, macOS, Windows
- **Dependencies:** None for `plugin.h`; Glaze headers for `plugin_helper.hpp`
