# Glaze Interfaces (Generic Library API)

Glaze has been designed to work as a generic interface for shared libraries and more. This is achieved through JSON pointer syntax access to memory.

Glaze allows a single header API (`api.hpp`) to be used for every shared library interface, greatly simplifying shared library handling.

> Interfaces are simply Glaze object types. So whatever any JSON/binary interface can automatically be used as a library API.

## Overview

The Glaze API system provides:

- **Type-safe cross-compilation access**: Access data structures and call functions across shared library boundaries with compile-time type checking
- **Single universal API**: One API header (`glaze/api/api.hpp`) works for all shared libraries
- **JSON/Binary serialization**: Built-in support for reading and writing data via JSON or BEVE (Binary Efficient Versatile Encoding)
- **JSON pointer access**: Navigate complex data structures using JSON pointer syntax (e.g., `/path/to/field`)
- **Member function invocation**: Call member functions across API boundaries with full type safety
- **Automatic pointer unwrapping**: Transparent access through `std::unique_ptr`, `std::shared_ptr`, and raw pointers

## The API Interface

The core API is shown below. It is simple, yet incredibly powerful, allowing pretty much any C++ class to be manipulated across the API via JSON or binary, or even the class itself to be passed and safely cast on the other side.

```c++
namespace glz {
  struct api {
    api() noexcept = default;
    api(const api&) noexcept = default;
    api(api&&) noexcept = default;
    api& operator=(const api&) noexcept = default;
    api& operator=(api&&) noexcept = default;
    virtual ~api() noexcept {}

    // Get a typed pointer to a value at the given JSON pointer path
    template <class T>
    [[nodiscard]] T* get(const sv path) noexcept;

    // Get a std::function from a member function or std::function across the API
    template <class T>
    [[nodiscard]] expected<T, error_code> get_fn(const sv path) noexcept;

    // Call a member function across the API
    template <class Ret, class... Args>
    expected<func_return_t<Ret>, error_code> call(const sv path, Args&&... args) noexcept;

    // Check if a path exists
    [[nodiscard]] virtual bool contains(const sv path) noexcept = 0;

    // Read data into the API object from JSON or BEVE
    virtual bool read(const uint32_t format, const sv path, const sv data) noexcept = 0;

    // Write data from the API object to JSON or BEVE
    virtual bool write(const uint32_t format, const sv path, std::string& data) noexcept = 0;

    // Get the last error message
    [[nodiscard]] virtual const sv last_error() const noexcept { return error; }

    // Low-level void* access (prefer templated get)
    [[nodiscard]] virtual std::pair<void*, glz::hash_t> get(const sv path) noexcept = 0;

   protected:
    virtual bool caller(const sv path, const glz::hash_t type_hash, void*& ret,
                        std::span<void*> args) noexcept = 0;

    virtual std::unique_ptr<void, void (*)(void*)> get_fn(const sv path,
                                                          const glz::hash_t type_hash) noexcept = 0;

    std::string error{};
  };

  // Interface map type: maps API names to factory functions
  using iface = std::map<std::string, std::function<std::shared_ptr<api>()>, std::less<>>;

  // Function type for the glz_iface entry point
  using iface_fn = std::shared_ptr<glz::iface> (*)();
}
```

## Basic Usage

### Accessing Data Members

You can access data members using the `get` method with JSON pointer syntax:

```c++
// Assume we have an API object 'io' of type std::shared_ptr<glz::api>
auto* x = io->get<int>("/x");              // Get pointer to int
auto* y = io->get<double>("/y");           // Get pointer to double
auto* z = io->get<std::vector<double>>("/z"); // Get pointer to vector

// Use the values
if (x) {
  std::cout << "x = " << *x << "\n";
}
```

### Reading and Writing Data

The API supports reading and writing entire objects or specific paths using JSON or BEVE:

```c++
// Write the entire object to JSON
std::string json_buffer;
io->write(glz::JSON, "", json_buffer);

// Write a specific path to JSON
std::string x_json;
io->write(glz::JSON, "/x", x_json);

// Read from JSON into the API object
std::string input_json = R"({"x": 42, "y": 3.14})";
io->read(glz::JSON, "", input_json);

// Read into a specific path
io->read(glz::JSON, "/x", "100");

// Use BEVE (binary format) for better performance
std::string beve_buffer;
io->write(glz::BEVE, "", beve_buffer);
io->read(glz::BEVE, "", beve_buffer);
```

## Member Functions

Member functions can be registered with the metadata, which allows the function to be called across the API.

