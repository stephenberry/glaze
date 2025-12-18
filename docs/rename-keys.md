# Rename Keys

The `rename_key` functionality in Glaze allows you to transform struct member names during JSON serialization and deserialization. This powerful feature operates entirely at compile time, directly affecting compile-time hash maps for optimal runtime performance.

## Overview

By default, Glaze uses C++ struct member names as JSON keys. However, you may need different key names in your JSON output to:

- Follow specific naming conventions (e.g., camelCase vs snake_case)
- Interface with external APIs that expect particular key formats
- Add prefixes, suffixes, or other transformations to keys
- Maintain backward compatibility while refactoring struct member names

## Basic Usage

To use `rename_key`, specialize the `glz::meta` template for your struct and implement a `rename_key` function:

```cpp
struct my_struct_t {
    std::string member_name{};
};

template <>
struct glz::meta<my_struct_t> {
    static constexpr std::string_view rename_key(const std::string_view key) {
        // Transform the key as needed
        return transformed_key;
    }
};
```

## Example 1: Specific Key Mapping

Transform specific member names to follow different naming conventions:

```cpp
struct renamed_t {
    std::string first_name{};
    std::string last_name{};
    int age{};
};

template <>
struct glz::meta<renamed_t> {
    static constexpr std::string_view rename_key(const std::string_view key) {
        if (key == "first_name") {
            return "firstName";
        }
        else if (key == "last_name") {
            return "lastName";
        }
        return key; // Return unchanged for other keys
    }
};
```

**Usage:**

```cpp
renamed_t obj{};
std::string buffer{};

// Writing JSON
glz::write_json(obj, buffer);
// Output: {"firstName":"","lastName":"","age":0}

// Reading JSON
buffer = R"({"firstName":"Kira","lastName":"Song","age":29})";
glz::read_json(obj, buffer);
// obj.first_name == "Kira"
// obj.last_name == "Song" 
// obj.age == 29
```

## Example 2: Dynamic Key Transformation

Apply systematic transformations to all keys using compile-time string manipulation:

```cpp
struct suffixed_keys_t {
    std::string first{};
    std::string last{};
};

template <>
struct glz::meta<suffixed_keys_t> {
    static constexpr std::string rename_key(const auto key) { 
        return std::string(key) + "_name"; 
    }
};
```

**Usage:**

```cpp
suffixed_keys_t obj{};
std::string buffer{};

// Writing JSON
glz::write_json(obj, buffer);
// Output: {"first_name":"","last_name":""}

// Reading JSON
buffer = R"({"first_name":"Kira","last_name":"Song"})";
glz::read_json(obj, buffer);
// obj.first == "Kira"
// obj.last == "Song"
```

## Compile-Time

The `rename_key` function, even when using `std::string` operates entirely at compile time. No string allocations or transformations at runtime.

## Implementation Details

### Return Types

The `rename_key` function can return:

- `const char*` or `std::string_view` for simple string literal transformations
- `std::string` when dynamic string construction is needed (as shown in the suffix example)

## Built-in Key Transformers

Glaze provides pre-built key transformers for common naming conventions. These transformers automatically convert between different casing styles at compile time.

### Available Transformers

Simply inherit from the appropriate transformer in your `glz::meta` specialization:

```cpp
struct my_struct {
    int user_id = 123;
    std::string first_name = "John";
    bool is_active = true;
};

// Choose your naming convention:
template <> struct glz::meta<my_struct> : glz::camel_case {};        // {"userId":123,"firstName":"John","isActive":true}
template <> struct glz::meta<my_struct> : glz::pascal_case {};       // {"UserId":123,"FirstName":"John","IsActive":true}
template <> struct glz::meta<my_struct> : glz::snake_case {};        // {"user_id":123,"first_name":"John","is_active":true}
template <> struct glz::meta<my_struct> : glz::screaming_snake_case {}; // {"USER_ID":123,"FIRST_NAME":"John","IS_ACTIVE":true}
template <> struct glz::meta<my_struct> : glz::kebab_case {};        // {"user-id":123,"first-name":"John","is-active":true}
template <> struct glz::meta<my_struct> : glz::screaming_kebab_case {}; // {"USER-ID":123,"FIRST-NAME":"John","IS-ACTIVE":true}
template <> struct glz::meta<my_struct> : glz::lower_case {};        // {"userid":123,"firstname":"John","isactive":true}
template <> struct glz::meta<my_struct> : glz::upper_case {};        // {"USERID":123,"FIRSTNAME":"John","ISACTIVE":true}
```

### Transformer Reference

| Transformer | Input Example | Output Example |
|------------|---------------|----------------|
| `camel_case` | `hello_world` | `helloWorld` |
| `pascal_case` | `hello_world` | `HelloWorld` |
| `snake_case` | `helloWorld` | `hello_world` |
| `screaming_snake_case` | `helloWorld` | `HELLO_WORLD` |
| `kebab_case` | `hello_world` | `hello-world` |
| `screaming_kebab_case` | `hello_world` | `HELLO-WORLD` |
| `lower_case` | `HelloWorld` | `helloworld` |
| `upper_case` | `HelloWorld` | `HELLOWORLD` |

### Advanced: Direct Function Usage

The transformation functions are also available for direct use:

```cpp
std::string camel = glz::to_camel_case("hello_world");        // "helloWorld"
std::string snake = glz::to_snake_case("XMLParser");          // "xml_parser"
std::string kebab = glz::to_kebab_case("HTTPSConnection");    // "https-connection"
```

