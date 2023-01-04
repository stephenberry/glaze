# Glaze
One of the fastest JSON libraries in the world. Glaze reads and writes from C++ memory, simplifying interfaces and offering incredible performance.

## Highlights

Glaze requires C++20, using concepts for cleaner code and more helpful errors.

- Simple registration
- Standard C++ library support
- Direct to memory serialization/deserialization
- Compile time maps with constant time lookups and perfect hashing
- Nearly zero intermediate allocations
- Direct memory access through JSON pointer syntax
- Tagged binary spec through the same API for maximum performance
- Much more!

## Performance

| Library                                                      | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| ------------------------------------------------------------ | ------------------ | ------------ | ----------- |
| [**Glaze**](https://github.com/stephenberry/glaze)           | **1.42**           | **978**      | **732**     |
| [**simdjson (on demand)**](https://github.com/simdjson/simdjson) | **N/A**            | **N/A**      | **1227**    |
| [**daw_json_link**](https://github.com/beached/daw_json_link) | **2.79**           | **376**      | **481**     |
| [**RapidJSON**](https://github.com/Tencent/rapidjson)        | **3.28**           | **302**      | **620**     |
| [**json_struct**](https://github.com/jorgen/json_struct)     | **4.33**           | **242**      | **336**     |
| [**nlohmann**](https://github.com/nlohmann/json)             | **16.81**          | **89**       | **73**      |

[Performance test code available here](https://github.com/stephenberry/json_performance)

*Note: [simdjson](https://github.com/simdjson/simdjson) (on demand) is great for parsing, but can experience major performance losses when the data is not in the expected sequence (the problem grows as the file size increases, as it must re-iterate through the document). And for large, nested objects, simdjson typically requires significantly more coding from the user.*

[ABC Test](https://github.com/stephenberry/json_performance) shows how simdjson (ondemand) has poor performance when keys are not in the expected sequence:

| Library                                                      | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| ------------------------------------------------------------ | ------------------ | ------------ | ----------- |
| [**Glaze**](https://github.com/stephenberry/glaze)           | **3.10**           | **1346**     | **404**     |
| [**simdjson (on demand)**](https://github.com/simdjson/simdjson) | **N/A**            | **N/A**      | **113**     |

## Binary Performance

Tagged binary specification: [Crusher](https://github.com/stephenberry/crusher)

| Metric                | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| --------------------- | ------------------ | ------------ | ----------- |
| Raw performance       | **0.37**           | **2199**     | **1949**    |
| Equivalent JSON data* | **0.37**           | **3261**     | **2891**    |

JSON message size: 617 bytes

Binary message size: 416 bytes

*Binary data packs more efficiently than JSON, so transporting the same amount of information is even faster.

## Compiler Support

[Actions](https://github.com/stephenberry/glaze/actions) automatically build and test with [Clang](https://clang.llvm.org), [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/), and [GCC](https://gcc.gnu.org) compilers on apple, windows, and linux.

![clang build](https://github.com/stephenberry/glaze/actions/workflows/clang.yml/badge.svg) ![gcc build](https://github.com/stephenberry/glaze/actions/workflows/gcc.yml/badge.svg) ![msvc build](https://github.com/stephenberry/glaze/actions/workflows/msvc_2022.yml/badge.svg) ![msvc build](https://github.com/stephenberry/glaze/actions/workflows/msvc_2019.yml/badge.svg)

## Example

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

### Read/Write From File

```c++
glz::read_file(obj, "./obj.json"); // reads as JSON from the extension
glz::read_file_json(obj, "./obj.txt"); // reads some text file as JSON

glz::write_file(obj, "./obj.json"); // writes file based on extension
glz::write_file_json(obj, "./obj.txt"); // explicit JSON write
```

## How To Use Glaze

### [CPM](https://github.com/cpm-cmake/CPM.cmake)

```cmake
include(cmake/CPM.cmake)

CPMFindPackage(
   NAME glaze
   GIT_REPOSITORY https://github.com/stephenberry/glaze
   GIT_TAG main
)

target_link_libraries(${PROJECT_NAME} glaze::glaze)
```

> CPM will search via `find_package` first, to see if dependencies have been installed. If not, CPM will automatically pull the dependencies into your project.

### [Conan](https://conan.io)

- [Glaze Conan recipe](https://github.com/Ahajha/glaze-conan)
- Also included in [Conan Center](https://conan.io/center/)

```
find_package(glaze REQUIRED)

target_link_libraries(main PRIVATE glaze::glaze)
```

## See [Wiki](https://github.com/stephenberry/glaze/wiki) for [Frequently Asked Questions](https://github.com/stephenberry/glaze/wiki)

## Local Glaze Meta

Glaze also supports metadata provided within its associated class:

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

## Struct Registration Macros

Glaze provides macros to more efficiently register your C++ structs.

> These macros are included in the header: `glaze/core/macros.hpp`

- GLZ_META is for external registration
- GLZ_LOCAL_META is for internal registration

```c++
struct macro_t {
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;
};

GLZ_META(macro_t, x, y, z);

struct local_macro_t {
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;
   
   GLZ_LOCAL_META(local_macro_t, x, y, z);
};
```

## JSON Pointer Syntax

[Link to simple JSON pointer syntax explanation](https://github.com/stephenberry/JSON-Pointer)

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

### read_as

`read_as` allows you to read into an object from a JSON pointer and an input buffer.

```c++
Thing thing{};
glz::read_as_json(thing, "/vec3", "[7.6, 1292.1, 0.333]");
expect(thing.vec3.x == 7.6 && thing.vec3.y == 1292.1 &&
thing.vec3.z == 0.333);

glz::read_as_json(thing, "/vec3/2", "999.9");
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

Formatted JSON can be written out directly via a compile time option:

```c++
glz::write<glz::opts{.prettify = true}>(obj, buffer);
```

Or, JSON text can be formatted with the `glz::prettify` function:

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

## JSON Schema

JSON Schema can automaticly be generated for serializable named types exposed via the meta system.
```c++
std::string schema = glz::write_json_schema<my_struct>();
```

This can be used for autocomplete, linting, and validation of user input/config files in editors like VS Code that support JSON Schema.

![autocomplete example](https://user-images.githubusercontent.com/9817348/199346159-8b127c7b-a9ac-49fe-b86d-71350f0e1b10.png)

![linting example](https://user-images.githubusercontent.com/9817348/199347118-ef7e9f74-ed20-4ff5-892a-f70ff1df23b5.png)


## Array Types

Array types logically convert to JSON array values. Concepts are used to allow various containers and even user containers if they match standard library interfaces.

- `glz::array` (compile time mixed types)
- `std::tuple`
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

## Compile Time Options

The `glz::opts` struct defines compile time optional settings for reading/writing.

Instead of calling `glz::read_json(...)`, you can call `glz::read<glz::opts{}>(...)` and customize the options.

For example: `glz::read<glz::opts{.error_on_unknown_keys = false}>(...)` will turn off erroring on unknown keys and simple skip the items.

`glz::opts` can also switch between formats:

- `glz::read<glz::opts{.format = glz::binary}>(...)` -> `glz::read_binary(...)`
- `glz::read<glz::opts{.format = glz::json}>(...)` -> `glz::read_json(...)`

## Available Compile Time Options

The struct below shows the available options and the default behavior.

```c++
struct opts {
   uint32_t format = json;
   bool comments = false; // write out comments
   bool error_on_unknown_keys = true; // error when an unknown key is encountered
   bool skip_null_members = true; // skip writing out params in an object if the value is null
   bool no_except = false; // turn off and on throwing exceptions
   bool allow_hash_check = false; // Will replace some string equality checks with hash checks
   bool prettify = false;         // write out prettified JSON
   char indentation_char = ' ';   // prettified JSON indentation char
   uint8_t indentation_width = 3; // prettified JSON indentation size
   bool shrink_to_fit = false; // shrinks dynamic containers to new size to save memory
};
```

## JSON Include System

When using JSON for configuration files it can be helpful to move object definitions into separate files. This reduces copying and the need to change inputs across multiple files.

Glaze provides a `glz::file_include` type that can be added to the meta information for an object. The key may be anything, in this example we use choose `#include` to mimic C++.

```c++
struct includer_struct {
   std::string str = "Hello";
   int i = 55;
};

template <>
struct glz::meta<includer_struct> {
   using T = includer_struct;
   static constexpr auto value = object("#include", glz::file_include{}, "str", &T::str, "i", &T::i);
};
```

When this object is parsed, when the key `#include` is encountered the associated file will be read into the local object.

```c++
includer_struct obj{};
std::string s = R"({"#include": "./obj.json", "i": 100})";
glz::read_json(obj, s);
```

This will read the `./obj.json` file into the `obj` as it is parsed. Since glaze parses in sequence, the order in which includes are listed in the JSON file is the order in which they will be evaluated. The `file_include` will only be read into the local object, so includes can be deeply nested.

> Paths are always relative to the location of the previously loaded file. For nested includes this means the user only needs to consider the relative path to the file in which the include is written.

## NDJSON Support

Glaze supports [Newline Delimited JSON](http://ndjson.org) for array-like types (e.g. `std::vector` and `std::tuple`).

```c++
std::vector<std::string> x = { "Hello", "World", "Ice", "Cream" };
std::string s = glz::write_ndjson(x);
glz::read_ndjson(x, s);
```

# More Features

- Tagged binary messaging for maximum performance
- A data recorder (`recorder.hpp`)
- A generic library API
- A simple thread pool
- Studies based on JSON structures
- Eigen C++ matrix library support

# Tagged Binary Messages (Crusher)

Glaze provides a tagged binary format to send and receive messages much like JSON, but with significantly improved performance and message size savings.

The binary specification is known as [Crusher](https://github.com/stephenberry/crusher).

Integers and integer keys are locally compressed for efficiency. Elements are byte aligned, but size headers uses bit packing where the benefits are greatest and performance costs are low.

Most classes use `std::memcpy` for maximum performance.

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

glz::write_file_json(rec, "recorder_out.json");
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

# Extensions

See the `ext` directory for extensions.

- [Eigen](https://gitlab.com/libeigen/eigen). Supports fixed size matrices and vectors.

# License

Glaze is distributed under the MIT license.