```c++
struct my_api {
  int x = 7;
  double y = 5.5;

  int func() { return 5; }
  double sum(double a, double b) { return a + b; }
  void increment(int& value) { ++value; }
};

template <>
struct glz::meta<my_api> {
  using T = my_api;
  static constexpr auto value = object(
    "x", &T::x,
    "y", &T::y,
    "func", &T::func,
    "sum", &T::sum,
    "increment", &T::increment
  );
  static constexpr std::string_view name = "my_api";
};
```

### Calling Member Functions

The `call` method invokes member functions across the API. It returns an `expected<T, error_code>` for proper error handling:

```c++
std::shared_ptr<glz::iface> iface{ glz_iface()() };
auto io = (*iface)["my_api"]();

// Call function with no arguments
auto result = io->call<int>("/func");
if (result) {
  std::cout << "func returned: " << result.value() << "\n";
}

// Call function with arguments
auto sum_result = io->call<double>("/sum", 7.0, 2.0);
if (sum_result) {
  std::cout << "sum = " << sum_result.value() << "\n";  // prints 9.0
}

// Call function with reference parameters
int value = 10;
auto inc_result = io->call<void>("/increment", value);
// value is now 11
```

### Getting std::function Objects

`get_fn` provides a means of getting a `std::function` from a member function across the API. This can be more efficient if you intend to call the same function multiple times:

```c++
// Get a std::function for a no-argument function
auto func = io->get_fn<std::function<int()>>("/func");
if (func) {
  int result = func.value()();
  std::cout << "result = " << result << "\n";
}

// Get a std::function for a multi-argument function
auto sum_fn = io->get_fn<std::function<double(double, double)>>("/sum");
if (sum_fn) {
  double result = sum_fn.value()(7.0, 2.0);
  std::cout << "sum = " << result << "\n";
}

// Get a std::function with reference parameters
auto inc_fn = io->get_fn<std::function<void(int&)>>("/increment");
if (inc_fn) {
  int val = 5;
  inc_fn.value()(val);
  // val is now 6
}
```

## std::function Support

Glaze allows `std::function` objects to be stored as members of your API types. This enables you to expose callable objects (lambdas, function objects, etc.) across the API boundary.

```c++
struct my_api {
  int x = 7;
  double y = 5.5;

  // Store a std::function as a member
  std::function<double(const int&, const double&)> f =
    [](const auto& i, const auto& d) { return i * d; };

  std::function<void()> init = [] {
    std::cout << "Initialization complete!\n";
  };
};

template <>
struct glz::meta<my_api> {
  using T = my_api;
  static constexpr auto value = object(
    "x", &T::x,
    "y", &T::y,
    "f", &T::f,
    "init", &T::init
  );
  static constexpr std::string_view name = "my_api";
};
```

You can then access and call these functions:

```c++
// Get and call a std::function
auto* f = io->get<std::function<double(const int&, const double&)>>("/f");
if (f) {
  int x = 7;
  double y = 5.5;
  double result = (*f)(x, y);  // result = 38.5
}

// Call a void function
auto* init = io->get<std::function<void()>>("/init");
if (init) {
  (*init)();  // Prints "Initialization complete!"
}
```

## Type Safety

A valid interface concern is binary compatibility between types. Glaze uses compile-time hashing of types that is able to catch a wide range of changes to classes or types that would cause binary incompatibility. These compile-time hashes are checked when accessing across the interface and provide a safeguard, much like a `std::any_cast`, but working across compilations.

**Key difference from `std::any_cast`**: `std::any_cast` does not guarantee any safety between separately compiled code, whereas Glaze adds significant type checking across compilations and versions of compilers.

The type hash is a 128-bit value computed from multiple type characteristics, providing robust detection of incompatible changes. When you call `get<T>()` or `call<Ret>()`, the system verifies that the type hash matches before allowing access, returning `nullptr` or an error if there's a mismatch.

### Hash Collision Safety

With a 128-bit hash, the probability of collision is astronomically low:
- With 10,000 registered types: approximately **1.47 × 10⁻³¹** chance of collision
- For comparison, the probability of winning the Mega Millions lottery (1/302,575,350 ≈ 3.3 × 10⁻⁹) is vastly higher
- You would need to win the lottery **2.25 × 10²²** times (22 sextillion times) to equal the collision probability

This makes the 128-bit hash more than sufficient for any practical application.

## Name

By default custom type names from `glz::name_v` will be `"Unnamed"`. It is best practice to give types the same name as it has in C++, including the namespace (at least the local namespace).

