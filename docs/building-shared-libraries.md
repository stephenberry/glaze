# Building Shared Libraries with Glaze

This guide demonstrates how to create shared libraries (DLLs on Windows, dylibs on macOS, shared objects on Linux) using the Glaze API system.

## Overview

Glaze provides a standardized approach to creating shared library interfaces with:
- Automatic library loading and symbol resolution
- Type-safe access to library functions and data
- Consistent API across all platforms
- Minimal boilerplate code

## Quick Start Example

### Step 1: Define Your API Interface

Create a header file that both your library and client will use:

**interface.hpp**
```c++
#pragma once

#include "glaze/api/impl.hpp"

struct my_api {
  int x = 7;
  double y = 5.5;
  std::vector<double> z = {1.0, 2.0};

  std::function<double(const int&, const double&)> multiply =
    [](const auto& i, const auto& d) { return i * d; };
};

template <>
struct glz::meta<my_api> {
  using T = my_api;
  static constexpr auto value = glz::object(
    &T::x,
    &T::y,
    &T::z,
    &T::multiply
  );

  static constexpr std::string_view name = "my_api";
  static constexpr glz::version_t version{0, 0, 1};
};
```

### Step 2: Create the Shared Library

**my_library.cpp**
```c++
#include "interface.hpp"

// This is the only function you need to export from your library
glz::iface_fn glz_iface() noexcept {
  return glz::make_iface<my_api>();
}
```

That's it! The `glz_iface()` function is automatically exported with the correct calling convention for your platform.

### Step 3: Build the Shared Library with CMake

**CMakeLists.txt for the library**
```cmake
project(my_library)

add_library(${PROJECT_NAME} SHARED my_library.cpp)

# Add debug postfix for debug builds (optional but recommended)
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "_d")

# Link against Glaze (assuming you have it available)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)

# Set output directory
set_target_properties(${PROJECT_NAME}
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

### Step 4: Load and Use the Library

**client.cpp**
```c++
#include "glaze/api/lib.hpp"
#include "interface.hpp"
#include <iostream>

int main() {
  // Load all libraries from a directory
  glz::lib_loader lib("./bin");

  // Get the API instance
  auto io = lib["my_api"]();

  // Access data members
  auto* x = io->get<int>("/x");
  std::cout << "x = " << *x << "\n";  // prints: x = 7

  // Call functions
  auto* multiply = io->get<std::function<double(const int&, const double&)>>("/multiply");
  std::cout << "multiply(3, 4.5) = " << (*multiply)(3, 4.5) << "\n";  // prints: 13.5

  // Modify values
  *x = 42;
  std::cout << "x = " << *x << "\n";  // prints: x = 42

  return 0;
}
```

### Step 5: Build the Client

**CMakeLists.txt for the client**
```cmake
project(client)

add_executable(${PROJECT_NAME} client.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)

# Make sure the client depends on the library being built
add_dependencies(${PROJECT_NAME} my_library)
```

## Platform-Specific Details

### Export Macro

Glaze automatically defines `DLL_EXPORT` with the correct platform-specific attributes:

```c++
// Defined in glaze/api/api.hpp
#if defined(_WIN32) || defined(__CYGWIN__)
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

extern "C" DLL_EXPORT glz::iface_fn glz_iface() noexcept;
```

### Library Extensions

Glaze handles platform-specific library extensions automatically:
- **Windows**: `.dll`
- **macOS**: `.dylib`
- **Linux**: `.so`

### Library Naming

The `lib_loader` follows platform conventions:
- **Windows**: `my_library.dll` or `my_library_d.dll` (debug)
- **macOS/Linux**: `libmy_library.dylib/so` or `libmy_library_d.dylib/so` (debug)

When loading by name (without extension), Glaze automatically adds the correct prefix and extension:

```c++
// Loads "libmy_library.dylib" on macOS, "my_library.dll" on Windows
lib_loader.load("my_library");
```

## Loading Libraries

### Load from Directory

Load all shared libraries from a directory:

```c++
glz::lib_loader lib("/path/to/libraries");

// Access any API by name
auto api1 = lib["my_api"]();
auto api2 = lib["another_api"]();
```

### Load Single Library

Load a specific library file:

```c++
glz::lib_loader lib;
lib.load("/path/to/my_library.dylib");

auto api = lib["my_api"]();
```

### Load by Name

Load a library by name (Glaze adds the platform-specific prefix/extension):

```c++
glz::lib_loader lib;
lib.load("my_library");  // Automatically becomes "libmy_library.dylib" on macOS

auto api = lib["my_api"]();
```

## Multiple APIs in One Library

You can expose multiple API types from a single shared library:

**library.cpp**
```c++
#include "glaze/api/impl.hpp"

struct math_api {
  double add(double a, double b) { return a + b; }
  double multiply(double a, double b) { return a * b; }
};

