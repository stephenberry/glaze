# Glaze
An extremely fast, in memory, JSON and interface library for modern C++.

Glaze reads into C++ memory, simplifying interfaces and offering incredible performance.

Glaze requires C++20, using concepts for cleaner code and more helpful errors.

- Direct to memory serialization/deserialization
- Compile time maps with constant time lookup
- Nearly zero intermediate allocations
- Much more!

### Example

```c++
struct my_struct
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
};

template <>
struct glz::meta<my_struct> {
   using T = my_struct;
   static constexpr auto value = object(
      "i", &T::i,
      "d", &T::d,
      "hello", &T::hello,
      "arr", &T::arr
   );
};
```

**JSON Output/Input**

```json
{
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [
      1,
      2,
      3
   ]
}
```

**Write JSON**

```c++
my_struct s{};
std::string buffer{};
glz::write_json(s, buffer);
// buffer is now: {"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]}
```

**Read JSON**

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
my_struct s{};
glz::read_json(s, buffer);
// populates 's' from JSON
```

## Local Glaze Meta

Glaze also supports metadata provided within its associated class, as shown below:

```c++
struct my_struct
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
  
  struct glaze {
     using T = my_struct;
     static constexpr auto value = glz::object(
        "i", &T::i,
        "d", &T::d,
        "hello", &T::hello,
        "arr", &T::arr
     );
  };
};
```

> Template specialization of `glz::meta` is preferred when separating class definition from the serialization mapping. Local glaze metadata is helpful for working within the local namespace or when the class itself is templated.

## Header Only

Glaze is designed to be used in a header only manner. The macro `FMT_HEADER_ONLY` is used for the [fmt](https://github.com/fmtlib/fmt) library.

## Dependencies

Dependencies are automatically included when running CMake. [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) is used for dependency management.

- [fmt](https://github.com/fmtlib/fmt)
- [fast_float](https://github.com/fastfloat/fast_float)
- [frozen](https://github.com/serge-sans-paille/frozen.git)
- [NanoRange](https://github.com/tcbrindle/NanoRange)

> NanoRange is directly included until C++20 ranges are supported across all major compilers.

## Unit Test Dependencies

*Only required for building tests.*

- [UT](https://github.com/boost-ext/ut)
- [Eigen](https://gitlab.com/libeigen/eigen)

## JSON Pointer Syntax

Glaze supports JSON pointer syntax access in a C++ context. This is extremely helpful for building generic APIs, which allows components of complex arguments to be accessed without needed know the encapsulating class.

```c++
my_struct s{};
auto& d = glz::get<double>(s, "/d");
// d is a reference to d in the structure s
```

## JSON With Comments (JSONC)

Comments are supported with the specification defined here: [JSONC](https://github.com/stephenberry/JSONC)

Comments may also be included in the `glaze::meta` description for your types. These comments can be written out to provide a description of your JSON interface. Calling `write_jsonc` as opposed to `write_json` will write out any comments included in the `meta` description.

```c++
struct thing {
  double x{5.0};
  int y{7};
};

template <>
struct glz::meta<thing> {
   static constexpr auto value = object(
      using T = thing;
      "x", &T::a, "x is a double",
      "y", &T::b, "y is an int"
   );
};
```

Prettified output:

```json
{
  "a": 5 /*x is a double*/,
  "b": 7 /*y is an int*/
}
```

## Object Mapping

When using member pointers (e.g. `&T::a`) the C++ class structures must match the JSON interface. It may be desirable to map C++ classes with differing layouts to the same object interface. This is accomplished through registering lambda functions instead of member pointers.

```c++
template <>
struct glz::meta<Thing> {
   static constexpr auto value = object(
      "i", [](auto&& self) -> auto& { return self.subclass.i; }
   );
};
```

The value `v` passed to the lambda function will be a `Thing` object, and the lambda function allows us to make the subclass invisible to the object interface.

Lambda functions by default copy returns, therefore the `auto&` return type is typically required in order for glaze to write to memory.

> Note that remapping can also be achieved through pointers/references, as glaze treats values, pointers, and references in the same manner when writing/reading.

## Enums

In JSON enums are used in their string form. In binary they are used in their integer form.

```c++
enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color> {
   using enum Color;
   static constexpr auto value = enumerate("Red", Red,
                                           "Green", Green,
                                           "Blue", Blue
   );
};
```

In use:

```c++
Color color = Color::Red;
std::string buffer{};
glz::write_json(color, buffer);
expect(buffer == "\"Red\"");
```

## Prettify

`glz::prettify` formats JSON text for easier reading.

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
auto beautiful = glz::prettify(buffer);
```

`beautiful` is now:

```json
{
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [
      1,
      2,
      3
   ]
}
```

Simplified prettify definition below, which allows the use of tabs or changing the number of spaces per indent.

```c++
string prettify(auto& in, bool tabs = false, uint32_t indent_size = 3)
```

## Array Types

Array types logically convert to JSON array values. Concepts are used to allow various containers and even user containers if they match standard library interfaces.

- `glz::array` (compile time mixed types)
- `std::array`
- `std::vector`
- `std::deque`
- `std::list`
- `std::forward_list`
- `std::span`

## Object Types

Object types logically convert to JSON object values, such as maps. Like JSON, Glaze treats object definitions as unordered maps. Therefore the order of an object layout does not have to mach the same binary sequence in C++ (hence the tagged specification).

- `glz::object` (compile time mixed types)
- `std::map`
- `std::unordered_map`

## Nullable Types

Glaze supports `std::unique_ptr`, `std::shared_ptr`, and `std::optional` as nullable types. Nullable types can be allocated by JSON input or nullified by the `null` keyword.

