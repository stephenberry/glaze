# Glaze
One of the fastest JSON libraries in the world. Glaze reads and writes from C++ memory, simplifying interfaces and offering incredible performance.

| Library                                                      | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| ------------------------------------------------------------ | ------------------ | ------------ | ----------- |
| [**Glaze**](https://github.com/stephenberry/glaze)           | **1.87**           | **635**      | **645**     |
| [**daw_json_link**](https://github.com/beached/daw_json_link) (with unsafe raw buffer) | **2.59**           | **462**      | **461**     |
| [**daw_json_link**](https://github.com/beached/daw_json_link) | **3.18**           | **317**      | **460**     |
| [**json_struct**](https://github.com/jorgen/json_struct)     | **8.31**           | **467**      | **173**     |
| [**nlohmann json**](https://github.com/nlohmann/json)        | **18.58**          | **76**       | **66**      |

[Performance test code available here](https://github.com/stephenberry/json_performance)

*daw_json_link is [significantly faster](https://github.com/beached/daw_json_link/blob/release/docs/images/kostya_bench_chart_2021_04_03.png) than libraries like [rapidjson](https://github.com/Tencent/rapidjson), so while benchmarks are coming, glaze has outperformed everything we've tested against. [simdjson](https://github.com/simdjson/simdjson) will probably be faster in a lot of reading contexts, but requires more code from the user to achieve this for nested objects*

Glaze requires C++20, using concepts for cleaner code and more helpful errors.

- Direct to memory serialization/deserialization
- Compile time maps with constant time lookups and perfect hashing
- Nearly zero intermediate allocations
- Direct memory access through JSON pointer syntax
- Tagged binary spec through the same API for maximum performance
- Much more!

## Binary Performance

*Tagged binary specification described further down.*

| Metric                | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| --------------------- | ------------------ | ------------ | ----------- |
| Raw performance       | **0.45**           | **1,697**    | **682**     |
| Equivalent JSON data* | **0.45**           | **3,222**    | **1,295**   |

JSON message size: 617 bytes

Binary message size: 325 bytes

*Binary data packs more efficiently than JSON, so transporting the same amount of information is even faster.

## Compiler Support

Glaze builds with [Clang](https://clang.llvm.org), [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/), and [GCC](https://gcc.gnu.org) compilers.

*Currently clang and gcc should perform better at writing because of MSVC compiler issues (waiting for a fix).*

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
std::string buffer = glz::write_json(s);
// buffer is now: {"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]}
```

or

```c++
my_struct s{};
std::string buffer{};
glz::write_json(s, buffer);
// buffer is now: {"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]}
```

**Read JSON**

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
auto s = glz::read_json<my_struct>(buffer);
// s is a my_struct populated from JSON
```

or

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
my_struct s{};
glz::read_json(s, buffer);
// populates s from JSON
```

## How To Use Glaze

*This documentation is subject to change*

Currently the easiest way to use Glaze with CMake is via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake).

```cmake
include(cmake/CPM.cmake)

CPMAddPackage(
   NAME glaze
   GIT_REPOSITORY https://github.com/stephenberry/glaze
   GIT_TAG main
)

target_link_libraries(${PROJECT_NAME} glaze)
```

Or, use the [Glaze Conan recipe](https://github.com/Ahajha/glaze-conan)

## Dependencies

Glaze can be used header only by linking to `fmt-header-only`. `fmt` can also be statically linked.

Dependencies are automatically included when running CMake. [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) is used for dependency management.

- [fmt](https://github.com/fmtlib/fmt)
- [fast_float](https://github.com/fastfloat/fast_float)
- [frozen](https://github.com/serge-sans-paille/frozen.git)
- [NanoRange](https://github.com/tcbrindle/NanoRange)

> NanoRange will be removed once C++20 ranges are supported across all major compilers.

## Unit Test Dependencies

*Only required for building tests.*

- [UT](https://github.com/boost-ext/ut)
- [Eigen](https://gitlab.com/libeigen/eigen)

## See [Wiki](https://github.com/stephenberry/glaze/wiki) for [Frequently Asked Questions](https://github.com/stephenberry/glaze/wiki)

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

## JSON Pointer Syntax

[Here is a simple JSON pointer syntax explanation](https://github.com/stephenberry/JSON-Pointer)

Glaze supports JSON pointer syntax access in a C++ context. This is extremely helpful for building generic APIs, which allows components of complex arguments to be accessed without needed know the encapsulating class.

```c++
my_struct s{};
auto& d = glz::get<double>(s, "/d");
// d is a reference to d in the structure s
```

```c++
my_struct s{};
glz::set(s, "/d", 42.0);
// d is now 42.0
```

> JSON pointer syntax works with deeply nested objects and anything serializable.

```c++
// Tuple Example
auto tuple = std::make_tuple(3, 2.7, std::string("curry"));
glz::set(tuple, "/0", 5);
expect(std::get<0>(tuple) == 5.0);
```

### write_from

`write_from` allows you to write to a JSON pointer via a JSON input buffer.

```c++
Thing thing{};
glz::write_from(thing, "/vec3", "[7.6, 1292.1, 0.333]");
expect(thing.vec3.x == 7.6 && thing.vec3.y == 1292.1 &&
thing.vec3.z == 0.333);

glz::write_from(thing, "/vec3/2", "999.9");
expect(thing.vec3.z == 999.9);
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
   using T = thing;
   static constexpr auto value = object(
      "x", &T::x, "x is a double",
      "y", &T::y, "y is an int"
   );
};
```

Prettified output:

```json
{
  "x": 5 /*x is a double*/,
  "y": 7 /*y is an int*/
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

The value `self` passed to the lambda function will be a `Thing` object, and the lambda function allows us to make the subclass invisible to the object interface.

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

## Error Handling

Glaze is safe to use with untrusted messages. Exceptions are thrown on errors, which can be caught and handled however you want.

Glaze also tries to be helpful and give useful information about where the error is exactly.

For example, this test case:

```json
{"Hello":"World"x, "color": "red"}
```

Produces this error:

```
1:17: Expected:,
   {"Hello":"World"x, "color": "red"}
                   ^
```

Denoting that x is invalid here.

### Raw Buffer Performance

Glaze is just about as fast writing to a `std::string` as it is writing to a raw char buffer. If you have sufficiently allocated space in your buffer you can write to the raw buffer, as shown below, but it is not recommended.

```
glz::read_json(obj, buffer);
const auto n = glz::write_json(obj, buffer.data());
buffer.resize(n);
```

## JSON Caveats

- Number types cannot begin with a positive `+` symbol, for efficiency.

## Compile Time Options

The `glz::opts` struct defines compile time optional settings for reading/writing.

Instead of calling `glz::read_json(...)`, you can call `glz::read<glz::opts{}>(...)` and customize the options.

For example: `glz::read<glz::opts{.error_on_unknown_keys = false}>(...)` will turn off erroring on unknown keys and simple skip the items.

`glz::opts` can also switch between formats:

- `glz::read<glz::opts{.format = glz::binary}>(...)` -> `glz::read_binary(...)`
- `glz::read<glz::opts{.format = glz::json}>(...)` -> `glz::read_json(...)`

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

### Row wise

```c++
std::vector<double> x, y;
std::deque<bool> z;
for (auto i = 0; i < 100; ++i) {
  const auto a = static_cast<double>(i);
  x.emplace_back(a);
  y.emplace_back(std::sin(a));
  z.emplace_back(i % 2 == 0);
}

to_csv_file("rowwise_to_file_test", "x", x, "y", y, "z", z);
```

[TODO: expand]

# Data Recorder

`record/recorder.hpp` provides an efficient recorder for mixed data types. The template argument takes all the supported types. The recorder stores the data as a variant of deques of those types. `std::deque` is used to avoid the cost of reallocating when a `std::vector` would grow, and typically a recorder is used in cases when the length is unknown.

```c++
glz::recorder<double, float> rec;

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

The API is shown below. It is simple, yet incredibly powerful, allowing pretty much any C++ class to be manipulated across the API via JSON or binary, or even the class itself to be passed and safely cast on the other side.

```c++
struct api {
  /*default constructors hidden for brevity*/
  
  template <class T>
    [[nodiscard]] T& get(const sv path);

  template <class T>
    [[nodiscard]] T* get_if(const sv path) noexcept;

  virtual bool read(const uint32_t /*format*/, const sv /*path*/,
                    const sv /*data*/) noexcept = 0;

  virtual bool write(const uint32_t /*format*/, const sv /*path*/, std::string& /*data*/) = 0;

  virtual const sv last_error() const noexcept {
    return error;
  }

  protected:
  /// unchecked void* access
  virtual void* get(const sv path, const sv type_hash) noexcept = 0;

  std::string error{};
};
```

## Type Safety

A valid interface concern is binary compatibility between types. Glaze uses compile time hashing of types that is able to catch a wide range of changes to classes or types that would cause binary incompatibility. These compile time hashes are checked when accessing across the interface and provide a safeguard, much like a `std::any_cast`, but working across compilations.`std::any_cast` does not guarantee any safety between separately compiled code, whereas Glaze adds significant type checking across compilations and versions of compilers.

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

Glaze catches the following changes:

-  `name` in meta
-  `version` in meta
- the `sizeof` the type
- All member variables names (for object types)
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

Glaze contains a simple thread pool for the sake of running studies efficiently across threads using the included JSON study code. However, the thread pool is generic and can be used for various applications. It is designed to minimize copies of the data passed to threads.

[TODO: example]

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