Concepts exist for naming `const`, pointer (`*`), and reference (`&`), versions of types as they are used. Many standard library containers are also supported.

```c++
expect(glz::name_v<std::vector<float>> == "std::vector<float>");
```

To add a name for your class, include it in the `glz::meta`:

```c++
template <>
struct glz::meta<my_api> {
  static constexpr std::string_view name = "my_api";
};
```

Or, include it via local glaze meta:

```c++
struct my_api {
  struct glaze {
    static constexpr std::string_view name = "my_api";
  };
};
```

## Version

By default all types get a version of `0.0.1`. The version tag allows the user to intentionally break API compatibility for a type when making changes that would not be caught by the compile time type checking.

```c++
template <>
struct glz::meta<my_api> {
  static constexpr glz::version_t version{ 0, 0, 2 };
};
```

Or, include it locally like `name` or `value`.

## What Is Checked?

Glaze's type safety system performs comprehensive compile-time checks to detect binary incompatibilities. The following characteristics are hashed and checked when accessing types across the API:

### Type Identity
-  **`name` in meta**: The type's registered name (e.g., "my_api")
-  **`version` in meta**: The semantic version of the type (major, minor, patch)
-  **`sizeof` the type**: The size of the type in bytes
-  **Member variable names**: All member variable names for object types (ensures field compatibility)

### Compiler Information
-  **Compiler type**: Distinguishes between clang, gcc, and msvc (different compilers may have different ABIs)

### Type Traits
These standard C++ type traits are hashed to ensure binary compatibility:

**Triviality and Layout**
-  `std::is_trivial`
-  `std::is_standard_layout`

**Construction**
-  `std::is_default_constructible`
-  `std::is_trivially_default_constructible`
-  `std::is_nothrow_default_constructible`

**Copy and Move**
-  `std::is_trivially_copyable`
-  `std::is_move_constructible`
-  `std::is_trivially_move_constructible`
-  `std::is_nothrow_move_constructible`

**Destruction**
-  `std::is_destructible`
-  `std::is_trivially_destructible`
-  `std::is_nothrow_destructible`

**Other Characteristics**
-  `std::has_unique_object_representations`
-  `std::is_polymorphic`
-  `std::has_virtual_destructor`
-  `std::is_aggregate`

Any change to these characteristics between the library and the client will result in a hash mismatch, preventing unsafe access.

## Automatic Pointer Unwrapping

Glaze automatically unwraps pointer types when accessing members through the API. This means you can access the pointed-to value directly without manually dereferencing:

```c++
struct my_api {
  int x = 7;
  int* x_ptr = &x;
  std::unique_ptr<double> uptr = std::make_unique<double>(5.5);
  std::shared_ptr<std::string> sptr = std::make_shared<std::string>("hello");
};

template <>
struct glz::meta<my_api> {
  using T = my_api;
  static constexpr auto value = object(
    "x", &T::x,
    "x_ptr", &T::x_ptr,
    "uptr", &T::uptr,
    "sptr", &T::sptr
  );
  static constexpr std::string_view name = "my_api";
};
```

Access the unwrapped values:

```c++
auto io = (*iface)["my_api"]();

// Access through raw pointer - returns pointer to the int, not pointer to pointer
auto* x = io->get<int>("/x_ptr");
if (x) {
  std::cout << *x << "\n";  // prints 7
}

// Access through unique_ptr - returns pointer to the double
auto* y = io->get<double>("/uptr");
if (y) {
  std::cout << *y << "\n";  // prints 5.5
}

// Access through shared_ptr - returns pointer to the string
auto* s = io->get<std::string>("/sptr");
if (s) {
  std::cout << *s << "\n";  // prints "hello"
}
```

This unwrapping works recursively, so `std::unique_ptr<std::shared_ptr<T>>` would also be unwrapped to access `T` directly.

## Supported Standard Library Types

The Glaze API system includes built-in support for many standard library types:

- **Containers**: `std::vector`, `std::array`, `std::deque`, `std::list`
- **Associative containers**: `std::map`, `std::unordered_map`
- **Smart pointers**: `std::unique_ptr`, `std::shared_ptr`
- **Optional**: `std::optional`
- **Variant**: `std::variant`
- **Tuple**: `std::tuple`
- **Span**: `std::span`
- **Functional**: `std::function`
- **String**: `std::string`, `std::string_view`

All these types have proper name and hash support for type-safe cross-compilation access.

## Error Handling

The API provides multiple mechanisms for error handling:

