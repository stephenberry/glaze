# Glaze Language Interoperability Interface

The Glaze interop interface provides a C-compatible API for accessing C++ types and their members from other programming languages. This enables seamless integration of Glaze-based C++ code with languages like Julia, Python, Rust, and others.

## Overview

The interop interface allows:
- **Type Registration**: Register C++ types with their member information
- **Instance Management**: Create and destroy C++ objects from other languages
- **Member Access**: Get/set data members and call member functions
- **Type Descriptors**: Rich type information for proper marshalling
- **Container Support**: Work with vectors, maps, optionals, and other STL containers
- **Async Support**: Handle `std::shared_future` types (Note: `std::future` is not supported as it's move-only)

## Quick Start

### C++ Side

```cpp
#include "glaze/interop/interop.hpp"

// Define your struct
struct Person {
    std::string name;
    int age;
    std::vector<std::string> hobbies;
    
    void greet() const {
        std::cout << "Hello, I'm " << name << "\n";
    }
};

// Define glz::meta
template <>
struct glz::meta<Person> {
    using T = Person;
    static constexpr auto value = object(
        "name", &T::name,
        "age", &T::age,
        "hobbies", &T::hobbies,
        "greet", &T::greet
    );
};

// Register the type
int main() {
    glz::register_type<Person>("Person");
    
    // Create an instance
    Person alice{"Alice", 30, {"reading", "hiking"}};
    glz::register_instance("alice", "Person", alice);
}
```

### From Other Languages (e.g., Julia)

```julia
# Load the shared library
using Libdl
lib = dlopen("libglaze_interop")

# Get type information
type_info = ccall((:glz_get_type_info, lib), Ptr{GlzTypeInfo}, (Cstring,), "Person")

# Access a registered instance
alice_ptr = ccall((:glz_get_instance, lib), Ptr{Cvoid}, (Cstring,), "alice")

# Get type info and find member function
type_info = ccall((:glz_get_type_info, lib), Ptr{GlzTypeInfo}, (Cstring,), "Person")
# Find the greet member
greet_member = find_member(type_info, "greet")
# Call member function
ccall((:glz_call_member_function_with_type, lib), Ptr{Cvoid}, 
      (Ptr{Cvoid}, Cstring, Ptr{GlzMemberInfo}, Ptr{Ptr{Cvoid}}, Ptr{Cvoid}), 
      alice_ptr, "Person", greet_member, C_NULL, C_NULL)
```

## Building

Add to your CMakeLists.txt:

```cmake
option(BUILD_INTEROP "Build language interoperability library" ON)

if(BUILD_INTEROP)
    add_library(glaze_interop SHARED
        src/interop/interop.cpp
    )
    target_link_libraries(glaze_interop PUBLIC glaze::glaze)
endif()
```

## API Reference

### Type Registration

```cpp
template<typename T>
void glz::register_type(std::string_view name);
```
Register a C++ type for interop. The type must have a `glz::meta` specialization.

### Instance Registration

```cpp
template<typename T>
void glz::register_instance(std::string_view instance_name, 
                           std::string_view type_name, 
                           T& instance);
```
Register a global instance that can be accessed from other languages.

### C API Functions

#### Type Information
- `glz_get_type_info(const char* type_name)` - Get type metadata
- `glz_get_type_info_by_hash(size_t type_hash)` - Get type by hash

#### Instance Management
- `glz_create_instance(const char* type_name)` - Create new instance
- `glz_destroy_instance(const char* type_name, void* instance)` - Destroy instance
- `glz_get_instance(const char* instance_name)` - Get registered instance

#### Member Access
- `glz_get_member_ptr(void* instance, const glz_member_info* member)` - Get member pointer
- `glz_call_member_function_with_type(...)` - Call member function

#### Container Operations
- `glz_vector_view(void* vec_ptr, const glz_type_descriptor* type_desc)` - Get vector view
- `glz_vector_resize(...)` - Resize vector
- `glz_vector_push_back(...)` - Add element to vector
- `glz_optional_has_value(...)` - Check if optional has value
- `glz_optional_get_value(...)` - Get optional value
- `glz_optional_set_value(...)` - Set optional value

## Type Descriptors

Type descriptors provide detailed type information for proper marshalling:

```cpp
enum glz_type_kind : uint8_t {
    GLZ_TYPE_PRIMITIVE = 0,  // Fundamental types (int, float, etc.)
    GLZ_TYPE_STRING,         // std::string and std::string_view
    GLZ_TYPE_VECTOR,         // std::vector<T>
    GLZ_TYPE_MAP,           // std::unordered_map<K,V>
    GLZ_TYPE_COMPLEX,       // std::complex<T>
    GLZ_TYPE_STRUCT,        // User-defined structs
    GLZ_TYPE_OPTIONAL,      // std::optional<T>
    GLZ_TYPE_FUNCTION,      // Member functions
    GLZ_TYPE_SHARED_FUTURE  // std::shared_future<T>
};
```

## Member Functions

Member functions are automatically exposed when included in `glz::meta`:

```cpp
struct Calculator {
    int add(int a, int b) { return a + b; }
    std::vector<int> range(int n) {
        std::vector<int> result;
        for (int i = 0; i < n; ++i) result.push_back(i);
        return result;
    }
};

template <>
struct glz::meta<Calculator> {
    using T = Calculator;
    static constexpr auto value = object(
        "add", &T::add,
        "range", &T::range
    );
};
```

## Async Operations

Support for `std::shared_future`:

```cpp
struct AsyncWorker {
    std::shared_future<int> compute_async() {
        return std::async(std::launch::async, []() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return 42;
        }).share();
    }
};

// From other language:
// 1. Call the async function to get a future handle
// 2. Check if ready: glz_shared_future_is_ready()
// 3. Wait if needed: glz_shared_future_wait()
// 4. Get result: glz_shared_future_get()
```

## C++ Client Library

The interop interface includes a C++ client library for consuming Glaze-enabled shared libraries. This enables plugin architectures and dynamic module loading in C++ applications.

### Using the C++ Client

```cpp
#include "glaze/interop/client.hpp"

// Load a shared library with Glaze interop
glz::interop::InteropLibrary lib("./my_plugin.so");

// Get type information
auto type_info = lib.get_type("MyPluginClass");

// Create an instance
auto instance = lib.create_instance("MyPluginClass");

// Access members
instance->set_member("name", std::string("Example"));
int value = instance->get_member<int>("value");

// Call member functions
double result = instance->call_function<double>("calculate", 10.0, 20.0);

// Access global instances
auto global = lib.get_instance("global_instance");
```

### Plugin Architecture Example

```cpp
// Plugin interface (shared by all plugins)
struct PluginInterface {
    std::string name;
    std::string version;
    virtual void execute() = 0;
};

// Load plugins dynamically
class PluginManager {
    std::vector<glz::interop::InteropLibrary> plugins;
    
public:
    void load_plugin(const std::string& path) {
        plugins.emplace_back(path);
        auto& lib = plugins.back();
        
        // Each plugin registers a "Plugin" type
        auto plugin = lib.create_instance("Plugin");
        std::cout << "Loaded: " << plugin->get_member<std::string>("name") << "\n";
    }
};
```

### Building C++ Clients

```cmake
# Client application
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE glaze_interop_client)

# Plugin library
add_library(my_plugin SHARED plugin.cpp)
target_link_libraries(my_plugin PRIVATE glaze_interop)
```

## Performance Considerations

- Type information is cached after first access
- Member function pointers are resolved at registration time
- Minimal overhead for primitive types
- Efficient handling of containers through view structures
- C++ client adds minimal overhead over raw C API

## Thread Safety

- Type registration should be done at startup (not thread-safe)
- Instance access is thread-safe for const operations
- Mutable operations require external synchronization

## Examples

See `examples/interop/` for complete examples including:
- Basic type registration and access
- Container manipulation
- Member function calls
- Async operations
- Complex nested structures

## Language Bindings

Currently available:
- **Julia**: Full support via glaze.jl
- **Python**: (Coming soon)
- **Rust**: (Coming soon)

## Limitations

- Templates must be explicitly instantiated
- Virtual functions are not directly supported
- Move-only types require special handling
- Maximum function arity depends on implementation

## Contributing

Contributions for additional language bindings are welcome! See the existing Julia implementation as a reference.