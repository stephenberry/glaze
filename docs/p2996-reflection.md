# C++26 P2996 Reflection Support

Glaze supports [P2996 "Reflection for C++26"](https://wg21.link/P2996) as an alternative reflection backend. When enabled, P2996 reflection replaces the traditional `__PRETTY_FUNCTION__` parsing and structured binding tricks with proper compile-time reflection primitives.

## Overview

P2996 was voted into C++26 in June 2025 and provides standardized compile-time reflection capabilities including:
- Querying type metadata at compile time
- Iterating over struct members
- Getting member names and types
- Splicing reflected entities back into code

When `GLZ_REFLECTION26` is enabled, Glaze uses P2996 for:
- `count_members<T>` - counting struct fields
- `to_tie(T&)` - creating a tuple of references to members
- `member_nameof<N, T>` - getting the name of the Nth member
- `member_names<T>` - array of all member names
- `type_name<T>` - getting the type name as a string

The entire Glaze API remains unchanged - JSON, BEVE, CSV, and all other formats work exactly as before.

## Requirements

P2996 reflection requires a compiler with C++26 reflection support:

- **Bloomberg clang-p2996**: The reference implementation
  - Repository: https://github.com/bloomberg/clang-p2996
  - Based on Clang/LLVM with P2996 extensions

### Compiler Flags

```bash
clang++ -std=c++26 -freflection -fexpansion-statements -stdlib=libc++
```

| Flag | Purpose |
|------|---------|
| `-std=c++26` | Enable C++26 mode |
| `-freflection` | Enable P2996 reflection |
| `-fexpansion-statements` | Enable template for... expansion |
| `-stdlib=libc++` | Required for `<meta>` header |

## Enabling P2996 Support

### Option 1: CMake (Recommended)

```cmake
set(glaze_ENABLE_REFLECTION26 ON)
FetchContent_MakeAvailable(glaze)
```

### Option 2: Compiler Define

```bash
clang++ -DGLZ_REFLECTION26=1 -std=c++26 -freflection ...
```

### Option 3: Automatic Detection

If your compiler defines `__cpp_lib_reflection` or `__cpp_impl_reflection`, Glaze automatically enables P2996 support.

## Feature Detection

Check if P2996 is enabled at compile time:

```cpp
#include "glaze/core/feature_test.hpp"

#if GLZ_REFLECTION26
// P2996 reflection is available
#endif

// Or use the constexpr variable
if constexpr (glz::has_reflection26) {
    // P2996 code path
}
```

## Usage Example

The API is identical whether using P2996 or traditional reflection:

```cpp
#include "glaze/glaze.hpp"

struct Person {
    std::string name;
    int age;
    double height;
};

int main() {
    // JSON serialization works the same
    Person p{"Alice", 30, 1.65};
    std::string json = glz::write_json(p).value_or("error");
    // {"name":"Alice","age":30,"height":1.65}

    // Member names reflection
    constexpr auto names = glz::member_names<Person>;
    // names == {"name", "age", "height"}

    // Count members
    constexpr auto count = glz::detail::count_members<Person>;
    // count == 3

    // Type name
    constexpr auto type = glz::type_name<Person>;
    // type == "Person"

    // to_tie for member access
    auto tie = glz::to_tie(p);
    glz::get<0>(tie) = "Bob";  // Modifies p.name
}
```

## Benefits of P2996

| Feature | Traditional | P2996 |
|---------|-------------|-------|
| Max struct members | 128 | Unlimited |
| Non-aggregate types | Not supported | Full support |
| Inheritance | Requires explicit `glz::meta` | Automatic (via `bases_of`) |
| Member name extraction | `__PRETTY_FUNCTION__` parsing | `std::meta::identifier_of` |
| Member count | Structured binding probe | `nonstatic_data_members_of().size()` |
| Private member access | Limited | Full (with `access_context::unchecked()`) |
| Compile-time safety | Compiler-specific hacks | Standardized API |

### Unlimited Member Count

Traditional reflection uses a compile-time binary search with structured bindings, limiting structs to 128 members. P2996 has no such limitation:

```cpp
// Works with P2996, would fail with traditional reflection
struct LargeStruct {
    int field1, field2, /* ... */ field200;
};

constexpr auto count = glz::detail::count_members<LargeStruct>;
// count == 200 (with P2996)
```

### Non-Aggregate Type Support

Traditional reflection requires types to be aggregates (no user-defined constructors, no private members, no virtual functions, no base classes). P2996 removes this limitation:

```cpp
// Classes with custom constructors
class ConstructedClass {
public:
    std::string name;
    int value;

    ConstructedClass() : name("default"), value(0) {}
    ConstructedClass(std::string n, int v) : name(std::move(n)), value(v) {}
};

// Works with P2996!
std::string json;
glz::write_json(ConstructedClass{"test", 42}, json);
// {"name":"test","value":42}

// Classes with private members (using glz::meta for access)
class PrivateMembers {
    std::string secret;
    int hidden;
public:
    PrivateMembers(std::string s, int h) : secret(std::move(s)), hidden(h) {}
    friend struct glz::meta<PrivateMembers>;
};

template <>
struct glz::meta<PrivateMembers> {
    using T = PrivateMembers;
    static constexpr auto value = object(&T::secret, &T::hidden);
};

// Classes with virtual functions
class VirtualClass {
public:
    std::string name;
    virtual ~VirtualClass() = default;
    virtual void do_something() {}
};

// Works with P2996!
constexpr auto count = glz::detail::count_members<VirtualClass>;
// count == 1 (only 'name', virtual function table pointer is not counted)

// Derived classes - base class members are automatically included!
class Base {
public:
    std::string name;
    int id;
    Base() : name("base"), id(0) {}
};

class Derived : public Base {
public:
    std::string extra;
    Derived() : Base(), extra("derived") {}
};

// No glz::meta needed! P2996 automatically includes base class members
std::string json;
glz::write_json(Derived{}, json);
// {"name":"base","id":0,"extra":"derived"}

// Member names include inherited members (base first, then derived)
constexpr auto names = glz::member_names<Derived>;
// names == {"name", "id", "extra"}

constexpr auto count = glz::detail::count_members<Derived>;
// count == 3 (2 from Base + 1 from Derived)
```

### Cleaner Type Names

P2996 provides cleaner type names via `std::meta::display_string_of`:

```cpp
// Traditional: might return "Person" or "struct Person" depending on compiler
// P2996: returns "Person" consistently
constexpr auto name = glz::type_name<Person>;
```

## Docker Development Environment

A Docker container with Bloomberg clang-p2996 can be used for development and testing:

```dockerfile
FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    git cmake ninja-build python3

# Clone and build Bloomberg clang-p2996
RUN git clone https://github.com/bloomberg/clang-p2996.git /opt/llvm-project
WORKDIR /opt/llvm-project
RUN cmake -S llvm -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi"
RUN cmake --build build
```

### Running Tests in Docker

```bash
docker run --rm -v $(pwd):/glaze -w /glaze glaze-p2996-test \
    clang++ -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ \
    -I/glaze/include -DGLZ_REFLECTION26=1 \
    -Wl,-rpath,/opt/llvm-project/build/lib/aarch64-unknown-linux-gnu \
    -o /tmp/test tests/p2996_test/p2996_json_test.cpp && /tmp/test
```

## Implementation Details

### How P2996 Reflection Works

The P2996 implementation uses these key primitives:

```cpp
// Reflect on a type to get meta-info
constexpr auto type_info = ^^Person;

// Get all non-static data members
constexpr auto members = std::meta::nonstatic_data_members_of(
    ^^Person,
    std::meta::access_context::unchecked()
);

// Get member name
constexpr auto name = std::meta::identifier_of(members[0]);
// name == "name"

// Splice to access member
Person p;
p.[:members[0]:] = "Alice";  // Sets p.name
```

### Access Context

Glaze uses `access_context::unchecked()` to reflect on all members regardless of access specifiers. This allows P2996 automatic reflection to access private members without requiring friend declarations.

> **Note:** If you use explicit `glz::meta` specializations with pointer-to-member syntax (e.g., `&T::private_member`), friend declarations are still required because C++ pointer-to-member respects access control. The `access_context::unchecked()` bypass only applies to P2996 automatic reflection.

## Compatibility Notes

- P2996 support is **opt-in** and does not affect builds using standard compilers
- All existing `glz::meta` specializations continue to work
- The `glz::reflectable<T>` and `glz::has_reflect<T>` concepts work identically
- Custom serializers (`glz::to<JSON>`, `glz::from<JSON>`) are unaffected

## Future

As C++26 compilers mature and P2996 becomes widely available, it will become the preferred reflection mechanism. The traditional `__PRETTY_FUNCTION__` approach will remain for backward compatibility with C++23 compilers.

## See Also

- [Reflection in Glaze](reflection.md) - General reflection documentation
- [Pure Reflection](pure-reflection.md) - Automatic struct reflection without metadata
- [Modify Reflection](modify-reflection.md) - Customizing reflected member names