### Return Values

`get`, `get_fn`, and `call` return values that indicate success or failure:

```c++
// get returns nullptr on failure
auto* x = io->get<int>("/nonexistent");
if (!x) {
  std::cerr << "Failed to get value\n";
}

// get_fn and call return expected<T, error_code>
auto result = io->call<int>("/func");
if (!result) {
  std::cerr << "Call failed with error code\n";
}

auto fn = io->get_fn<std::function<void()>>("/init");
if (!fn) {
  std::cerr << "Failed to get function\n";
}
```

### Error Messages

The `last_error()` method provides detailed error messages:

```c++
auto* x = io->get<int>("/wrong_path");
if (!x) {
  std::cout << "Error: " << io->last_error() << "\n";
}

auto result = io->call<int>("/wrong_type");
if (!result) {
  std::cout << "Error: " << io->last_error() << "\n";
}
```

### Path Checking

Use `contains()` to check if a path exists before accessing:

```c++
if (io->contains("/x")) {
  auto* x = io->get<int>("/x");
  // Safe to use x
}
```

## Further Reading

For more detailed information on specific topics:

- **[Building Shared Libraries](building-shared-libraries.md)**: Complete guide to creating and using shared libraries with Glaze, including CMake configuration, platform-specific details, and best practices
- **[Advanced API Usage](advanced-api-usage.md)**: In-depth coverage of advanced patterns, performance optimization, type hashing, and cross-compilation safety

## Example: Complete API Definition

Here's a complete example showing all major features:

```c++
#include "glaze/api/impl.hpp"

// Custom types
struct user {
  std::string name;
  int age;
};

template <>
struct glz::meta<user> {
  using T = user;
  static constexpr auto value = glz::object(
    "name", &T::name,
    "age", &T::age
  );
  static constexpr std::string_view name = "user";
};

// Main API type
struct user_management_api {
  // Data members
  std::vector<user> users;
  int user_count = 0;
  std::map<std::string, int> user_ids;

  // Smart pointers
  std::unique_ptr<std::string> api_key = std::make_unique<std::string>("secret");

  // std::function members
  std::function<bool(const user&)> validate_user = [](const user& u) {
    return !u.name.empty() && u.age > 0;
  };

  // Member functions
  void add_user(const user& u) {
    users.push_back(u);
    user_count++;
  }

  user& get_user(int index) {
    return users.at(index);
  }

  int count_users() const {
    return user_count;
  }

  std::vector<std::string> get_user_names() const {
    std::vector<std::string> names;
    for (const auto& u : users) {
      names.push_back(u.name);
    }
    return names;
  }
};

template <>
struct glz::meta<user_management_api> {
  using T = user_management_api;
  static constexpr auto value = glz::object(
    "users", &T::users,
    "user_count", &T::user_count,
    "user_ids", &T::user_ids,
    "api_key", &T::api_key,
    "validate_user", &T::validate_user,
    "add_user", &T::add_user,
    "get_user", &T::get_user,
    "count_users", &T::count_users,
    "get_user_names", &T::get_user_names
  );

  static constexpr std::string_view name = "user_management_api";
  static constexpr glz::version_t version{1, 0, 0};
};

// Export the API
glz::iface_fn glz_iface() noexcept {
  return glz::make_iface<user_management_api>();
}
```

Usage:

```c++
#include "glaze/api/lib.hpp"

int main() {
  // Load the library
  glz::lib_loader lib("./libs");
  auto api = lib["user_management_api"]();

  // Add a user using a member function
  user new_user{"Alice", 30};
  auto result = api->call<void>("/add_user", new_user);

  // Get user count
  auto count = api->call<int>("/count_users");
  std::cout << "User count: " << count.value() << "\n";

  // Access data directly
  auto* users = api->get<std::vector<user>>("/users");
  for (const auto& u : *users) {
    std::cout << u.name << " (" << u.age << ")\n";
  }

  // Call std::function
  auto* validator = api->get<std::function<bool(const user&)>>("/validate_user");
  user test_user{"Bob", 25};
  if ((*validator)(test_user)) {
    std::cout << "User is valid\n";
  }

  // Read/write the entire API state
  std::string json;
  api->write(glz::JSON, "", json);
  std::cout << "API state:\n" << json << "\n";

  // Modify and read back
  api->read(glz::JSON, "", R"({"user_count": 10})");
  auto* new_count = api->get<int>("/user_count");
  std::cout << "New count: " << *new_count << "\n";

  return 0;
}
```
