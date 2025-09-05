# Reflection in Glaze

Glaze provides compile time reflection utilities for C++.

```c++
struct T
{
  int a{};
  std::string str{};
  std::array<float, 3> arr{};
};

static_assert(glz::refl<T>::size == 3);
static_assert(glz::refl<T>::keys[0] == "a");
```

## Reflection Concepts

### `reflectable<T>`

The `reflectable<T>` concept identifies aggregate types that can be automatically reflected without requiring a `glz::meta` specialization:

```c++
struct SimpleStruct {
    int x;
    double y;
};

static_assert(glz::reflectable<SimpleStruct>); // true - aggregate type
```

### `has_reflect<T>` 

The `has_reflect<T>` concept detects whether `glz::reflect<T>` can be instantiated for a given type. It automatically captures **all** types that have a valid `reflect<T>` specialization, including:
- Types satisfying `reflectable<T>` (aggregate types)
- Types with `glz::meta` specializations (`glaze_object_t`, `glaze_array_t`, `glaze_enum_t`, etc.)
- Readable map types like `std::map` and `std::unordered_map`
- Any future types that get `reflect<T>` specializations

```c++
// Aggregate struct - both reflectable and has_reflect
struct Aggregate {
    int value;
};

// Struct with glz::meta - NOT reflectable but has_reflect
struct WithMeta {
    int x;
    double y;
};

template <>
struct glz::meta<WithMeta> {
    using T = WithMeta;
    static constexpr auto value = glz::object(&T::x, &T::y);
};

static_assert(glz::reflectable<Aggregate>);  // true
static_assert(glz::has_reflect<Aggregate>);  // true

static_assert(!glz::reflectable<WithMeta>);  // false - has meta specialization
static_assert(glz::has_reflect<WithMeta>);   // true - can use reflect<T>

// Both can safely use reflect<T>
constexpr auto aggregate_size = glz::reflect<Aggregate>::size;  // Works!
constexpr auto meta_size = glz::reflect<WithMeta>::size;        // Works!
```

### When to use each concept

- Use `reflectable<T>` when you specifically need aggregate types without `glz::meta` specializations
- Use `has_reflect<T>` when you want to check if `glz::reflect<T>` can be safely called
- Use `has_reflect<T>` in generic code that needs to work with any type that supports reflection

The `has_reflect<T>` concept is implemented by checking if `reflect<T>` can be instantiated, making it automatically stay in sync with all `reflect<T>` specializations

```c++
// Generic function that works with any reflectable type
template<glz::has_reflect T>
void print_field_count(const T& obj) {
    std::cout << "Type has " << glz::reflect<T>::size << " fields\n";
}

// Works with both aggregate types and types with glz::meta
print_field_count(Aggregate{});  // Works
print_field_count(WithMeta{});   // Also works
```

## glz::convert_struct

The `glz::convert_struct` function show a simple application of Glaze reflection at work. It allows two different structs with the same member names to convert from one to the other. If any of the fields don't have matching names, a compile time error will be generated.

```c++
struct a_type
{
   float fluff = 1.1f;
   int goo = 1;
   std::string stub = "a";
};

struct b_type
{
   float fluff = 2.2f;
   int goo = 2;
   std::string stub = "b";
};

struct c_type
{
   std::optional<float> fluff = 3.3f;
   std::optional<int> goo = 3;
   std::optional<std::string> stub = "c";
};

suite convert_tests = [] {
   "convert a to b"_test = [] {
      a_type in{};
      b_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 1.1f);
      expect(out.goo == 1);
      expect(out.stub == "a");
   };

   "convert a to c"_test = [] {
      a_type in{};
      c_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff.value() == 1.1f);
      expect(out.goo.value() == 1);
      expect(out.stub.value() == "a");
   };

   "convert c to a"_test = [] {
      c_type in{};
      a_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 3.3f);
      expect(out.goo == 3);
      expect(out.stub == "c");
   };
};
```

## Enum Reflection

Glaze provides compile-time enum reflection that extracts enum names and values without requiring manual registration or macros.

### Overview

Enum reflection in Glaze works with:
- Scoped enums (`enum class`)
- Unscoped enums (`enum`)
- Enums with custom underlying types
- Sparse enums (non-consecutive values)

The reflection happens entirely at compile time using compiler intrinsics, with zero runtime overhead.

### Basic Usage

```c++
#include "glaze/reflection/enum_reflect.hpp"

enum class Color { Red, Green, Blue };
enum class Status : int { Pending = -1, Running = 0, Complete = 1 };

// Get enum count
static_assert(glz::enum_count<Color> == 3);
static_assert(glz::enum_count<Status> == 3);

// Get enum names
static_assert(glz::enum_names<Color>[0] == "Red");
static_assert(glz::enum_names<Color>[1] == "Green");
static_assert(glz::enum_names<Color>[2] == "Blue");

// Get enum values
static_assert(glz::enum_values<Color>[0] == Color::Red);
static_assert(glz::enum_values<Color>[1] == Color::Green);
static_assert(glz::enum_values<Color>[2] == Color::Blue);

// Get min/max values
static_assert(glz::enum_min<Status> == Status::Pending);
static_assert(glz::enum_max<Status> == Status::Complete);
```

### Runtime Utilities

#### enum_name - Convert enum value to string

```c++
Color c = Color::Green;
std::string_view name = glz::enum_name(c);  // "Green"

// Returns empty string for invalid values
auto invalid = static_cast<Color>(100);
std::string_view invalid_name = glz::enum_name(invalid);  // ""
```

