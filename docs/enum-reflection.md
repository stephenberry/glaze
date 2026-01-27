# Automatic Enum String Serialization

Glaze supports serializing enums as strings by providing metadata via `glz::meta`. By default, this requires manually specifying each enum value. This guide shows how to use compile-time enum reflection libraries to automatically serialize **all** enums as strings without writing individual metadata for each type.

## The Problem

Glaze serializes enums as integers by default. To serialize as strings, you typically write:

```cpp
enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color> {
    using enum Color;
    static constexpr auto value = glz::enumerate(Red, Green, Blue);
};
```

This becomes tedious with many enums and must be maintained as enums change.

## The Solution

Glaze's `meta<T>` template supports two members for enum serialization:

- `keys` - An array of string names
- `value` - An array of enum values

Enum reflection libraries can provide these at compile-time, enabling a single generic template that handles all enums automatically.

## Library Options

### magic_enum

[magic_enum](https://github.com/Neargye/magic_enum) is the most widely used enum reflection library.

```cpp
#include <glaze/glaze.hpp>
#include <magic_enum/magic_enum.hpp>

namespace glz {
template <typename T>
   requires std::is_enum_v<T>
struct meta<T> {
   static constexpr auto keys = magic_enum::enum_names<T>();
   static constexpr auto value = magic_enum::enum_values<T>();
};
}
```

**Limitations:**

- Default range is `[-128, 127]`. Extend with:
  ```cpp
  #define MAGIC_ENUM_RANGE_MIN 0
  #define MAGIC_ENUM_RANGE_MAX 256
  ```
- Can increase compile times on large projects

### enchantum

[enchantum](https://github.com/ZXShady/enchantum) focuses on faster compile times than magic_enum.

```cpp
#include <glaze/glaze.hpp>
#include <enchantum/enchantum.hpp>
#include <enchantum/entries.hpp>

namespace glz {
template <typename T>
   requires std::is_enum_v<T>
struct meta<T> {
   static constexpr auto keys = enchantum::names<T>;
   static constexpr auto value = enchantum::values<T>;
};
}
```

**Advantages:**

- Significantly faster compile times than magic_enum
- Smaller binary sizes

### simple_enum

[simple_enum](https://github.com/arturbac/simple_enum) provides native Glaze integration for Glaze 5.x.

For Glaze 5.x, simple_enum's built-in integration handles everything automatically. For newer Glaze versions, you can use the same pattern:

```cpp
#include <glaze/glaze.hpp>
#include <simple_enum/simple_enum.hpp>

namespace glz {
template <typename T>
   requires std::is_enum_v<T>
struct meta<T> {
   static constexpr auto keys = simple_enum::enum_names_array_v<T>;
   static constexpr auto value = simple_enum::enum_values_array_v<T>;
};
}
```

**Advantages:**

- Bounded enumeration support for efficient compile times
- Native Glaze 5.x integration

## Example Usage

With any of the above integrations in place:

```cpp
enum class Status { Active, Inactive, Pending };
enum class Priority : int { Low = 1, Medium = 5, High = 10 };

struct Task {
   std::string name;
   Status status;
   Priority priority;
};

int main() {
   Task task{"My Task", Status::Active, Priority::High};

   auto json = glz::write_json(task).value();
   // Result: {"name":"My Task","status":"Active","priority":"High"}

   Task parsed{};
   glz::read_json(parsed, json);
   // Correctly parses strings back to enum values
}
```

## Supported Features

These integrations work with:

- **Scoped enums** (`enum class`)
- **Unscoped enums** (`enum`)
- **Sparse enums** (non-sequential values)
- **Enums in containers** (`std::vector<MyEnum>`, `std::map<MyEnum, T>`)
- **Enums in structs**

## Overriding for Specific Enums

If you need custom names for a specific enum, define an explicit specialization:

```cpp
enum class HttpStatus { OK = 200, NotFound = 404 };

// Custom names override the generic template
template <>
struct glz::meta<HttpStatus> {
   static constexpr std::array keys{"200 OK", "404 Not Found"};
   static constexpr std::array value{HttpStatus::OK, HttpStatus::NotFound};
};
```

## Choosing a Library

| Library | Compile Time | Range Limits | Notes |
|---------|--------------|--------------|-------|
| magic_enum | Slower | [-128, 127] default | Most popular, well-documented |
| enchantum | Faster | Configurable | Optimized for compile time |
| simple_enum | Fast | Bounded enums | Native Glaze 5.x support |

For projects with many enums or where compile time matters, consider enchantum or simple_enum. For smaller projects or maximum compatibility, magic_enum is a safe choice.

## See Also

- [Glaze enum documentation](json.md#enums)
- [magic_enum](https://github.com/Neargye/magic_enum)
- [enchantum](https://github.com/ZXShady/enchantum)
- [simple_enum](https://github.com/arturbac/simple_enum)
