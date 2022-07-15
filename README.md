# Glaze
An extremely fast, direct to memory, JSON and interface library for modern C++.

Glaze reads into C++ memory, simplifying interfaces and offering incredible performance.

Glaze requires C++20 due to heavy use of concepts and compile time programming.

Example:

```c++
struct thing
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
};

template <>
struct glaze::meta<thing>
{
  
};
```

Output/Input:

```json
{
  "i": 287,
  "d": 3.14,
  "hello": "Hello World",
  "arr": [1, 2, 3]
}
```

## Dependencies

- [fmt](https://github.com/fmtlib/fmt)
- [fast_float](https://github.com/fastfloat/fast_float)

## Header Only

Glaze can be used in a header only mode by using fmt's `FMT_HEADER_ONLY` macro.

## Glaze Interfaces

Glaze has been designed to work as a generic interface for libraries.

## Caveats

### Integers

- Integer types cannot begin with a positive `+` symbol, for efficiency.
- Integer types do not support exponential values (e.g. `1e10`). This is to enable vectorization of integer reading. Submit an issue if you desire a wrapper to enable this behavior, e.g. `exponential(&T::i)`.
