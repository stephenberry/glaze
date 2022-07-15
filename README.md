# Glaze
An extremely fast, direct to memory, JSON and interface library for modern C++.

Glaze reads into C++ memory, simplifying interfaces and offering incredible performance.

Glaze requires C++20 due to heavy use of concepts and compile time programming.

```c++
struct thing
{
  int i = 287;
  double d = 3.4039e-2;
  std::string hello = "Hello World";
};

template <>
struct glaze::meta<thing>
{
  
};
```



```json
{
  "i": 287,
  "d": 
}
```



## Dependencies

- [fmt](https://github.com/fmtlib/fmt)
- [fast_float](https://github.com/fastfloat/fast_float)

## Header Only

Glaze can be used in a header only mode by using fmt's `FMT_HEADER_ONLY` macro.
