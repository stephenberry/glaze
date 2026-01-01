# Glaze
One of the fastest JSON libraries in the world. Glaze reads and writes from object memory, simplifying interfaces and offering incredible performance.

Glaze also supports:

- [BEVE](https://github.com/beve-org/beve) (Binary Efficient Versatile Encoding)
- [CBOR](https://stephenberry.github.io/glaze/cbor/) (Concise Binary Object Representation)
- [CSV](https://stephenberry.github.io/glaze/csv/) (Comma Separated Value)
- [MessagePack](https://stephenberry.github.io/glaze/msgpack/)
- [Stencil/Mustache](https://stephenberry.github.io/glaze/stencil-mustache/) (string interpolation)
- [TOML](https://stephenberry.github.io/glaze/toml/) (Tom's Obvious, Minimal Language)
- [EETF](https://stephenberry.github.io/glaze/EETF/erlang-external-term-format/) (Erlang External Term Format) [optionally included]
- [And Many More Features](https://stephenberry.github.io/glaze/)

> [!NOTE]
>
> Glaze is getting HTTP support with REST servers, clients, websockets, and more. The networking side of Glaze is under active development, and while it is usable and feedback is desired, the API is likely to be changing and improving.

> [!TIP]
>
> **New: Streaming I/O Support** - Glaze now supports streaming serialization and deserialization for processing large files with bounded memory usage. Write JSON/BEVE directly to output streams with automatic flushing, or read from input streams with automatic refilling. See [Streaming I/O](https://stephenberry.github.io/glaze/streaming/) for details.

## With compile time reflection for MSVC, Clang, and GCC!

- Read/write aggregate initializable structs without writing any metadata or macros!
- See [example on Compiler Explorer](https://gcc.godbolt.org/z/T4To5fKfz)

## [📖 Documentation](https://stephenberry.github.io/glaze/)

See this README, the [Glaze Documentation Page](https://stephenberry.github.io/glaze/), or [docs folder](https://github.com/stephenberry/glaze/tree/main/docs) for documentation.

## Highlights

- Pure, compile time reflection for structs
  - Powerful meta specialization system for custom names and behavior
- JSON [RFC 8259](https://datatracker.ietf.org/doc/html/rfc8259) compliance with UTF-8 validation
- Standard C++ library support
- Header only
- Direct to memory serialization/deserialization
- Compile time maps with constant time lookups and perfect hashing
- Powerful wrappers to modify read/write behavior ([Wrappers](https://stephenberry.github.io/glaze/wrappers/))
- Use your own custom read/write functions ([Custom Read/Write](#custom-readwrite))
- [Handle unknown keys](https://stephenberry.github.io/glaze/unknown-keys/) in a fast and flexible manner
- Direct memory access through [JSON pointer syntax](https://stephenberry.github.io/glaze/json-pointer-syntax/)
- [JMESPath](https://stephenberry.github.io/glaze/JMESPath/) querying
- No exceptions (compiles with `-fno-exceptions`)
  - If you desire helpers that throw for cleaner syntax see [Glaze Exceptions](https://stephenberry.github.io/glaze/exceptions/)
- No runtime type information necessary (compiles with `-fno-rtti`)
- [JSON Schema generation](https://stephenberry.github.io/glaze/json-schema/)
- [Partial Read](https://stephenberry.github.io/glaze/partial-read/) and [Partial Write](https://stephenberry.github.io/glaze/partial-write/) support
- [Streaming I/O](https://stephenberry.github.io/glaze/streaming/) for reading/writing large files with bounded memory
- [Much more!](#more-features)

## Performance

| Library                                                      | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| ------------------------------------------------------------ | ------------------ | ------------ | ----------- |
| [**Glaze**](https://github.com/stephenberry/glaze)           | **1.01**           | **1396**     | **1200**    |
| [**simdjson (on demand)**](https://github.com/simdjson/simdjson) | **N/A**            | **N/A**      | **1163**    |
| [**yyjson**](https://github.com/ibireme/yyjson)              | **1.22**           | **1023**     | **1106**    |
| [**reflect_cpp**](https://github.com/getml/reflect-cpp)      | **3.15**           | **488**      | **365**     |
| [**daw_json_link**](https://github.com/beached/daw_json_link) | **3.29**           | **334**      | **479**     |
| [**RapidJSON**](https://github.com/Tencent/rapidjson)        | **3.76**           | **289**      | **416**     |
| [**json_struct**](https://github.com/jorgen/json_struct)     | **5.87**           | **178**      | **316**     |
| [**Boost.JSON**](https://boost.org/libs/json)                | **5.38**           | **198**      | **308**     |
| [**nlohmann**](https://github.com/nlohmann/json)             | **15.44**          | **86**       | **81**      |

[Performance test code available here](https://github.com/stephenberry/json_performance)

*Performance caveats: [simdjson](https://github.com/simdjson/simdjson) and [yyjson](https://github.com/ibireme/yyjson) are great, but they experience major performance losses when the data is not in the expected sequence or any keys are missing (the problem grows as the file size increases, as they must re-iterate through the document).*

*Also, [simdjson](https://github.com/simdjson/simdjson) and [yyjson](https://github.com/ibireme/yyjson) do not support automatic escaped string handling, so if any of the currently non-escaped strings in this benchmark were to contain an escape, the escapes would not be handled.*

[ABC Test](https://github.com/stephenberry/json_performance) shows how simdjson has poor performance when keys are not in the expected sequence:

| Library                                                      | Read (MB/s) |
| ------------------------------------------------------------ | ----------- |
| [**Glaze**](https://github.com/stephenberry/glaze)           | **1219**    |
| [**simdjson (on demand)**](https://github.com/simdjson/simdjson) | **89**      |

## Binary Performance

Tagged binary specification: [BEVE](https://github.com/stephenberry/beve)

| Metric                | Roundtrip Time (s) | Write (MB/s) | Read (MB/s) |
| --------------------- | ------------------ | ------------ | ----------- |
| Raw performance       | **0.42**           | **3235**     | **2468**    |
| Equivalent JSON data* | **0.42**           | **3547**     | **2706**    |

JSON size: 670 bytes

BEVE size: 611 bytes

*BEVE packs more efficiently than JSON, so transporting the same data is even faster.

## Examples

> [!TIP]
>
> See the [example_json](https://github.com/stephenberry/glaze/blob/main/tests/example_json/example_json.cpp) unit test for basic examples of how to use Glaze. See [json_test](https://github.com/stephenberry/glaze/blob/main/tests/json_test/json_test.cpp) for an extensive test of features.

Your struct will automatically get reflected! No metadata is required by the user.

```c++
struct my_struct
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
  std::map<std::string, int> map{{"one", 1}, {"two", 2}};
};
```

**JSON** (prettified)

```json
{
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [
      1,
      2,
      3
   ],
   "map": {
      "one": 1,
      "two": 2
   }
}
```

**Write JSON**

```c++
my_struct s{};
std::string buffer = glz::write_json(s).value_or("error");
```

or

```c++
my_struct s{};
std::string buffer{};
auto ec = glz::write_json(s, buffer);
if (ec) {
  // handle error
}
```

**Read JSON**

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3],"map":{"one":1,"two":2}})";
auto s = glz::read_json<my_struct>(buffer);
if (s) // check std::expected
{
  s.value(); // s.value() is a my_struct populated from buffer
}
```

or

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3],"map":{"one":1,"two":2}})";
my_struct s{};
auto ec = glz::read_json(s, buffer); // populates s from buffer
if (ec) {
  // handle error
}
```

### Read/Write From File

```c++
auto ec = glz::read_file_json(obj, "./obj.json", std::string{});
auto ec = glz::write_file_json(obj, "./obj.json", std::string{});
```

> [!IMPORTANT]
>
> The file name (2nd argument), must be null terminated.

### Writing to Streams

For streaming to `std::ostream` destinations (files, network, etc.) with bounded memory:

```c++
#include "glaze/core/ostream_buffer.hpp"

std::ofstream file("output.json");
glz::basic_ostream_buffer<std::ofstream> buffer(file);  // Concrete type for performance
auto ec = glz::write_json(obj, buffer);
```

The buffer flushes incrementally during serialization, enabling arbitrarily large outputs with fixed memory. See [Streaming I/O](https://stephenberry.github.io/glaze/streaming/) for details on buffer types and stream concepts.

### Reading from Streams

For streaming from `std::istream` sources (files, network, etc.) with bounded memory:

```c++
#include "glaze/core/istream_buffer.hpp"

std::ifstream file("input.json");
glz::basic_istream_buffer<std::ifstream> buffer(file);
my_struct obj;
auto ec = glz::read_json(obj, buffer);
```

The buffer refills automatically during parsing, enabling reading of arbitrarily large inputs with fixed memory. See [Streaming I/O](https://stephenberry.github.io/glaze/streaming/) for NDJSON processing and other streaming patterns.

## Compiler/System Support

- Requires C++23
- Tested for both 64bit and 32bit
- Supports both little-endian and big-endian systems

[Actions](https://github.com/stephenberry/glaze/actions) build and test with [Clang](https://clang.llvm.org) (18+), [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/) (2022), and [GCC](https://gcc.gnu.org) (13+) on apple, windows, and linux. Big-endian is tested via QEMU emulation on s390x.

![clang build](https://github.com/stephenberry/glaze/actions/workflows/clang.yml/badge.svg) ![gcc build](https://github.com/stephenberry/glaze/actions/workflows/gcc.yml/badge.svg) ![msvc build](https://github.com/stephenberry/glaze/actions/workflows/msvc.yml/badge.svg) 

> Glaze seeks to maintain compatibility with the latest three versions of GCC and Clang, as well as the latest version of MSVC and Apple Clang (Xcode). And, we aim to only drop old versions with major releases.

### MSVC Compiler Flags

Glaze requires a C++ standard conformant pre-processor, which requires the `/Zc:preprocessor` flag when building with MSVC.

### SIMD CMake Options

The CMake has the option `glaze_ENABLE_AVX2`. This will attempt to use `AVX2` SIMD instructions in some cases to improve performance, as long as the system you are configuring on supports it. Set this option to `OFF` to disable the AVX2 instruction set, such as if you are cross-compiling for Arm. If you aren't using CMake the macro `GLZ_USE_AVX2` enables the feature if defined.

### Disable Forced Inlining

For faster compilation at the cost of peak performance, use `glaze_DISABLE_ALWAYS_INLINE`:

```cmake
set(glaze_DISABLE_ALWAYS_INLINE ON)
```

> **Note:** This primarily reduces compilation time, not binary size. For binary size reduction, use the `linear_search` option. See [Optimizing Performance](https://stephenberry.github.io/glaze/optimizing-performance/) for more details.

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

- Included in [Conan Center](https://conan.io/center/) ![Conan Center](https://img.shields.io/conan/v/glaze)

```
find_package(glaze REQUIRED)

target_link_libraries(main PRIVATE glaze::glaze)
```

### [build2](https://build2.org)

- Available on [cppget](https://cppget.org/libglaze)

```
import libs = libglaze%lib{glaze}
```

### Arch Linux

- [Official Arch repository](https://archlinux.org/packages/extra/any/glaze/)
- AUR git package: [glaze-git](https://aur.archlinux.org/packages/glaze-git)

### See this [Example Repository](https://github.com/stephenberry/glaze_example) for how to use Glaze in a new project

---

## See [FAQ](https://stephenberry.github.io/glaze/FAQ/) for Frequently Asked Questions

# Explicit Metadata

If you want to specialize your reflection then you can **optionally** write the code below:

> This metadata is also necessary for non-aggregate initializable structs.

```c++
template <>
struct glz::meta<my_struct> {
   using T = my_struct;
   static constexpr auto value = object(
      &T::i,
      &T::d,
      &T::hello,
      &T::arr,
      &T::map
   );
};
```

## Local Glaze Meta

<details><summary>Glaze also supports metadata within its associated class:</summary>

```c++
struct my_struct
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
  std::map<std::string, int> map{{"one", 1}, {"two", 2}};
  
  struct glaze {
     using T = my_struct;
     static constexpr auto value = glz::object(
        &T::i,
        &T::d,
        &T::hello,
        &T::arr,
        &T::map
     );
  };
};
```

</details>

## Custom Key Names or Unnamed Types

When you define Glaze metadata, objects will automatically reflect the non-static names of your member object pointers. However, if you want custom names or you register lambda functions or wrappers that do not provide names for your fields, you can optionally add field names in your metadata.

Example of custom names:

```c++
template <>
struct glz::meta<my_struct> {
   using T = my_struct;
   static constexpr auto value = object(
      "integer", &T::i,
      "double", &T::d,
      "string", &T::hello,
      "array", &T::arr,
      "my map", &T::map
   );
};
```

> Each of these strings is optional and can be removed for individual fields if you want the name to be reflected.
>
> Names are required for:
>
> - static constexpr member variables
> - [Wrappers](https://stephenberry.github.io/glaze/wrappers/)
> - Lambda functions

### Extending pure reflection with `modify`

If you only need to tweak a couple of fields, you can layer those changes on top of the automatically reflected members with `glz::meta<T>::modify`:

```c++
struct server_status
{
   std::string name;
   std::string region;
   uint64_t active_sessions{};
   std::optional<std::string> maintenance;
   double cpu_percent{};
};

template <> struct glz::meta<server_status>
{
   static constexpr auto modify = glz::object(
      "maintenance_alias", [](auto& self) -> auto& { return self.maintenance; },
      "cpuPercent", &server_status::cpu_percent
   );
};
```

Serialising

```c++
server_status status{
   .name = "edge-01",
   .region = "us-east",
   .active_sessions = 2412,
   .maintenance = std::string{"scheduled"},
   .cpu_percent = 73.5,
};
```

produces

```json
{
  "name": "edge-01",
  "region": "us-east",
  "active_sessions": 2412,
  "maintenance": "scheduled",
  "cpu_percent": 73.5,
  "maintenance_alias": "scheduled",
  "cpuPercent": 73.5
}
```

All the untouched members (`name`, `region`, `active_sessions`, `maintenance`, `cpu_percent`) still come from pure reflection, so adding or removing members later keeps working automatically. Only the extra keys provided in `modify` are layered on top.

# Reflection API

Glaze provides a compile time reflection API that can be modified via `glz::meta` specializations. This reflection API uses pure reflection unless a `glz::meta` specialization is provided, in which case the default behavior is overridden by the developer.

```c++
static_assert(glz::reflect<my_struct>::size == 5); // Number of fields
static_assert(glz::reflect<my_struct>::keys[0] == "i"); // Access keys
```

> [!WARNING]
>
> The `glz::reflect` fields described above have been formalized and are unlikely to change. Other fields may evolve as we continue to formalize the spec.

## glz::for_each_field

```c++
struct test_type {
   int32_t int1{};
   int64_t int2{};
};

test_type var{42, 43};

glz::for_each_field(var, [](auto& field) {
    field += 1;
});

expect(var.int1 == 43);
expect(var.int2 == 44);
```

# Custom Read/Write

Custom reading and writing can be achieved through the powerful `to`/`from` specialization approach, which is described here: [custom-serialization.md](https://github.com/stephenberry/glaze/blob/main/docs/custom-serialization.md). However, this only works for user defined types.

For common use cases or cases where a specific member variable should have special reading and writing, you can use [glz::custom](https://github.com/stephenberry/glaze/blob/main/docs/wrappers.md#custom) to register read/write member functions, std::functions, or lambda functions.

<details><summary>See example:</summary>

```c++
struct custom_encoding
{
   uint64_t x{};
   std::string y{};
   std::array<uint32_t, 3> z{};
   
   void read_x(const std::string& s) {
      x = std::stoi(s);
   }
   
   uint64_t write_x() {
      return x;
   }
   
   void read_y(const std::string& s) {
      y = "hello" + s;
   }
   
   auto& write_z() {
      z[0] = 5;
      return z;
   }
};

template <>
struct glz::meta<custom_encoding>
{
   using T = custom_encoding;
   static constexpr auto value = object("x", custom<&T::read_x, &T::write_x>, //
                                        "y", custom<&T::read_y, &T::y>, //
                                        "z", custom<&T::z, &T::write_z>);
};

suite custom_encoding_test = [] {
   "custom_reading"_test = [] {
      custom_encoding obj{};
      std::string s = R"({"x":"3","y":"world","z":[1,2,3]})";
      expect(!glz::read_json(obj, s));
      expect(obj.x == 3);
      expect(obj.y == "helloworld");
      expect(obj.z == std::array<uint32_t, 3>{1, 2, 3});
   };
   
   "custom_writing"_test = [] {
      custom_encoding obj{};
      std::string s = R"({"x":"3","y":"world","z":[1,2,3]})";
      expect(!glz::read_json(obj, s));
      std::string out{};
      expect(not glz::write_json(obj, out));
      expect(out == R"({"x":3,"y":"helloworld","z":[5,2,3]})");
   };
};
```

</details>

<details><summary>Another example with constexpr lambdas:</summary>

```c++
struct custom_buffer_input
{
   std::string str{};
};

template <>
struct glz::meta<custom_buffer_input>
{
   static constexpr auto read_x = [](custom_buffer_input& s, const std::string& input) { s.str = input; };
   static constexpr auto write_x = [](auto& s) -> auto& { return s.str; };
   static constexpr auto value = glz::object("str", glz::custom<read_x, write_x>);
};

suite custom_lambdas_test = [] {
   "custom_buffer_input"_test = [] {
      std::string s = R"({"str":"Hello!"})";
      custom_buffer_input obj{};
      expect(!glz::read_json(obj, s));
      expect(obj.str == "Hello!");
      s.clear();
      expect(!glz::write_json(obj, s));
      expect(s == R"({"str":"Hello!"})");
      expect(obj.str == "Hello!");
   };
};
```

</details>

### Error handling with `glz::custom`

Developers can throw errors, but for builds that disable exceptions or if it is desirable to integrate error handling within Glaze's `context`, the last argument of custom lambdas may be a `glz::context&`. This enables custom error handling that integrates well with the rest of Glaze.

<details><summary>See example:</summary>

```c++
struct age_custom_error_obj
{
   int age{};
};

template <>
struct glz::meta<age_custom_error_obj>
{
   using T = age_custom_error_obj;
   static constexpr auto read_x = [](T& s, int age, glz::context& ctx) {
      if (age < 21) {
         ctx.error = glz::error_code::constraint_violated;
         ctx.custom_error_message = "age too young";
      }
      else {
         s.age = age;
      }
   };
   static constexpr auto value = object("age", glz::custom<read_x, &T::age>);
};
```

In use:
```c++
age_custom_error_obj obj{};
std::string s = R"({"age":18})";
auto ec = glz::read_json(obj, s);
auto err_msg = glz::format_error(ec, s);
std::cout << err_msg << '\n';
```

Console output:
```
1:10: constraint_violated
   {"age":18}
            ^ age too young
```

</details>

# Object Mapping

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

# Value Types

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

# Read Constraints

Glaze provides a wrapper to enable complex reading constraints for struct members: `glz::read_constraint`.

<details><summary>See example:</summary>

```c++
struct constrained_object
{
   int age{};
   std::string name{};
};

template <>
struct glz::meta<constrained_object>
{
   using T = constrained_object;
   static constexpr auto limit_age = [](const T&, int age) {
      return (age >= 0 && age <= 120);
   };
   
   static constexpr auto limit_name = [](const T&, const std::string& name) {
      return name.size() <= 8;
   };
   
   static constexpr auto value = object("age", read_constraint<&T::age, limit_age, "Age out of range">, //
                                        "name", read_constraint<&T::name, limit_name, "Name is too long">);
};
```

For invalid input such as `{"age": -1, "name": "Victor"}`, Glaze will outut the following formatted error message:

```
1:11: constraint_violated
   {"age": -1, "name": "Victor"}
             ^ Age out of range
```

- Member functions can also be registered as the constraint. 
- The first field of the constraint lambda is the parent object, allowing complex constraints to be written by the user.

</details>

# Reading/Writing Private Fields

Serialize and deserialize private fields by making a `glz::meta<T>` and adding `friend struct glz::meta<T>;` to your class.

<details><summary>See example:</summary>

```c++
class private_fields_t
{
private:
   double cash = 22.0;
   std::string currency = "$";

   friend struct glz::meta<private_fields_t>;
};

template <>
struct glz::meta<private_fields_t>
{
   using T = private_fields_t;
   static constexpr auto value = object(&T::cash, &T::currency);
};

suite private_fields_tests = []
{
   "private fields"_test = [] {
      private_fields_t obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"cash":22,"currency":"$"})");
      
      buffer = R"({"cash":2200.0, "currency":"¢"})";
      expect(not glz::read_json(obj, buffer));
      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"cash":2200,"currency":"¢"})");
   };
};
```

</details>

# Error Handling

Glaze is safe to use with untrusted messages. Errors are returned as error codes, typically within a `glz::expected`, which behaves just like a `std::expected`.

> Glaze works to short circuit error handling, which means the parsing exits very rapidly if an error is encountered.

To generate more helpful error messages, call `format_error`:

```c++
auto pe = glz::read_json(obj, buffer);
if (pe) {
  std::string descriptive_error = glz::format_error(pe, buffer);
}
```

This test case:

```json
{"Hello":"World"x, "color": "red"}
```

Produces this error:

```
1:17: expected_comma
   {"Hello":"World"x, "color": "red"}
                   ^
```

Denoting that x is invalid here.

## Bytes Consumed on Read

The `error_ctx` type returned by read operations includes a `count` field that indicates the byte position in the input buffer:

```c++
std::string buffer = R"({"x":1,"y":2})";
my_struct obj{};
auto ec = glz::read_json(obj, buffer);
if (!ec) {
   // Success: ec.count contains bytes consumed
   size_t bytes_consumed = ec.count;
   // bytes_consumed == 13 (entire JSON object)
}
```

This is useful for:
- **Streaming**: Reading multiple JSON values from a single buffer
- **Partial parsing**: Knowing where parsing stopped with `partial_read` option
- **Error diagnostics**: On failure, `count` indicates where the error occurred

# Input Buffer (Null) Termination

A non-const `std::string` is recommended for input buffers, as this allows Glaze to improve performance with temporary padding and the buffer will be null terminated.

## JSON

By default the option `null_terminated` is set to `true` and null-terminated buffers must be used when parsing JSON. The option can be turned off with a small loss in performance, which allows non-null terminated buffers:

```c++
constexpr glz::opts options{.null_terminated = false};
auto ec = glz::read<options>(value, buffer); // read in a non-null terminated buffer
```

## BEVE

Null-termination is not required for BEVE. It makes no difference in performance.

## CBOR

Null-termination is not required for CBOR. It makes no difference in performance.

## CSV

Null-termination is not required for CSV. It makes no difference in performance.


# Type Support

## Array Types

Array types logically convert to JSON array values. Concepts are used to allow various containers and even user containers if they match standard library interfaces.

- `glz::array` (compile time mixed types)
- `std::tuple` (compile time mixed types)
- `std::array`
- `std::vector`
- `std::deque`
- `std::list`
- `std::forward_list`
- `std::span`
- `std::set`
- `std::unordered_set`

## Object Types

Object types logically convert to JSON object values, such as maps. Like JSON, Glaze treats object definitions as unordered maps. Therefore the order of an object layout does not have to match the same binary sequence in C++.

- `glz::object` (compile time mixed types)
- `std::map`
- `std::unordered_map`
- `std::pair` (enables dynamic keys in stack storage)

> `std::pair` is handled as an object with a single key and value, but when `std::pair` is used in an array, Glaze concatenates the pairs into a single object. `std::vector<std::pair<...>>` will serialize as a single  object. If you don't want this behavior set the compile time option `.concatenate = false`.

## Variants

- `std::variant`

See [Variant Handling](https://stephenberry.github.io/glaze/variant-handling/) for more information.

## Nullable Types

- `std::unique_ptr`
- `std::shared_ptr`
- `std::optional`

Nullable types may be allocated by valid input or nullified by the `null` keyword.

```c++
std::unique_ptr<int> ptr{};
std::string buffer{};
expect(not glz::write_json(ptr, buffer));
expect(buffer == "null");

expect(not glz::read_json(ptr, "5"));
expect(*ptr == 5);
buffer.clear();
expect(not glz::write_json(ptr, buffer));
expect(buffer == "5");

expect(not glz::read_json(ptr, "null"));
expect(!bool(ptr));
```

## Enums

By default enums will be written and read in integer form. No `glz::meta` is necessary if this is the desired behavior.

However, if you prefer to use enums as strings in JSON, they can be registered in the `glz::meta` as follows:

```c++
enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color> {
   using enum Color;
   static constexpr auto value = enumerate(Red,
                                           Green,
                                           Blue
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

> [!TIP]
>
> For automatic enum-to-string serialization without writing metadata, consider using [simple_enum](https://github.com/arturbac/simple_enum), which provides Glaze integration.

# JSON With Comments (JSONC)

Comments are supported with the specification defined here: [JSONC](https://github.com/stephenberry/JSONC)

Read support for comments is provided with `glz::read_jsonc` or `glz::read<glz::opts{.comments = true}>(...)`.

# Prettify JSON

Formatted JSON can be written out directly via a compile time option:

```c++
auto ec = glz::write<glz::opts{.prettify = true}>(obj, buffer);
```

Or, JSON text can be formatted with the `glz::prettify_json` function:

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
auto beautiful = glz::prettify_json(buffer);
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

# Minify JSON

To write minified JSON:

```c++
auto ec = glz::write_json(obj, buffer); // default is minified
```

To minify JSON text call:

```c++
std::string minified = glz::minify_json(buffer);
```

## Minified JSON Reading

If you wish require minified JSON or know your input will always be minified, then you can gain a little more performance by using the compile time option `.minified = true`.

```c++
auto ec = glz::read<glz::opts{.minified = true}>(obj, buffer);
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

> When writing BEVE, `flags` only use one bit per boolean (byte aligned).

## Logging JSON

Sometimes you just want to write out JSON structures on the fly as efficiently as possible. Glaze provides tuple-like structures that allow you to stack allocate structures to write out JSON with high speed. These structures are named `glz::obj` for objects and `glz::arr` for arrays.

Below is an example of building an object, which also contains an array, and writing it out.

```c++
auto obj = glz::obj{"pi", 3.14, "happy", true, "name", "Stephen", "arr", glz::arr{"Hello", "World", 2}};

std::string s{};
expect(not glz::write_json(obj, s));
expect(s == R"({"pi":3.14,"happy":true,"name":"Stephen","arr":["Hello","World",2]})");
```

> This approach is significantly faster than `glz::generic` for generic JSON. But, may not be suitable for all contexts.

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

See [Generic JSON](https://stephenberry.github.io/glaze/generic-json/) for `glz::generic`.

```c++
glz::generic json{};
std::string buffer = R"([5,"Hello World",{"pi":3.14}])";
glz::read_json(json, buffer);
assert(json[2]["pi"].get<double>() == 3.14);
```

## Raw Buffer Performance

Glaze is just about as fast writing to a `std::string` as it is writing to a raw char buffer. If you have sufficiently allocated space in your buffer you can write to the raw buffer, as shown below, but it is not recommended.

```c++
glz::read_json(obj, buffer);
const auto result = glz::write_json(obj, buffer.data());
if (!result) {
   buffer.resize(result.count);
}
```

### Writing to Fixed-Size Buffers

All write functions return `glz::error_ctx`, which provides both error information and the byte count:

```c++
std::array<char, 1024> buffer;
auto ec = glz::write_json(my_obj, buffer);
if (ec) {
   if (ec.ec == glz::error_code::buffer_overflow) {
      // Buffer was too small
      std::cerr << "Overflow after " << ec.count << " bytes\n";
   }
   return;
}
// Success: ec.count contains bytes written
std::string_view json(buffer.data(), ec.count);
```

The `error_ctx` type provides:
- `if (ec)` - true when there is an error (matches `std::error_code` semantics)
- `ec.count` - bytes processed (always populated, even on error)
- `ec.ec` - the error code
- `glz::format_error(ec, buffer)` - formatted error message

## Compile Time Options

The `glz::opts` struct defines the default compile time options for reading/writing.

Instead of calling `glz::read_json(...)`, you can call `glz::read<glz::opts{}>(...)` and customize the options.

For example: `glz::read<glz::opts{.error_on_unknown_keys = false}>(...)` will turn off erroring on unknown keys and simple skip the items.

`glz::opts` can also switch between formats:

- `glz::read<glz::opts{.format = glz::BEVE}>(...)` -> `glz::read_beve(...)`
- `glz::read<glz::opts{.format = glz::JSON}>(...)` -> `glz::read_json(...)`

> [!IMPORTANT]
>
> See [Options](https://stephenberry.github.io/glaze/options/) for a **comprehensive reference table** of all compile time options, including inheritable options that can be added to custom option structs.

### Common Compile Time Options

The `glz::opts` struct provides default options. Here are the most commonly used:

| Option | Default | Description |
|--------|---------|-------------|
| `format` | `JSON` | Format selector (`JSON`, `BEVE`, `CSV`, `TOML`) |
| `null_terminated` | `true` | Whether input buffer is null terminated |
| `error_on_unknown_keys` | `true` | Error on unknown JSON keys |
| `skip_null_members` | `true` | Skip null values when writing |
| `prettify` | `false` | Output formatted JSON |
| `minified` | `false` | Require minified input (faster parsing) |
| `error_on_missing_keys` | `false` | Require all keys to be present |
| `partial_read` | `false` | Exit after reading deepest object |

**Inheritable options** (not in `glz::opts` by default) can be added via custom structs:

```c++
struct my_opts : glz::opts {
   bool validate_skipped = true;        // Full validation on skipped values
   bool append_arrays = true;           // Append to arrays instead of replace
};

constexpr my_opts opts{};
auto ec = glz::read<opts>(obj, buffer);
```

> See [Options](https://stephenberry.github.io/glaze/options/) for the complete list with detailed descriptions, and [Wrappers](https://stephenberry.github.io/glaze/wrappers/) for per-field options.

## JSON Conformance

By default Glaze is strictly conformant with the latest JSON standard except in two cases with associated options:

- `validate_skipped`
  This option does full JSON validation for skipped values when parsing. This is not set by default because values are typically skipped when the user is unconcerned with them, and Glaze still validates for major issues. But, this makes skipping faster by not caring if the skipped values are exactly JSON conformant. For example, by default Glaze will ensure skipped numbers have all valid numerical characters, but it will not validate for issues like leading zeros in skipped numbers unless `validate_skipped` is on. Wherever Glaze parses a value to be used it is fully validated.
- `validate_trailing_whitespace`
  This option validates the trailing whitespace in a parsed document. Because Glaze parses C++ structs, there is typically no need to continue parsing after the object of interest has been read. Turn on this option if you want to ensure that the rest of the document has valid whitespace, otherwise Glaze will just ignore the content after the content of interest has been parsed.

> [!NOTE]
>
> By default, Glaze does not unicode escape control characters (e.g. `"\x1f"` to `"\u001f"`), as this poses a risk of embedding null characters and other invisible characters in strings. The compile time option `escape_control_characters` is available for those who desire to write out control characters as escaped unicode in strings.
>
> ```c++
> // Example options for enabling escape_control_characters
> struct options : glz::opts {
>    bool escape_control_characters = true;
> };
> ```

## Skip

It can be useful to acknowledge a keys existence in an object to prevent errors, and yet the value may not be needed or exist in C++. These cases are handled by registering a `glz::skip` type with the meta data.

<details><summary>See example:</summary>

```c++
struct S {
  int i{};
};

template <>
struct glz::meta<S> {
  static constexpr auto value = object("key_to_skip", skip{}, &S::i);
};
```

```c++
std::string buffer = R"({"key_to_skip": [1,2,3], "i": 7})";
S s{};
glz::read_json(s, buffer);
// The value [1,2,3] will be skipped
expect(s.i == 7); // only the value i will be read into
```

</details>

## Hide

Glaze is designed to help with building generic APIs. Sometimes a value needs to be exposed to the API, but it is not desirable to read in or write out the value in JSON. This is the use case for `glz::hide`.

`glz::hide` hides the value from JSON output while still allowing API (and JSON pointer) access.

<details><summary>See example:</summary>

```c++
struct hide_struct {
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
};

template <>
struct glz::meta<hide_struct> {
   using T = hide_struct;
   static constexpr auto value = object(&T::i,  //
                                        &T::d, //
                                        "hello", hide{&T::hello});
};
```

```c++
hide_struct s{};
auto b = glz::write_json(s);
expect(b == R"({"i":287,"d":3.14})"); // notice that "hello" is hidden from the output
```

</details>

## Quoted Numbers

You can parse quoted JSON numbers directly to types like `double`, `int`, etc. by utilizing the `glz::quoted` wrapper.

```c++
struct A {
   double x;
   std::vector<uint32_t> y;
};

template <>
struct glz::meta<A> {
   static constexpr auto value = object("x", glz::quoted_num<&A::x>, "y", glz::quoted_num<&A::y>;
};
```

```json
{
  "x": "3.14",
  "y": ["1", "2", "3"]
}
```

The quoted JSON numbers will be parsed directly into the `double` and `std::vector<uint32_t>`. The `glz::quoted` function works for nested objects and arrays as well.

## JSON Lines (NDJSON) Support

Glaze supports [JSON Lines](https://jsonlines.org) (or Newline Delimited JSON) for array-like types (e.g. `std::vector` and `std::tuple`).

```c++
std::vector<std::string> x = { "Hello", "World", "Ice", "Cream" };
std::string s = glz::write_ndjson(x).value_or("error");
auto ec = glz::read_ndjson(x, s);
```

# More Features

### [Data Recorder](https://stephenberry.github.io/glaze/recorder/)

### [Command Line Interface Menu](https://stephenberry.github.io/glaze/cli-menu/)

### [JMESPath](https://stephenberry.github.io/glaze/JMESPath/)

- Querying JSON

### [JSON Include System](https://stephenberry.github.io/glaze/json-include/)

### [JSON Pointer Syntax](https://stephenberry.github.io/glaze/json-pointer-syntax/)

### [JSON-RPC 2.0](https://stephenberry.github.io/glaze/rpc/json-rpc/)

### [JSON Schema](https://stephenberry.github.io/glaze/json-schema/)

### [Shared Library API](https://stephenberry.github.io/glaze/glaze-interfaces/)

### [Streaming I/O](https://stephenberry.github.io/glaze/streaming/)

### [Tagged Binary Messages](https://stephenberry.github.io/glaze/binary/)

### [Thread Pool](https://stephenberry.github.io/glaze/thread-pool/)

### [Time Trace Profiling](https://stephenberry.github.io/glaze/time-trace/)

- Output performance profiles to JSON and visualize using [Perfetto](https://ui.perfetto.dev)

### [Wrappers](https://stephenberry.github.io/glaze/wrappers/)

# Extensions

See the `ext` directory for extensions.

- [Eigen](https://gitlab.com/libeigen/eigen)
- [JSON-RPC 2.0](https://stephenberry.github.io/glaze/rpc/json-rpc/)
- [Command Line Interface Menu (cli_menu)](https://stephenberry.github.io/glaze/cli-menu/)

# License

Glaze is distributed under the MIT license with an exception for embedded forms:

> --- Optional exception to the license ---
>
> As an exception, if, as a result of your compiling your source code, portions of this Software are embedded into a machine-executable object form of such source code, you may redistribute such embedded portions in such object form without including the copyright and permission notices.
