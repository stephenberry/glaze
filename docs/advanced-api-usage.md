# Advanced Glaze API Usage

This guide covers advanced patterns and techniques for using the Glaze API system.

## Table of Contents
- [Creating Custom API Implementations](#creating-custom-api-implementations)
- [JSON Pointer Navigation](#json-pointer-navigation)
- [Working with Complex Types](#working-with-complex-types)
- [Function Signatures and Type Handling](#function-signatures-and-type-handling)
- [Performance Optimization](#performance-optimization)
- [Type Hashing Deep Dive](#type-hashing-deep-dive)
- [Cross-Compilation Safety](#cross-compilation-safety)

## Creating Custom API Implementations

While `glz::impl<T>` provides the standard implementation, you can create custom API implementations by inheriting from `glz::api`:

```c++
#include "glaze/api/api.hpp"

struct custom_api : glz::api {
  // Your custom data
  std::map<std::string, int> data;

  // Override the virtual methods
  std::pair<void*, glz::hash_t> get(const sv path) noexcept override {
    // Custom implementation
    if (path == "/count") {
      static constexpr auto hash = glz::hash<int>();
      return {&data["count"], hash};
    }
    return {nullptr, {}};
  }

  bool contains(const sv path) noexcept override {
    return path == "/count";
  }

  bool read(const uint32_t format, const sv path, const sv data) noexcept override {
    // Custom deserialization logic
    return true;
  }

  bool write(const uint32_t format, const sv path, std::string& data) noexcept override {
    // Custom serialization logic
    return true;
  }

protected:
  bool caller(const sv path, const glz::hash_t type_hash, void*& ret,
              std::span<void*> args) noexcept override {
    // Custom function calling logic
    return false;
  }

  std::unique_ptr<void, void (*)(void*)> get_fn(const sv path,
                                                 const glz::hash_t type_hash) noexcept override {
    // Custom function retrieval logic
    return {nullptr, nullptr};
  }
};
```

## JSON Pointer Navigation

The API uses [JSON Pointer (RFC 6901)](https://tools.ietf.org/html/rfc6901) syntax for navigating data structures:

### Basic Paths

```c++
struct config {
  int port = 8080;
  std::string host = "localhost";
  std::vector<std::string> endpoints = {"api", "health", "metrics"};
};

auto* port = api->get<int>("/port");
auto* host = api->get<std::string>("/host");
```

### Nested Objects

```c++
struct database {
  std::string connection_string;
  int max_connections = 10;
};

struct app_config {
  database db;
  int port = 8080;
};

template <>
struct glz::meta<database> {
  using T = database;
  static constexpr auto value = glz::object(
    &T::connection_string,
    &T::max_connections
  );
  static constexpr std::string_view name = "database";
};

template <>
struct glz::meta<app_config> {
  using T = app_config;
  static constexpr auto value = glz::object(
    &T::db,
    &T::port
  );
  static constexpr std::string_view name = "app_config";
};

// Navigate to nested members
auto* conn_str = api->get<std::string>("/db/connection_string");
auto* max_conn = api->get<int>("/db/max_connections");
```

### Array Access

```c++
struct config {
  std::vector<int> ports = {8080, 8081, 8082};
};

// Access array elements by index
auto* first_port = api->get<int>("/ports/0");
auto* second_port = api->get<int>("/ports/1");
```

### Escaping Special Characters

If your keys contain special characters like `/` or `~`, they must be escaped:
- `~0` represents `~`
- `~1` represents `/`

```c++
// A key named "a/b" would be accessed as "/a~1b"
// A key named "m~n" would be accessed as "/m~0n"
```

## Working with Complex Types

### Nested Smart Pointers

```c++
struct data {
  std::unique_ptr<std::shared_ptr<int>> value =
    std::make_unique<std::shared_ptr<int>>(std::make_shared<int>(42));
};

// Glaze automatically unwraps nested smart pointers
auto* val = api->get<int>("/value");  // Returns int*, not unique_ptr<shared_ptr<int>>*
```

### Optional Values

```c++
struct config {
  std::optional<std::string> api_key;
  std::optional<int> timeout;
};

template <>
struct glz::meta<config> {
  using T = config;
  static constexpr auto value = glz::object(
    &T::api_key,
    &T::timeout
  );
  static constexpr std::string_view name = "config";
};

// Access optional values
auto* api_key = api->get<std::string>("/api_key");
if (api_key) {
  std::cout << "API Key: " << *api_key << "\n";
} else {
  std::cout << "API key not set\n";
}
```

### Variant Types

```c++
struct config {
  std::variant<int, std::string, double> value = 42;
};

template <>
struct glz::meta<config> {
  using T = config;
  static constexpr auto value = glz::object(&T::value);
  static constexpr std::string_view name = "config";
};

// Access the variant - must know the active type
auto* int_val = api->get<int>("/value");
if (int_val) {
  std::cout << "Value is int: " << *int_val << "\n";
}

auto* str_val = api->get<std::string>("/value");
if (str_val) {
  std::cout << "Value is string: " << *str_val << "\n";
}
```

### Spans

```c++
struct data {
  std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
  std::span<double> view;

  data() : view(values) {}
};

template <>
struct glz::meta<data> {
  using T = data;
  static constexpr auto value = glz::object(
    "values", &T::values,
    "view", &T::view
  );
  static constexpr std::string_view name = "data";
};

// Access span - provides view into the vector
auto* span = api->get<std::span<double>>("/view");
if (span) {
  for (auto val : *span) {
    std::cout << val << " ";
  }
}
```

## Function Signatures and Type Handling

### Reference Parameters

Member functions can accept parameters by value, lvalue reference, const lvalue reference, or rvalue reference:

```c++
struct api_type {
  void by_value(int x) { /* ... */ }
  void by_lvalue_ref(int& x) { ++x; }
  void by_const_lvalue_ref(const int& x) { /* ... */ }
  void by_rvalue_ref(int&& x) { /* ... */ }

  double sum_const_refs(const double& a, const double& b) { return a + b; }
  double sum_rvalue_refs(double&& a, double&& b) { return a + b; }
};

template <>
struct glz::meta<api_type> {
  using T = api_type;
  static constexpr auto value = glz::object(
    &T::by_value,
    &T::by_lvalue_ref,
    &T::by_const_lvalue_ref,
    &T::by_rvalue_ref,
    &T::sum_const_refs,
    &T::sum_rvalue_refs
  );
  static constexpr std::string_view name = "api_type";
};

// Call with different parameter styles
int val = 10;
api->call<void>("/by_lvalue_ref", val);  // val is now 11

auto result1 = api->call<double>("/sum_const_refs", 3.0, 4.0);
auto result2 = api->call<double>("/sum_rvalue_refs", 3.0, 4.0);
```

### Return Types

Functions can return by value, reference, const reference, or pointer:

```c++
struct api_type {
  int x = 42;

  int by_value() { return x; }
  int& by_reference() { return x; }
  const int& by_const_reference() { return x; }
  int* by_pointer() { return &x; }
};

template <>
struct glz::meta<api_type> {
  using T = api_type;
  static constexpr auto value = glz::object(
    &T::x,
    &T::by_value,
    &T::by_reference,
    &T::by_const_reference,
    &T::by_pointer
  );
  static constexpr std::string_view name = "api_type";
};

// Call functions with different return types
auto val = api->call<int>("/by_value");
auto ref = api->call<int&>("/by_reference");
auto const_ref = api->call<const int&>("/by_const_reference");
auto ptr = api->call<int*>("/by_pointer");

// Reference returns are wrapped in std::reference_wrapper
if (ref) {
  std::cout << "Reference value: " << ref.value().get() << "\n";
}
```

### Custom Types as Parameters

```c++
struct point {
  double x, y;
};

template <>
struct glz::meta<point> {
  using T = point;
  static constexpr auto value = glz::object("x", &T::x, "y", &T::y);
  static constexpr std::string_view name = "point";
};

struct geometry_api {
  double distance(const point& p1, const point& p2) {
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return std::sqrt(dx * dx + dy * dy);
  }
};

template <>
struct glz::meta<geometry_api> {
  using T = geometry_api;
  static constexpr auto value = glz::object("distance", &T::distance);
  static constexpr std::string_view name = "geometry_api";
};

// Call with custom types
point p1{0, 0};
point p2{3, 4};
auto dist = api->call<double>("/distance", p1, p2);  // Returns 5.0
```

## Performance Optimization

### Cache Function Objects

If you're calling the same function multiple times, use `get_fn` to retrieve a `std::function` once and reuse it:

```c++
// Inefficient: Creates std::function on every call
for (int i = 0; i < 1000; ++i) {
  auto result = api->call<int>("/compute", i);
}

// Efficient: Retrieve function once, call many times
auto compute_fn = api->get_fn<std::function<int(int)>>("/compute");
if (compute_fn) {
  for (int i = 0; i < 1000; ++i) {
    int result = compute_fn.value()(i);
  }
}
```

### Cache Pointers

Similarly, cache pointers to frequently accessed data:

```c++
// Inefficient: Looks up path on every access
for (int i = 0; i < 1000; ++i) {
  auto* value = api->get<int>("/counter");
  (*value)++;
}

// Efficient: Look up once, use many times
auto* counter = api->get<int>("/counter");
if (counter) {
  for (int i = 0; i < 1000; ++i) {
    (*counter)++;
  }
}
```

### Use BEVE for Binary Data

For performance-critical serialization, use BEVE instead of JSON:

```c++
std::string buffer;

// Slower: JSON serialization
api->write(glz::JSON, "", buffer);
api->read(glz::JSON, "", buffer);

// Faster: Binary serialization
api->write(glz::BEVE, "", buffer);
api->read(glz::BEVE, "", buffer);
```

BEVE is significantly faster for serialization/deserialization and produces smaller output.

### Batch Operations

When modifying multiple values, consider reading/writing at the root level:

```c++
// Less efficient: Multiple separate writes
std::string buf1, buf2, buf3;
api->write(glz::JSON, "/x", buf1);
api->write(glz::JSON, "/y", buf2);
api->write(glz::JSON, "/z", buf3);

// More efficient: Single write of entire object
std::string buffer;
api->write(glz::JSON, "", buffer);
```

## Type Hashing Deep Dive

Understanding how Glaze hashes types helps you debug type mismatches and design safer APIs.

### Hash Components

The type hash for a type `T` includes:

```c++
// Pseudo-code showing what gets hashed
hash = hash128(
  name,                        // Type name from glz::meta
  sizeof(T),                   // Size in bytes
  version.major,               // Version components
  version.minor,
  version.patch,
  is_trivial<T>,              // Type traits
  is_standard_layout<T>,
  is_default_constructible<T>,
  // ... all other type traits
  compiler_id,                 // "clang", "gcc", or "msvc"
  member_names                 // Names of all members (for object types)
);
```

### Type Name Examples

Glaze generates type names following these rules:

```c++
// Fundamental types
glz::name_v<int>           // "int32_t"
glz::name_v<double>        // "double"
glz::name_v<bool>          // "bool"

// CV-qualifiers and references
glz::name_v<const int>     // "const int32_t"
glz::name_v<int&>          // "int32_t&"
glz::name_v<const int&>    // "const int32_t&"
glz::name_v<int&&>         // "int32_t&&"

// Pointers
glz::name_v<int*>          // "int32_t*"
glz::name_v<const int*>    // "const int32_t*"

// Containers
glz::name_v<std::vector<int>>                           // "std::vector<int32_t>"
glz::name_v<std::map<std::string, int>>                 // "std::map<std::string,int32_t>"
glz::name_v<std::unordered_map<uint64_t, std::string>>  // "std::unordered_map<uint64_t,std::string>"

// Functions
glz::name_v<std::function<int(double)>>                 // "std::function<int32_t(double)>"
glz::name_v<std::function<void()>>                      // "std::function<void()>"
glz::name_v<std::function<double(const int&, const double&)>>
  // "std::function<double(const int32_t&,const double&)>"
```

### Debugging Type Mismatches

When you get a type mismatch error, check:

1. **Type name**: Ensure both sides use the same name in `glz::meta`
2. **Version**: Check if versions match
3. **Member names**: Verify all members have the same names
4. **Compiler**: Are you using the same compiler family?
5. **Type traits**: Did you change the type in a way that affects its traits?

Example debugging:

```c++
// Print type information for debugging
std::cout << "Type name: " << glz::name_v<my_api> << "\n";
std::cout << "Version: " << glz::version_v<my_api>.major << "."
          << glz::version_v<my_api>.minor << "."
          << glz::version_v<my_api>.patch << "\n";
std::cout << "Size: " << sizeof(my_api) << "\n";

// Get the actual hash
auto hash = glz::hash<my_api>();
std::cout << "Hash: " << std::hex << hash[0] << hash[1] << std::dec << "\n";
```

## Cross-Compilation Safety

### Compiler Compatibility

The type hash includes the compiler identifier, so types compiled with different compiler families won't match:

```c++
// Library compiled with GCC
// Client compiled with Clang
// Result: Type hash mismatch error ✓ (Safety feature!)
```

This is a **safety feature** because different compilers may have different ABIs for the same type.

### Same Compiler, Different Versions

Types compiled with different versions of the **same compiler family** will generally work if:
- The ABI hasn't changed
- All type traits remain the same
- The type layout is identical

### Breaking Changes

Certain changes will always break compatibility:

**Always Breaks Compatibility:**
- Changing type name in `glz::meta`
- Incrementing major version
- Changing `sizeof(T)`
- Adding/removing/renaming members
- Changing member types
- Changing from non-polymorphic to polymorphic (or vice versa)

**May Break Compatibility:**
- Changing member order (changes layout)
- Changing alignment requirements
- Adding virtual functions (changes type traits)

**Safe Changes:**
- Changing function implementations (no signature change)
- Changing private member variables (if not in glz::meta)

## Best Practices Summary

1. **Type Names**: Always provide meaningful, unique names for your types
2. **Versioning**: Use semantic versioning and increment appropriately
3. **Error Handling**: Always check return values and handle errors gracefully
4. **Performance**: Cache function objects and pointers for frequently used items
5. **Serialization**: Use BEVE for performance-critical binary serialization
6. **Type Safety**: Let Glaze's type system protect you - don't cast away safety
7. **Documentation**: Document your API types and their versioning policy
8. **Testing**: Test across the actual compilation boundaries you'll use in production
9. **Portability**: Prefer fixed-size types over platform-dependent types
10. **Compatibility**: Plan for API evolution - design for forward/backward compatibility from the start