```c++
std::unique_ptr<int> ptr{};
std::string buffer{};
glz::write_json(ptr, buffer);
expect(buffer == "null");

glz::read_json(ptr, "5");
expect(*ptr == 5);
buffer.clear();
glz::write_json(ptr, buffer);
expect(buffer == "5");

glz::read_json(ptr, "null");
expect(!bool(ptr));
```

## JSON Caveats

- Integer types cannot begin with a positive `+` symbol, for efficiency.

# More Features

- Tagged binary messaging for maximum performance
- Comma Separated Value files (CSV)
- A data recorder (logging) (`recorder.hpp`)
- A generic library API
- A simple thread pool
- Studies based on JSON structures
- A JSON file include system
- Eigen C++ matrix library support

# Tagged Binary Messages (Crusher)

Glaze provides a tagged binary format to send and receive messages much like JSON, but with significantly improved performance and message size savings.

The binary specification is known as [Crusher]() [TODO: add link].

Integers and integer keys are locally compressed for efficiency. Elements are byte aligned, but compression uses bit packing where the benefits are greatest and performance costs are low.

Most classes use `std::memcpy` for maximum performance.

Compile time known objects use integer mapping for JSON equivalent keys, significantly reducing message sizes and increasing performance.

**Write Binary**

```c++
my_struct s{};
std::vector<std::byte> buffer{};
glz::write_binary(s, buffer);
```

**Read Binary**

```c++
my_struct s{};
glz::read_binary(s, buffer);
```

## Binary Arrays

Arrays of compile time known size, e.g. `std::array`, do not include the size (number of elements) with the message. This is to enable minimal binary size if required. Dynamic types, such as `std::vector`, include the number of elements. *This means that statically sized arrays and dynamically sized arrays cannot be intermixed across implementations.*

## Partial Objects

It is sometimes desirable to write out only a portion of an object. This is permitted via an array of JSON pointers, which indicate which parts of the object should be written out.

```c++
static constexpr auto partial = glz::json_ptrs("/i",
                                               "/d",
                                               "/sub/x",
                                               "/sub/y");
std::vector<std::byte> out;
glz::write_binary<partial>(s, out);
```

# Comma Separated Value Format (CSV)

Glaze by default writes row wise files, as this is more efficient for in memory data that is written once to file. Column wise output is also supported for logging use cases.

[TODO: expand]

# Data Recorder

`record/recorder.hpp` provides an efficient recorder for mixed data types. The template argument takes a variant of supported types. However, recorder does not store recorded elements in this variant type. Instead, the variant is reinterpreted as a variant of deques of those types.

```c++
glz::recorder<std::variant<double, float>> rec;

double x = 0.0;
float y = 0.f;

rec["x"] = x;
rec["y"] = y;

for (int i = 0; i < 100; ++i) {
	x += 1.5;
	y += static_cast<float>(i);
	rec.update(); // saves the current state of x and y
}

to_csv_file("recorder_out", rec);
```

# Glaze Interfaces (Generic Library API)

Glaze has been designed to work as a generic interface for shared libraries and more. This is achieved through JSON pointer syntax access to memory.

Glaze allows a single header API (`api.hpp`) to be used for every shared library interface, greatly simplifying shared library handling.

> Interfaces are simply Glaze object types. So whatever any JSON/binary interface can automatically be used as a library API.

## Type Safety

A valid interface concern is binary compatibility between types. Glaze uses compile time hashing of types that is able to catch a wide range of changes to classes or types that would cause binary incompatibility. These compile time hashes are checked when accessing across the interface and provide a safeguard, much like a `std::any_cast`, but working across compilations.`std::any_cast` does not guarantee any safety between separately compiled code, whereas Glaze adds significant type checking across compilations and versions of compilers.

## Name

Every type accessed through a Glaze api must be named. It is best practice to give the type the same name as it has in C++, including the namespace (at least local namespace).

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

Or, include it locally:

```c++
struct my_api {
	struct glaze {
		static constexpr std::string_view name = "my_api";
	};
};
```

## Version

Glaze also always a version to be specified for types. By default all types get a version of `0.0.1`. The version tag allows the user to intentionally break API compatibility for a type for changes that would not be caught by the compile time type checking.

```c++
template <>
struct glz::meta<my_api> {
  static constexpr glz::version_t version{ 0, 0, 2 };
};
```

Or, include it locally like `name` or `value`.

## What Is Checked?

Glaze catches the following changes:

-  `name` in meta
-  `version` in meta
- the `sizeof` the type
- The compiler (clang, gcc, msvc)
- `std::is_trivial`
- `std::is_standard_layout`
- `std::is_default_constructible`
- `std::is_trivially_default_constructible`
- `std::is_nothrow_default_constructible`
- `std::is_trivially_copyable`
- `std::is_move_constructible`
- `std::is_trivially_move_constructible`
- `std::is_nothrow_move_constructible`
- `std::is_destructible`
- `std::is_trivially_destructible`
- `std::is_nothrow_destructible`
- `std::has_unique_object_representations`
- `std::is_polymorphic`
- `std::has_virtual_destructor`
- `std::is_aggregate`

## Functions

Functions do not make sense in a JSON or binary context (and are ignored in those contexts). However, Glaze allows `std::function` types to be added to objects and arrays in order to use them across Glaze APIs.

```c++
int x = 7;
double y = 5.5;
auto& f = io->get<std::function<double(int, double)>>("/f");
expect(f(x, y) == 38.5);
```

# Thread Pool

[TODO: expand]

# Design of Experiments (Studies)

[TODO: expand]

# JSON Schema (Include System)

[TODO: expand]

# Extensions

See the `ext` directory for extensions.

## Eigen

[Eigen](https://gitlab.com/libeigen/eigen) is a linear algebra library. Glaze currently supports fixed sized matrices and vectors.

# License

Glaze is distributed under the MIT license.
