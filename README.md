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
- [Tagged binary spec](./docs/binary.md) through the same API for maximum performance
- No exceptions (compiles with `-fno-exceptions`)
- If you desire helpers that throw for cleaner syntax see [Glaze Exceptions](./docs/exceptions.md)
- No runtime type information necessary (compiles with `-fno-rtti`)
- [JSON-RPC 2.0 support](./docs/json-rpc.md)
- [JSON Schema generation](./docs/json-schema.md)
- [CSV Reading/Writing](./docs/csv.md)
- Much more!

## Performance

| Library                                                      | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| ------------------------------------------------------------ | ------------------ | ------------ | ----------- |
| [**Glaze**](https://github.com/stephenberry/glaze)           | **1.23**           | **897**      | **1094**    |
| [**simdjson (on demand)**](https://github.com/simdjson/simdjson) | **N/A**            | **N/A**      | **1286**    |
| [**yyjson**](https://github.com/ibireme/yyjson)              | **1.77**           | **628**      | **1000**    |
| [**daw_json_link**](https://github.com/beached/daw_json_link) | **2.89**           | **354**      | **490**     |
| [**RapidJSON**](https://github.com/Tencent/rapidjson)        | **3.59**           | **304**      | **507**     |
| [**json_struct**](https://github.com/jorgen/json_struct)     | **4.30**           | **235**      | **332**     |
| [**nlohmann**](https://github.com/nlohmann/json)             | **15.35**          | **89**       | **85**      |

[Performance test code available here](https://github.com/stephenberry/json_performance)

*Note: [simdjson](https://github.com/simdjson/simdjson) is great for parsing, but can experience major performance losses when the data is not in the expected sequence (the problem grows as the file size increases, as it must re-iterate through the document). And for large, nested objects, simdjson typically requires significantly more coding from the user.*

[ABC Test](https://github.com/stephenberry/json_performance) shows how simdjson has poor performance when keys are not in the expected sequence:

| Library                                                      | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| ------------------------------------------------------------ | ------------------ | ------------ | ----------- |
| [**Glaze**](https://github.com/stephenberry/glaze)           | **2.25**           | **1269**     | **639**     |
| [**simdjson (on demand)**](https://github.com/simdjson/simdjson) | **N/A**            | **N/A**      | **171**     |

## Binary Performance

Tagged binary specification: [EVE](https://github.com/stephenberry/eve)

| Metric                | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| --------------------- | ------------------ | ------------ | ----------- |
| Raw performance       | **0.45**           | **2859**     | **2178**    |
| Equivalent JSON data* | **0.45**           | **3123**     | **2379**    |

JSON message size: 616 bytes

Binary message size: 564 bytes

*Binary data packs more efficiently than JSON, so transporting the same amount of information is even faster.

## Compiler Support

[Actions](https://github.com/stephenberry/glaze/actions) automatically build and test with [Clang](https://clang.llvm.org), [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/), and [GCC](https://gcc.gnu.org) compilers on apple, windows, and linux.

![clang build](https://github.com/stephenberry/glaze/actions/workflows/clang.yml/badge.svg) ![gcc build](https://github.com/stephenberry/glaze/actions/workflows/gcc.yml/badge.svg) ![msvc build](https://github.com/stephenberry/glaze/actions/workflows/msvc_2022.yml/badge.svg) 

> MSVC 2019 is no longer supported as of v1.3.0

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
if (s) // check for error
{
  s.value(); // s.value() is a my_struct populated from JSON
}
```

or

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
my_struct s{};
auto ec = glz::read_json(s, buffer);
if (ec) {
  // handle error
}
// populates s from JSON
```

### Read/Write From File

```c++
auto ec = glz::read_file(obj, "./obj.json", std::string{}); // reads as JSON from the extension
auto ec = glz::read_file_json(obj, "./obj.txt", std::string{}); // reads some text file as JSON

auto ec = glz::write_file(obj, "./obj.json", std::string{}); // writes file based on extension
auto ec = glz::write_file_json(obj, "./obj.txt", std::string{}); // explicit JSON write
```

## How To Use Glaze

### [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)
```cmake
include(FetchContent)

FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glaze)

target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)
```

### [Conan](https://conan.io)

- [Glaze Conan recipe](https://github.com/Ahajha/glaze-conan)
- Also included in [Conan Center](https://conan.io/center/)

```
find_package(glaze REQUIRED)

target_link_libraries(main PRIVATE glaze::glaze)
```

### See this [Example Repository](https://github.com/stephenberry/glaze_example) for how to use Glaze in a new project

---

## See [Wiki](https://github.com/stephenberry/glaze/wiki) for Frequently Asked Questions

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

**Macros must be explicitly included via: `#include "glaze/core/macros.hpp"`**

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
auto d = glz::get<double>(s, "/d");
// d.value() is a std::reference_wrapper to d in the structure s
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

### get_as_json

`get_as_json` allows you to get a targeted value from within an input buffer. This is especially useful if you need to change how an object is parsed based on a value within the object.

```c++
std::string s = R"({"obj":{"x":5.5}})";
auto z = glz::get_as_json<double, "/obj/x">(s);
expect(z == 5.5);
```

### get_sv_json

`get_sv_json` allows you to get a `std::string_view` to a targeted value within an input buffer. This can be more efficient to check values and handle custom parsing than constructing a new value with `get_as_json`.

```c++
std::string s = R"({"obj":{"x":5.5}})";
auto view = glz::get_sv_json<"/obj/x">(s);
expect(view == "5.5");
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

By default enums will be written and read in integer form. No `glz::meta` is necessary if this is the desired behavior.

However, if you prefer to use enums as strings in JSON, they can be registered in the `glz::meta` as follows:

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
- `std::set`
- `std::unordered_set`

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

## Value Types

A class can be treated as an underlying value as follows:

```c++
struct S {
  int x{};
};

template <>
struct glz::meta<S> {
  static constexpr auto value{ &S::x };
};
```

or using a lambda:

```c++
template <>
struct glz::meta<S> {
  static constexpr auto value = [](auto& self) -> auto& { return self.x; };
};
```

## Boolean Flags

Glaze supports registering a set of boolean flags that behave as an array of string options:

```c++
struct flags_t {
   bool x{ true };
   bool y{};
   bool z{ true };
};

template <>
struct glz::meta<flags_t> {
   using T = flags_t;
   static constexpr auto value = flags("x", &T::x, "y", &T::y, "z", &T::z);
};
```

Example:

```c++
flags_t s{};
expect(glz::write_json(s) == R"(["x","z"])");
```

Only `"x"` and `"z"` are written out, because they are true. Reading in the buffer will set the appropriate booleans.

> When writing binary, `flags` only uses one bit per boolean (byte aligned).

## Variant Handling and Type Deduction
See [Variant Handling](./docs/variant-handling.md) for details on `std::variant` support.

## Logging JSON

Sometimes you just want to write out JSON structures on the fly as efficiently as possible. Glaze provides tuple-like structures that allow you to stack allocate structures to write out JSON with high speed. These structures are named `glz::obj` for objects and `glz::arr` for arrays.

Below is an example of building an object, which also contains an array, and writing it out.

```c++
auto obj = glz::obj{"pi", 3.14, "happy", true, "name", "Stephen", "arr", glz::arr{"Hello", "World", 2}};

std::string s{};
glz::write_json(obj, s);
expect(s == R"({"pi":3.14,"happy":true,"name":"Stephen","arr":["Hello","World",2]})");
```

> This approach is significantly faster than `glz::json_t` for generic JSON. But, may not be suitable for all contexts.

## Merge

`glz::merge` allows the user to merge multiple JSON object types into a single object.

```c++
glz::obj o{"pi", 3.141};
std::map<std::string_view, int> map = {{"a", 1}, {"b", 2}, {"c", 3}};
auto merged = glz::merge{o, map};
std::string s{};
glz::write_json(merged, s); // will write out a single, merged object
// s is now: {"pi":3.141,"a":0,"b":2,"c":3}
```

> `glz::merge` stores references to lvalues to avoid copies

## Generic JSON

See [Generic JSON](./docs/generic-json.md) for `glz::json_t`.

```c++
glz::json_t json{};
std::string buffer = R"([5,"Hello World",{"pi":3.14}])";
glz::read_json(json, buffer);
assert(json[2]["pi"].get<double>() == 3.14);
```

## Error Handling

Glaze is safe to use with untrusted messages. Errors are returned as error codes, typically within a `glz::expected`, which behaves just like a `std::expected`.

To generate more helpful error messages, call `format_error`:

```c++
auto pe = glz::read_json(obj, buffer);
if (pe) {
  std::string descriptive_error = glz::format_error(pe, s);
}
```

This test case:

```json
{"Hello":"World"x, "color": "red"}
```

Produces this error:

```
1:17: syntax_error
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
      bool comments = false; // Write out comments
      bool error_on_unknown_keys = true; // Error when an unknown key is encountered
      bool skip_null_members = true; // Skip writing out params in an object if the value is null
      bool use_hash_comparison = true; // Will replace some string equality checks with hash checks
      bool prettify = false; // Write out prettified JSON
      char indentation_char = ' '; // Prettified JSON indentation char
      uint8_t indentation_width = 3; // Prettified JSON indentation size
      bool shrink_to_fit = false; // Shrinks dynamic containers to new size to save memory
      bool write_type_info = true; // Write type info for meta objects in variants
      bool force_conformance = false; // Do not allow invalid json normally accepted such as comments, nan, inf.
      bool error_on_missing_keys = false; // Require all non nullable keys to be present in the object. Use
                                          // skip_null_members = false to require nullable members
      uint32_t layout = rowwise; // CSV row wise output/input
      bool quoted = false; // treat numbers as quoted or array-like types as having quoted numbers
      bool number = false; // read numbers as strings and write these string as numbers
};
```

## Skip

It can be useful to acknowledge a keys existence in an object to prevent errors, and yet the value may not be needed or exist in C++. These cases are handled by registering a `glz::skip` type with the meta data.

```c++
struct S {
  int i{};
};

template <>
struct glz::meta<S> {
  static constexpr auto value = object("key_to_skip", skip{}, "x", &S::i);
};
```

```c++
std::string buffer = R"({"key_to_skip": [1,2,3], "i": 7})";
S s{};
glz::read_json(s, buffer);
// The value [1,2,3] will be skipped
expect(s.i == 7); // only the value i will be read into
```

## Hide

Glaze is designed to help with building generic APIs. Sometimes a value needs to be exposed to the API, but it is not desirable to read in or write out the value in JSON. This is the use case for `glz::hide`.

`glz::hide` hides the value from JSON output while still allowing API access.

```c++
struct hide_struct {
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
};

template <>
struct glz::meta<hide_struct> {
   using T = hide_struct;
   static constexpr auto value = object("i", &T::i,  //
                                        "d", &T::d, //
                                        "hello", hide{&T::hello});
};
```

```c++
hide_struct s{};
auto b = glz::write_json(s);
expect(b == R"({"i":287,"d":3.14})"); // notice that "hello" is hidden from the output
```

## Quoted Numbers

You can parse quoted JSON numbers directly to types like `double`, `int`, etc. by utilizing the `glz::quoted` wrapper.

```c++
struct A {
   double x;
   std::vector<uint32_t> y;
};

template <>
struct glz::meta<A> {
   static constexpr auto value = object("x", glz::quoted<&A::x>(), "y", glz::quoted<&A::y>());
};
```

```json
{
  "x": "3.14",
  "y": ["1", "2", "3"]
}
```

The quoted JSON numbers will be parsed directly into the `double` and `std::vector<uint32_t>`. The `glz::quoted` function works for nested objects and arrays as well.

## NDJSON Support

Glaze supports [Newline Delimited JSON](http://ndjson.org) for array-like types (e.g. `std::vector` and `std::tuple`).

```c++
std::vector<std::string> x = { "Hello", "World", "Ice", "Cream" };
std::string s = glz::write_ndjson(x);
glz::read_ndjson(x, s);
```

# More Features

### [Tagged Binary Messages](./docs/binary.md)

### [Shared Library API](./docs/glaze-interfaces.md)

### [Thread Pool](./docs/thread-pool.md)

### [Data Recorder](./docs/recorder.md)

### [JSON-RPC 2.0](./docs/json-rpc.md)

### [JSON Schema](./docs/json-schema.md)

### [JSON Include System](./docs/json-include.md)

# Extensions

See the `ext` directory for extensions.

- [Eigen](https://gitlab.com/libeigen/eigen). Supports reading and writing from Eigen Vector types.
- [JSON-RPC documentation](./docs/json-rpc.md). Glaze wrapper supporting [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification).

# License

Glaze is distributed under the MIT license.
