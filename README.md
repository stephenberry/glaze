# Glaze
An extremely fast, in memory, JSON and interface library for modern C++.

Glaze reads into C++ memory, simplifying interfaces and offering incredible performance.

Glaze requires C++20, using concepts for cleaner code and more helpful errors.

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
      "i", &T::i, //
      "d", &T::d, //
      "hello", &T::hello, //
      "arr", &T::arr //
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

## Header Only

Glaze is designed to be used in a header only manner. The macro `FMT_HEADER_ONLY` is used for the [fmt](https://github.com/fmtlib/fmt) library.

## Dependencies

Dependencies are automatically included when running CMake. [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) is used for dependency management.

- [fmt](https://github.com/fmtlib/fmt)
- [fast_float](https://github.com/fastfloat/fast_float)
- [frozen](https://github.com/serge-sans-paille/frozen.git)
- [NanoRange](https://github.com/tcbrindle/NanoRange)

> NanoRange is directly included until C++20 ranges are supported across all major compilers.

## JSON Pointer Syntax

Glaze supports JSON pointer syntax access in a C++ context. This is extremely helpful for building generic APIs, allowing components of complex arguments to be accessed without needed know the encapsulating class.

```c++
my_struct s{};
auto& d = glz::get<double>(s, "/d");
// d is a reference to d in the structure s
```

## JSON With Comments (JSONC)

Comments are supported with the specification defined here: [JSONC](https://github.com/stephenberry/JSONC)

Comments can also be included in the `glaze::meta` description for your types. These comments can be written out to provide a description of your JSON interface. Calling `write_jsonc` as opposed to `write_json` will write out any comments included in the `meta` description.

```c++
struct thing {
  double x{5.0};
  int y{7};
};

template <>
struct glz::meta<thing> {
   static constexpr auto value = object(
      using T = thing;
      "x", &T::a, "x is a double"_c,  //
      "y", &T::b, "y is an int"_c   //
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

# Additional Features

- Tagged binary messaging for maximum performance
- Comma Separated Value files (CSV)
- A data recorder (logging) (`recorder.hpp`)
- A simple thread pool
- Studies based on JSON structures
- A JSON file include system
- Eigen C++ matrix library support

## Tagged Binary Messages

[TODO: expand]

## Data Recorder (Logging)

[TODO: expand]

## Glaze Interfaces

Glaze has been designed to work as a generic interface for shared libraries and more. This is achieved through JSON pointer syntax access to memory.

Glaze allows a single header API (`api.hpp`) to be used for every shared library interface, greatly simplifying shared library handling.

> A valid concern is binary compatibility between types. Glaze uses compile time hashing of types that can catch changes to classes or types that would cause binary incompatibility. These compile time hashes are checked when accessing across the interface and provide a safeguard, much like a `std::any_cast`, but working across code and compiler changes.

# JSON Caveats

### Integers

- Integer types cannot begin with a positive `+` symbol, for efficiency.

# License

Glaze is distributed under the MIT license.
