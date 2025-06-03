# Glaze

**Extremely fast, in memory, JSON and reflection library for modern C++**

One of the fastest JSON libraries in the world. Glaze reads and writes from object memory, simplifying interfaces and offering incredible performance.

## Feature Shortlist

- **Pure, compile time reflection** for structs
- **JSON RFC 8259 compliance** with UTF-8 validation
- **Standard C++ library support**
- **Header only**
- **Direct to memory serialization/deserialization**
- **Compile time maps** with constant time lookups and perfect hashing
- **No exceptions** (compiles with `-fno-exceptions`)
- **No runtime type information** necessary (compiles with `-fno-rtti`)

## Quick Example

```cpp
struct my_struct {
    int i = 287;
    double d = 3.14;
    std::string hello = "Hello World";
    std::array<uint64_t, 3> arr = {1, 2, 3};
};

// Write JSON
my_struct s{};
std::string buffer = glz::write_json(s).value_or("error");

// Read JSON
auto s2 = glz::read_json<my_struct>(buffer);
```