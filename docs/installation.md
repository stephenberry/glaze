# Glaze Installation Guide

This guide covers some of the ways to install and integrate the Glaze JSON library into your C++ project. There are lots of packaged versions of Glaze, from [homebrew](https://formulae.brew.sh/formula/glaze) to [Conan](https://conan.io/center/recipes/glaze).

## System Requirements

### Compiler Support
- **C++23** standard required
- **Clang 17+**
- **GCC 12+** 
- **MSVC 2022+**
- **Apple Clang (latest Xcode)**

### Platform Support
- **Architecture**: 64-bit and 32-bit
- **Endianness**: Little-endian systems only
- **Operating Systems**: Windows, Linux, macOS

### MSVC Specific Requirements
When using MSVC, you **must** use the `/Zc:preprocessor` flag for C++ standard conformant preprocessing:

```bash
cl /Zc:preprocessor your_source.cpp
```

## Installation Methods

### 1. CMake FetchContent (Recommended)

Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glaze)

target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)
```

#### Using a Specific Version

For production use, it's recommended to pin to a specific version:

```cmake
FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG v5.0.0  # Replace with desired version
  GIT_SHALLOW TRUE
)
```

### 2. Conan Package Manager

Glaze is available in [Conan Center](https://conan.io/center/recipes/glaze).

#### conanfile.txt
```ini
[requires]
glaze/[>=5.0.0]

[generators]
CMakeDeps
CMakeToolchain
```

#### CMakeLists.txt
```cmake
find_package(glaze REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)
```

#### Command Line Installation
```bash
conan install . --build=missing
cmake --preset conan-default
cmake --build --preset conan-release
```

### 3. build2 Package Manager

Glaze is available on [cppget](https://cppget.org/libglaze).

#### manifest
```
depends: libglaze ^5.0.0
```

#### buildfile
```
import libs = libglaze%lib{glaze}
exe{myapp}: cxx{main} $libs
```

### 4. Linux Package Managers

#### Arch Linux
```bash
# Official repository
sudo pacman -S glaze

# Or AUR development version
yay -S glaze-git
```

#### Ubuntu/Debian (Manual)
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake git

# Clone and install
git clone https://github.com/stephenberry/glaze.git
cd glaze
mkdir build && cd build
cmake ..
sudo make install
```

### 5. Manual Installation

#### Download and Extract
```bash
# Download latest release
wget https://github.com/stephenberry/glaze/archive/refs/tags/v5.0.0.tar.gz
tar -xzf v5.0.0.tar.gz
cd glaze-5.0.0
```

#### Header-Only Integration
Since Glaze is header-only, you can simply:

1. Copy the `include/` directory to your project
2. Add the include path to your compiler flags:
   ```bash
   g++ -I/path/to/glaze/include your_source.cpp
   ```

## CMake Configuration Options

### SIMD Support

Glaze automatically detects the target architecture and enables platform-specific SIMD optimizations:

| Flag | Detected When | Architecture |
|------|--------------|--------------|
| `GLZ_USE_SSE2` | `__x86_64__` or `_M_X64` | x86-64 (always has SSE2) |
| `GLZ_USE_AVX2` | `__AVX2__` (in addition to x86-64) | x86-64 with AVX2 |
| `GLZ_USE_NEON` | `__aarch64__`, `_M_ARM64`, or `__ARM_NEON` | ARM64 / AArch64 |

Detection uses compiler-predefined macros that reflect the target architecture, so cross-compilation works correctly without any manual configuration.

### Disable SIMD

To disable all SIMD intrinsics (e.g., when benchmarking SWAR-only performance or working around a platform issue):

```cmake
set(glaze_DISABLE_SIMD_WHEN_SUPPORTED ON)
FetchContent_MakeAvailable(glaze)
```

This sets `GLZ_DISABLE_SIMD` as an INTERFACE compile definition, so it automatically propagates to all targets linking against `glaze::glaze`.

Or define the macro directly before including Glaze headers:
```cpp
#define GLZ_DISABLE_SIMD
#include "glaze/glaze.hpp"
```

### Disable Forced Inlining

By default, Glaze uses compiler-specific attributes (`__attribute__((always_inline))` on GCC/Clang, `[[msvc::forceinline]]` on MSVC) to force inlining of performance-critical functions. This maximizes runtime performance but increases compilation time.

To disable forced inlining:

```cmake
set(glaze_DISABLE_ALWAYS_INLINE ON)
FetchContent_MakeAvailable(glaze)
```

Or define the macro directly before including Glaze headers:
```cpp
#define GLZ_DISABLE_ALWAYS_INLINE
#include "glaze/glaze.hpp"
```

When disabled, `GLZ_ALWAYS_INLINE` and `GLZ_FLATTEN` fall back to regular `inline` hints, allowing the compiler to make its own inlining decisions. This is useful when:
- Compilation time is a priority
- You're building for debug or development purposes
- Peak runtime performance is not critical

> [!NOTE]
> This option primarily reduces **compilation time**, not binary size. Modern compilers typically inline hot paths anyway using their own heuristics, so binary size reduction is often minimal. For significant binary size reduction, use the `linear_search` option instead (see [Optimizing Performance](optimizing-performance.md)).

## Optional Dependencies

The core Glaze library (JSON, BEVE, CSV, TOML serialization) is **header-only with no external dependencies**.

### Networking Features

For HTTP server/client, WebSocket, and RPC features, you need:

- **ASIO** - Either [standalone ASIO](https://think-async.com/Asio/) or Boost.Asio
- **OpenSSL** (optional) - For HTTPS and secure WebSocket (wss://) support

See the **[ASIO Setup Guide](networking/asio-setup.md)** for detailed instructions on:
- Installing and configuring ASIO
- CMake integration
- SSL/TLS setup
- Platform-specific configuration

## Example Project Setup

### Complete CMakeLists.txt Example
```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGlazeProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glaze)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE glaze::glaze)

# MSVC specific flag
if(MSVC)
    target_compile_options(myapp PRIVATE /Zc:preprocessor)
endif()
```

### Getting Help

- **Documentation**: [GitHub Docs](https://github.com/stephenberry/glaze/tree/main/docs)
- **Issues**: [GitHub Issues](https://github.com/stephenberry/glaze/issues)
- **Example Repository**: [Glaze Example](https://github.com/stephenberry/glaze_example)

## Next Steps

After installation, check out:
- [Basic Usage Examples](https://github.com/stephenberry/glaze/blob/main/tests/example_json/example_json.cpp)