### Implementation Notes

- All transformers handle acronyms intelligently (e.g., `XMLParser` → `xml_parser`)
- Number handling preserves digits in identifiers
- Transformations are bidirectional - the same transformer works for both reading and writing JSON
- Zero runtime overhead - all transformations occur at compile time via `constexpr`

## Indexed `rename_key`: Type-Aware Transformations

The indexed `rename_key` variant provides access to member type information, enabling conditional transformations based on compile-time type checks. This is particularly useful when you want to treat different member types differently.

### Overview

With indexed `rename_key`, you can access:
- **Member index:** The template parameter `Index`
- **Member type:** Via `glz::member_type_t<T, Index>`
- **Member name:** Via `glz::member_nameof<Index, T>`

This enables powerful compile-time logic for key transformations.

### Example 1: Using Enum Type Names as Keys

A common use case is to use an enum's type name as the JSON key instead of the struct member name:

```cpp
namespace mylib {
    enum struct MyEnum { First, Second };
    enum struct MyFlag { Yes, No };
}

// Register enums with Glaze
template <>
struct glz::meta<mylib::MyEnum> {
    using enum mylib::MyEnum;
    static constexpr auto value = enumerate(First, Second);
};

template <>
struct glz::meta<mylib::MyFlag> {
    using enum mylib::MyFlag;
    static constexpr auto value = enumerate(Yes, No);
};

struct AppContext {
    int num{};
    mylib::MyEnum e{};
    mylib::MyFlag f{};
};

template <>
struct glz::meta<AppContext> {
    template <size_t Index>
    static constexpr auto rename_key() {
        using MemberType = glz::member_type_t<AppContext, Index>;

        if constexpr (std::is_enum_v<MemberType>) {
            return glz::name_v<MemberType>;  // Use enum's type name
        } else {
            return glz::member_nameof<Index, AppContext>;  // Use member name
        }
    }
};
```

**Usage:**

```cpp
AppContext obj{42, mylib::MyEnum::Second, mylib::MyFlag::Yes};
std::string json = glz::write_json(obj).value();
// Output: {"num":42,"mylib::MyEnum":"Second","mylib::MyFlag":"Yes"}
```

### Example 2: Namespace Stripping

You can customize how type names are formatted by manipulating them at compile time:

```cpp
template <>
struct glz::meta<AppContext> {
    template <size_t Index>
    static constexpr auto rename_key() {
        using MemberType = glz::member_type_t<AppContext, Index>;

        if constexpr (std::is_enum_v<MemberType>) {
            constexpr auto full_name = glz::name_v<MemberType>;
            constexpr size_t pos = full_name.rfind("::");
            return (pos == std::string_view::npos)
                ? full_name
                : full_name.substr(pos + 2);
        } else {
            return glz::member_nameof<Index, AppContext>;
        }
    }
};
```

**Usage:**

```cpp
AppContext obj{42, mylib::MyEnum::Second, mylib::MyFlag::Yes};
std::string json = glz::write_json(obj).value();
// Output: {"num":42,"MyEnum":"Second","MyFlag":"Yes"}
```

### Example 3: Conditional Transformations by Type

Apply different transformations based on member types:

```cpp
struct MixedTypes {
    std::string name{};
    int count{};
    Status status{};  // enum
};

template <>
struct glz::meta<MixedTypes> {
    template <size_t Index>
    static constexpr auto rename_key() {
        using MemberType = glz::member_type_t<MixedTypes, Index>;
        constexpr auto name = glz::member_nameof<Index, MixedTypes>;

        if constexpr (std::is_enum_v<MemberType>) {
            return glz::name_v<MemberType>;  // Use type name for enums
        } else if constexpr (std::is_integral_v<MemberType>) {
            return name;  // Keep original name for integers
        } else {
            return glz::to_camel_case(name);  // CamelCase for other types
        }
    }
};
```

### Example 4: Generic Key Transformation

The indexed variant also works for generic transformations without using type information:

```cpp
struct Point {
    double x{};
    double y{};
};

template <>
struct glz::meta<Point> {
    template <size_t Index>
    static constexpr auto rename_key() {
        constexpr auto name = glz::member_nameof<Index, Point>;

        if constexpr (name == "x") return "X";
        else if constexpr (name == "y") return "Y";
        else return name;
    }
};
```

**Usage:**

```cpp
Point p{3.14, 2.71};
std::string json = glz::write_json(p).value();
// Output: {"X":3.14,"Y":2.71}
```

### Benefits

- **Type-aware:** Make decisions based on member types at compile time
- **Generic:** Works for any transformation logic
- **Zero overhead:** All logic executes at compile time
- **Composable:** Combine multiple transformation strategies
- **Natural integration:** Works seamlessly with automatic reflection

### When to Use Each Variant

| Variant | Use When |
|---------|----------|
| `rename_key(const std::string_view key)` | You need to transform keys based on their string values |
| `template <size_t Index> rename_key()` | You need access to member types or index information |
| Built-in transformers (e.g., `glz::camel_case`) | You want simple naming convention conversions |

### Implementation Notes

- The indexed `rename_key` must return a type convertible to `std::string_view`
- All transformation logic is evaluated at compile time
- The indexed variant is mutually exclusive with the string-based variant
- You can only define one `rename_key` variant per type
