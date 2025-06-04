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

### AVX2 Support
Enable AVX2 SIMD instructions for better performance (if your target supports it):

```cmake
set(glaze_ENABLE_AVX2 ON)
FetchContent_MakeAvailable(glaze)
```

Or define the macro directly:
```cpp
#define GLZ_USE_AVX2
```

### Disable AVX2 (Cross-compilation)
For cross-compilation to ARM or other architectures:

```cmake
set(glaze_ENABLE_AVX2 OFF)
```

## Example Project Setup

### Complete CMakeLists.txt Example
```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGlazeProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable AVX2 if building for x86_64
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(glaze_ENABLE_AVX2 ON)
endif()

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