#### enum_value - Convert string to enum value

```c++
std::optional<Color> c = glz::enum_value<Color>("Blue");  // Color::Blue
std::optional<Color> invalid = glz::enum_value<Color>("Yellow");  // std::nullopt

// Case-insensitive search
std::optional<Color> c2 = glz::enum_value<Color>("blue", 
    [](auto a, auto b) { return glz::ieq(a, b); });  // Color::Blue
```

#### contains - Check if value/name exists

```c++
// Check by value
bool has_red = glz::contains<Color>(Color::Red);  // true
bool has_invalid = glz::contains<Color>(static_cast<Color>(10));  // false

// Check by underlying value
bool has_zero = glz::contains<Color>(0);  // true (Red = 0)

// Check by name
bool has_green = glz::contains<Color>("Green");  // true
bool has_yellow = glz::contains<Color>("Yellow");  // false
```

#### index_of - Get index of enum value

```c++
std::optional<size_t> idx = glz::index_of<Color>(Color::Blue);  // 2
std::optional<size_t> invalid = glz::index_of<Color>(static_cast<Color>(100));  // nullopt
```

### Concepts and Type Traits

```c++
// Check if type is an enum
static_assert(glz::is_enum<Color>);
static_assert(!glz::is_enum<int>);

// Check if enum is scoped
static_assert(glz::scoped_enum<Color>);  // enum class
static_assert(!glz::unscoped_enum<Color>);

// Check if enum is contiguous (consecutive values)
static_assert(glz::enum_is_contiguous<Color>);  // 0, 1, 2
static_assert(!glz::enum_is_contiguous<Sparse>);  // 1, 5, 10

// Check underlying type
static_assert(glz::signed_enum<Status>);
static_assert(glz::unsigned_enum<Color>);
```

### JSON Serialization with Enums

Enum reflection integrates seamlessly with Glaze's JSON serialization:

```c++
enum class Priority { Low, Medium, High };

struct Task {
   std::string name;
   Priority priority{Priority::Medium};
};

Task t{"Important Task", Priority::High};

// Default: serialize as numbers
std::string json;
glz::write_json(t, json);  // {"name":"Important Task","priority":2}

// Option: serialize as strings
constexpr glz::opts opts{.enum_as_string = true};
glz::write<opts>(t, json);  // {"name":"Important Task","priority":"High"}

// Reading works with both formats
Task parsed;
glz::read_json(parsed, R"({"name":"Task","priority":"Low"})");  // Works
glz::read_json(parsed, R"({"name":"Task","priority":0})");  // Also works
```


### Advanced: enumerate() Function

The `enumerate()` function leverages enum reflection to iterate over values:

```c++
enum class State { Init, Running, Done };

// Automatically uses enum reflection - no string names needed!
constexpr auto states = glz::enumerate<State>();

for (const auto& [value, name] : states) {
   // value is State enum value
   // name is string_view of the enum name
   std::cout << "State::" << name << " = " 
             << static_cast<int>(value) << "\n";
}
```

### Performance Considerations

- **Compile-time extraction**: Enum names and values are extracted at compile time
- **Zero overhead**: No runtime registration or initialization
- **Constexpr support**: Most operations can be used in constexpr contexts
- **Efficient storage**: Names are stored as `string_view` with static lifetime
- **Contiguous optimization**: Consecutive enums use optimized range checks

### Limitations

1. **Value range**: Works best with enums in range [-128, 127], configurable via `glz::enum_traits`
2. **Duplicate values (aliases)**: Enums with duplicate values are not fully supported. Only the first occurrence of each value is preserved during reflection; all aliases are discarded. For example:
   ```cpp
   enum class Example {
       Original = 2,
       Alias = 2,     // This will be discarded
       Another = 2    // This will also be discarded
   };
   // enum_count<Example> == 1 (not 3)
   // enum_name(Example::Alias) returns "Original" (not "Alias")
   // enum_cast<Example>("Alias") returns empty (fails)
   ```
   If you need alias support, use manual `glz::meta` specialization with `enumerate()`.
3. **Template enums**: Cannot reflect enums defined inside template classes

### Customization

For enums outside the default range, specialize `glz::enum_traits`:

```c++
enum class BigEnum { 
   Min = 1000, 
   Max = 2000 
};

template <>
struct glz::enum_traits<BigEnum> {
   static constexpr auto min = 1000;
   static constexpr auto max = 2000;
};
```

### Complete Example

```c++
#include "glaze/glaze.hpp"
#include "glaze/reflection/enum_reflect.hpp"

enum class LogLevel { Debug, Info, Warning, Error, Fatal };

struct LogEntry {
   LogLevel level;
   std::string message;
   int64_t timestamp;
};

void example() {
   // Create log entry
   LogEntry entry{LogLevel::Error, "Database connection failed", 1234567890};
   
   // Serialize with enum as string
   std::string json;
   constexpr glz::opts opts{.enum_as_string = true};
   glz::write<opts>(entry, json);
   // {"level":"Error","message":"Database connection failed","timestamp":1234567890}
   
   // Get human-readable level name
   std::string_view level_name = glz::enum_name(entry.level);  // "Error"
   
   // Check severity
   if (glz::index_of<LogLevel>(entry.level) >= 
       glz::index_of<LogLevel>(LogLevel::Error)) {
      // Handle critical log levels
   }
   
   // Parse log level from string
   auto parsed_level = glz::enum_value<LogLevel>("Warning");
   if (parsed_level) {
      entry.level = *parsed_level;
   }
}
```