template <>
struct glz::meta<math_api> {
  using T = math_api;
  static constexpr auto value = glz::object(
    &T::add,
    &T::multiply
  );
  static constexpr std::string_view name = "math_api";
};

struct string_api {
  std::string concat(const std::string& a, const std::string& b) {
    return a + b;
  }
};

template <>
struct glz::meta<string_api> {
  using T = string_api;
  static constexpr auto value = glz::object(
    &T::concat
  );
  static constexpr std::string_view name = "string_api";
};

// Export both APIs from this library
glz::iface_fn glz_iface() noexcept {
  return glz::make_iface<math_api, string_api>();
}
```

**Usage:**
```c++
glz::lib_loader lib("./bin");

auto math = lib["math_api"]();
auto str = lib["string_api"]();

auto result1 = math->call<double>("/add", 3.0, 4.0);
auto result2 = str->call<std::string>("/concat", "Hello", "World");
```

## Library Lifecycle

### Constructor and Destructor

The `lib_loader` manages library lifecycle automatically:

```c++
{
  glz::lib_loader lib("/path/to/libs");

  auto api = lib["my_api"]();
  // Use the API...

} // Libraries are unloaded here when lib_loader is destroyed
```

### Manual Loading

```c++
glz::lib_loader lib;

// Load libraries on demand
lib.load("library1");
lib.load("library2");

// Use the APIs
auto api1 = lib["api1"]();
auto api2 = lib["api2"]();

// Libraries remain loaded until lib_loader is destroyed
```

## Debug vs Release Builds

The `lib_loader` automatically handles debug/release library naming:

- In **Debug** builds: looks for `library_d.dll` / `liblibrary_d.dylib`
- In **Release** builds: looks for `library.dll` / `liblibrary.dylib`

This is controlled by the `NDEBUG` macro:

```c++
// From glaze/api/lib.hpp
#ifdef NDEBUG
static std::string suffix = "";
#else
static std::string suffix = "_d";
#endif
```

Configure this in CMake:

```cmake
set_target_properties(my_library PROPERTIES DEBUG_POSTFIX "_d")
```

## Error Handling

### Library Loading Errors

```c++
glz::lib_loader lib;
lib.load("/path/to/library.dylib");

// Check if the API is available
if (lib["my_api"]) {
  auto api = lib["my_api"]();
  // Use the API
} else {
  std::cerr << "Failed to load my_api\n";
}
```

### Runtime Errors

```c++
auto api = lib["my_api"]();

// Check for errors when accessing members
auto* x = api->get<int>("/x");
if (!x) {
  std::cerr << "Error: " << api->last_error() << "\n";
}

// Check for errors when calling functions
auto result = api->call<int>("/func");
if (!result) {
  std::cerr << "Error: " << api->last_error() << "\n";
}
```

## Best Practices

### 1. Use Shared Headers

Define your API interfaces in headers shared between the library and client. This ensures type consistency and enables compile-time checks.

### 2. Version Your APIs

Always specify a version for your API types:

```c++
template <>
struct glz::meta<my_api> {
  static constexpr glz::version_t version{1, 0, 0};  // major, minor, patch
  // ...
};
```

Increment versions when making incompatible changes:
- **Major**: Breaking changes (change struct layout, remove members)
- **Minor**: Backward-compatible additions (add new members)
- **Patch**: Bug fixes (no API changes)

### 3. Use Descriptive Names

Give your APIs clear, descriptive names that indicate their purpose:

```c++
template <>
struct glz::meta<database_api> {
  static constexpr std::string_view name = "database_api";
};
```

### 4. Group Related Functionality

Create separate API types for different concerns:

```c++
// Good: Separate APIs for different subsystems
glz::make_iface<rendering_api, physics_api, audio_api>()

// Less ideal: One monolithic API for everything
glz::make_iface<game_engine_api>()
```

### 5. Handle Null Pointers

Always check return values from `get()`:

```c++
auto* value = api->get<int>("/x");
if (value) {
  // Safe to use
  std::cout << *value << "\n";
}
```

### 6. Use expected for Error Handling

Leverage the `expected` return type from `call()` and `get_fn()`:

```c++
auto result = api->call<int>("/func");
if (result) {
  int value = result.value();
} else {
  // Handle error
  std::cerr << api->last_error() << "\n";
}
```

## Complete Example

See the `tests/lib_test` directory in the Glaze repository for a complete working example with CMake configuration.

**Directory structure:**
```
tests/lib_test/
├── CMakeLists.txt           # Client test executable
├── lib_test.cpp             # Client code that loads the library
├── interface.hpp            # Shared API definition
└── test_lib/
    ├── CMakeLists.txt       # Library build configuration
    └── test_lib.cpp         # Library implementation
```

This example demonstrates:
- Building a shared library with Glaze
- Loading the library from a client application
- Type-safe access across the library boundary
- Proper CMake configuration for both library and client
