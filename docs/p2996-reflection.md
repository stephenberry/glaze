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

- **GCC 16+**: Reflection support merged into GCC trunk
  - Available via the [Ubuntu Toolchain PPA](https://launchpad.net/~ubuntu-toolchain-r/+archive/ubuntu/ppa) or by building from source
  - See [GCC 16 changes](https://gcc.gnu.org/gcc-16/changes.html) for the full list of supported reflection proposals
- **Bloomberg clang-p2996**: Experimental Clang fork with P2996 support
  - Repository: https://github.com/bloomberg/clang-p2996
  - Docker image: `vsavkov/clang-p2996:amd64`

### Compiler Flags

**GCC 16+:**
```bash
g++-16 -std=c++26 -freflection
```

**Bloomberg clang-p2996:**
```bash
clang++ -std=c++26 -freflection -fexpansion-statements -stdlib=libc++
```

| Flag | Purpose |
|------|---------|
| `-std=c++26` | Enable C++26 mode |
| `-freflection` | Enable P2996 reflection |
| `-fexpansion-statements` | Enable expansion statements (Bloomberg Clang only) |
| `-stdlib=libc++` | Required for `<meta>` header (Bloomberg Clang only) |

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
| Inheritance | Requires explicit `glz::meta` | Automatic (base members first, then the type's own) |
| Member name extraction | `__PRETTY_FUNCTION__` parsing | `std::meta::identifier_of` |
| Member count | Structured binding probe | Number of members across the hierarchy |
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

> **Note:** Two shapes need a `glz::meta`. The first is a base whose second subobject is reachable
> through a non-virtual edge — a base inherited twice *non*-virtually that holds members, or one that
> another path reaches virtually and this one does not. It has two subobjects, whose members the
> compiler refuses to name through the derived type, while a base reached virtually on every path is
> a single shared subobject and is reflected once, and the same holds inside a repeated base: members
> that all come from a virtual base of it are reflected once as well. Reflecting a type of the refused
> shape is refused with a `static_assert` that names it, rather than handing out a count that leaves
> one of the subobjects out. An empty base repeated non-virtually reflects nothing twice and stays
> reflectable. A `glz::meta` written for the refused shape has to reach the members through lambdas —
> `&B::a` is an `int A::*` when `a` is declared in `A`, so applying it to the derived type is
> ambiguous, while `[](auto& self) -> auto& { return self.C::a; }` names one of the two subobjects.
>
> The second is two members with one name: a member that hides a member of a base class is a member of
> its own, and two bases can repeat a name as well, so no key can be told from the other. The writer
> emits that key twice while the keyed reader cannot be instantiated for the type at all, so give such
> a type a `glz::meta` that names its members apart, such as
> `glz::object("base_x", &Base::x, "x", &Derived::x)`. The array-shaped writes carry the members
> positionally and still round-trip, so it is only the keyed formats that need the `glz::meta`.
>
> A `glz::meta` written for a base class is not consulted for a derived type that is reflected
> automatically, because the reflection reads the base's data members: the same base serializes as
> `{"renamed":1}` on its own and as `{"raw":1,"extra":2}` inside such a derived type.
>
> Inheriting now means reflecting what the base holds, so a base whose members glaze cannot serialize
> — a `std::mutex`, say — breaks its derived types at compile time. The escape is the same: a
> `glz::meta` for the derived type that names the members to keep.
>
> Two consequences of the base members being part of the reflection are worth knowing when upgrading.
> `glz::meta<T>::modify` layers on top of the inherited members as well, so a derived type that used
> `modify` writes its base members where it did not before. Both that and the array-shaped writes
> (`structs_as_arrays`, `glz::reflect_array`, which gain one element per inherited member) change what
> is stored for a derived type. A payload an earlier version wrote for the array-shaped writes is
> rejected when read, because the element count no longer matches. A keyed payload still reads without
> an error, but the members that used to be left out are simply absent from it, so they keep the value
> they were already holding.

### Automatic Enum String Serialization

With P2996 enabled, enums can be automatically serialized as strings without writing any `glz::meta` specializations. This uses the `reflect_enums` option.

Since `reflect_enums` is not part of the base `glz::opts`, you enable it by creating a custom opts struct:

```cpp
struct reflect_enums_opts : glz::opts {
   bool reflect_enums = true;
};

enum class Color { Red, Green, Blue };

Color c = Color::Green;

// Write enum as string
auto json = glz::write<reflect_enums_opts{}>(c).value_or("error");
// json == "\"Green\""

// Read string back to enum
Color c2;
glz::read<reflect_enums_opts{}>(c2, json);
// c2 == Color::Green
```

Enums in structs also work:

```cpp
struct Pixel {
   int x;
   int y;
   Color color;
};

Pixel p{10, 20, Color::Blue};
auto json = glz::write<reflect_enums_opts{}>(p).value_or("error");
// {"x":10,"y":20,"color":"Blue"}
```

Writing a value that is not a named enumerator (e.g. `static_cast<Color>(7)`) with `reflect_enums` fails with `glz::error_code::unexpected_enum`, since it could not be read back.

P2996 also provides `enum_to_string` and `string_to_enum` utility functions:

```cpp
constexpr auto name = glz::enum_to_string(Color::Red);
// name == "Red"

constexpr auto value = glz::string_to_enum<Color>("Blue");
// value == Color::Blue

constexpr auto invalid = glz::string_to_enum<Color>("Invalid");
// invalid == std::nullopt
```

> **Note:** Without the `reflect_enums` option, enums without `glz::meta` specializations are still serialized as their underlying integer values, even when P2996 is enabled.

### Cleaner Type Names

P2996 provides cleaner type names via `std::meta::display_string_of`:

```cpp
// Traditional: might return "Person" or "struct Person" depending on compiler
// P2996: returns "Person" consistently
constexpr auto name = glz::type_name<Person>;
```

> **Breaking Change:** P2996 `type_name` returns unqualified names without namespace prefixes, while traditional reflection includes them:
> - Traditional: `"mylib::MyEnum"`
> - P2996: `"MyEnum"`
>
> Code that depends on the exact format of type names (e.g., for key generation in `rename_key`) may produce different output.

### Qualified Type Names Option

> **Note:** The `qualified_type_names` option is prepared for future P2996 implementations. Bloomberg clang-p2996 does not yet support `std::meta::qualified_name_of`, so this option currently has no effect with P2996. Traditional reflection always returns qualified names regardless of this option.

The `qualified_type_names` option and associated functions (`type_name_for_opts`, `name_for_opts`) are available for forward compatibility:

```cpp
struct my_opts : glz::opts {
    bool qualified_type_names = true;
};

// These functions are ready for when qualified_name_of becomes available
constexpr auto name = glz::type_name_for_opts<mylib::MyType, my_opts{}>();
constexpr auto name2 = glz::name_for_opts<mylib::MyType, my_opts{}>();
```

When `std::meta::qualified_name_of` is added to the P2996 implementation, these functions will automatically support returning fully-qualified type names with namespace prefixes.

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

// Get all non-static data members, the inherited ones included
// (glz::detail::all_members_of walks std::meta::bases_of depth-first before asking for the members
// the type declares itself, and skips a base it has already reached)
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
- [Automatic Enum String Serialization](enum-reflection.md) - Enum reflection via external libraries
