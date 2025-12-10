# ASIO Setup for Glaze Networking

Glaze's networking features (HTTP server/client, WebSocket client, REPE RPC) require [ASIO](https://think-async.com/Asio/) for async I/O. This guide covers how to configure ASIO for use with Glaze.

## Which Features Require ASIO?

| Feature | Header | ASIO Required |
|---------|--------|---------------|
| JSON/BEVE serialization | `glaze/glaze.hpp` | No |
| CSV/TOML parsing | `glaze/csv.hpp`, `glaze/toml.hpp` | No |
| HTTP Server | `glaze/net/http_server.hpp` | Yes |
| HTTP Client | `glaze/net/http_client.hpp` | Yes |
| WebSocket Client | `glaze/net/websocket_client.hpp` | Yes |
| REPE RPC (asio_server/client) | `glaze/ext/glaze_asio.hpp` | Yes |

The core Glaze serialization library has **no external dependencies**. ASIO is only required for networking features.

## ASIO Detection

Glaze automatically detects which ASIO implementation is available using `__has_include`:

1. **Standalone ASIO** (`<asio.hpp>`) - Preferred, used by default
2. **Boost.Asio** (`<boost/asio.hpp>`) - Used as fallback if standalone not found

No configuration is needed if ASIO headers are in your include path - Glaze will find them automatically.

## Installation Methods

### Option 1: Standalone ASIO (Recommended)

Standalone ASIO is a header-only library with no dependencies (except OpenSSL for TLS).

#### Via Package Manager

```bash
# macOS (Homebrew)
brew install asio

# Ubuntu/Debian
sudo apt-get install libasio-dev

# Arch Linux
sudo pacman -S asio

# vcpkg
vcpkg install asio

# Conan
conan install asio/1.28.0@
```

#### Via CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-36-0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

# Add include path
target_include_directories(your_target PRIVATE ${asio_SOURCE_DIR}/asio/include)
```

#### Manual Download

Download from [think-async.com](https://think-async.com/Asio/) and add the `include` directory to your compiler's include path.

### Option 2: Boost.Asio

If you're already using Boost in your project, you can use Boost.Asio instead.

#### Via Package Manager

```bash
# macOS (Homebrew)
brew install boost

# Ubuntu/Debian
sudo apt-get install libboost-system-dev

# Arch Linux
sudo pacman -S boost

# vcpkg
vcpkg install boost-asio

# Conan
conan install boost/1.83.0@
```

#### CMake Integration

```cmake
find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(your_target PRIVATE Boost::system)
```

## CMake Configuration

### Complete Example with ASIO

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyNetworkingApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch Glaze
FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glaze)

# Fetch standalone ASIO
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-36-0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE glaze::glaze)
target_include_directories(myapp PRIVATE ${asio_SOURCE_DIR}/asio/include)

# MSVC specific
if(MSVC)
    target_compile_options(myapp PRIVATE /Zc:preprocessor)
endif()
```

### Using System ASIO

```cmake
# Try to find system ASIO
find_package(asio CONFIG QUIET)

if(asio_FOUND)
    target_link_libraries(myapp PRIVATE asio::asio)
else()
    # Fallback: try pkg-config or manual path
    find_path(ASIO_INCLUDE_DIR asio.hpp)
    if(ASIO_INCLUDE_DIR)
        target_include_directories(myapp PRIVATE ${ASIO_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "ASIO not found. Install it or use FetchContent.")
    endif()
endif()
```

### Using Boost.Asio

```cmake
find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(myapp PRIVATE glaze::glaze Boost::system)
```

## Configuration Options

### Force Boost.Asio

If both standalone ASIO and Boost.Asio are available, Glaze prefers standalone ASIO. To force Boost.Asio:

```cpp
#define GLZ_USE_BOOST_ASIO
#include "glaze/net/http_server.hpp"
```

Or via CMake:

```cmake
target_compile_definitions(myapp PRIVATE GLZ_USE_BOOST_ASIO)
```

### Detecting Which ASIO is Used

After including Glaze networking headers, you can check which implementation is active:

```cpp
#include "glaze/ext/glaze_asio.hpp"

#ifdef GLZ_USING_BOOST_ASIO
    // Using Boost.Asio
#else
    // Using standalone ASIO
#endif
```

## SSL/TLS Support

For HTTPS and secure WebSocket connections (wss://), you need OpenSSL.

### Installing OpenSSL

```bash
# macOS (Homebrew)
brew install openssl

# Ubuntu/Debian
sudo apt-get install libssl-dev

# Arch Linux
sudo pacman -S openssl

# vcpkg
vcpkg install openssl
```

### CMake with OpenSSL

```cmake
find_package(OpenSSL REQUIRED)
target_link_libraries(myapp PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Enable SSL in Glaze
target_compile_definitions(myapp PRIVATE GLZ_ENABLE_SSL)
```

### macOS OpenSSL Path

On macOS with Homebrew, you may need to specify the OpenSSL path:

```cmake
if(APPLE)
    execute_process(
        COMMAND brew --prefix openssl
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR})
endif()

find_package(OpenSSL REQUIRED)
```

## Platform-Specific Notes

### Windows

On Windows, ASIO requires linking against `ws2_32` (Winsock):

```cmake
if(WIN32)
    target_link_libraries(myapp PRIVATE ws2_32)
endif()
```

With MSVC, you also need the conformant preprocessor:

```cmake
if(MSVC)
    target_compile_options(myapp PRIVATE /Zc:preprocessor)
endif()
```

### macOS

No special configuration needed beyond OpenSSL path for TLS support.

### Linux

No special configuration needed. Ensure `pthread` is linked if using threads:

```cmake
find_package(Threads REQUIRED)
target_link_libraries(myapp PRIVATE Threads::Threads)
```

## Troubleshooting

### "asio.hpp not found"

ASIO headers are not in your include path. Either:
- Install ASIO via your package manager
- Add the ASIO include directory to your build
- Use FetchContent to download ASIO

### "standalone or boost asio must be included"

This static assertion fires when neither ASIO implementation is found. Ensure ASIO is properly installed and the include path is set.

### Linker errors with Boost.Asio

Boost.Asio requires linking against `Boost::system`:

```cmake
find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(myapp PRIVATE Boost::system)
```

### SSL handshake failures

Ensure OpenSSL is properly installed and linked. On macOS, verify you're using Homebrew's OpenSSL, not the system LibreSSL.

### "undefined reference to pthread" on Linux

Add threading support:

```cmake
find_package(Threads REQUIRED)
target_link_libraries(myapp PRIVATE Threads::Threads)
```

## Complete Working Example

```cpp
// main.cpp
#include "glaze/net/http_server.hpp"
#include <iostream>

int main() {
    glz::http_server server;

    server.get("/hello", [](const glz::request&, glz::response& res) {
        res.json({{"message", "Hello from Glaze!"}});
    });

    server.bind("127.0.0.1", 8080).with_signals();

    std::cout << "Server running on http://127.0.0.1:8080\n";

    server.start();
    server.wait_for_signal();

    return 0;
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(hello_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-36-0
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glaze asio)

add_executable(hello_server main.cpp)
target_link_libraries(hello_server PRIVATE glaze::glaze)
target_include_directories(hello_server PRIVATE ${asio_SOURCE_DIR}/asio/include)

if(WIN32)
    target_link_libraries(hello_server PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(hello_server PRIVATE /Zc:preprocessor)
endif()
```

## Next Steps

- [HTTP Server Guide](http-server.md) - Server configuration and routing
- [WebSocket Client Guide](websocket-client.md) - Real-time communication
- [REPE RPC Guide](../rpc/repe-rpc.md) - High-performance RPC
