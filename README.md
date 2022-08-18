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
struct glaze::meta<my_struct> {
   using T = my_struct;
   static constexpr auto value = glaze::object(
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
glaze::write_json(s, buffer);
// buffer is now: {"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]}
```

**Read JSON**

```c++
std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
my_struct s{};
glaze::read_json(s, buffer);
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

# Additional Features

- Binary messaging for maximum performance
- Comma Separated Value files (CSV)
- A data recorder (`recorder.hpp`)
- A simple thread pool
- Studies based on JSON structures
- A JSON file include system
- Eigen C++ matrix library support

## Glaze Interfaces

Glaze has been designed to work as a generic interface for shared libraries and more. This is achieved through JSON pointer syntax access to memory.

Glaze allows a single header API (`api.hpp`) to be used for every shared library interface, greatly simplifying shared library handling.

# JSON Caveats

### Integers

- Integer types cannot begin with a positive `+` symbol, for efficiency.

# License

Glaze is distributed under the MIT license.
