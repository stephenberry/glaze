# CBOR (Concise Binary Object Representation)

Glaze provides comprehensive support for [CBOR](https://cbor.io/) (RFC 8949), a binary data serialization format designed for small message size and extensibility. CBOR is an IETF standard that provides a self-describing binary format, making it ideal for interoperability with other languages and systems.

## Basic Usage

**Write CBOR**

```c++
#include "glaze/cbor/cbor.hpp"

my_struct s{};
std::string buffer{};
auto ec = glz::write_cbor(s, buffer);
if (!ec) {
   // Success: ec.count contains bytes written
}
```

**Read CBOR**

```c++
my_struct s{};
auto ec = glz::read_cbor(s, buffer);
if (!ec) {
   // Success
}
```

> [!NOTE]
> Reading CBOR is safe for invalid input and does not require null-terminated buffers.

## Exceptions Interface

If you prefer exceptions over error codes:

```c++
#include "glaze/cbor/cbor_exceptions.hpp"

try {
   my_struct s{};
   auto buffer = glz::ex::write_cbor(s);
   glz::ex::read_cbor(s, buffer);
}
catch (const std::runtime_error& e) {
   // Handle error
}
```

## Standards Compliance

Glaze CBOR implements the following standards:

| Standard | Description |
|----------|-------------|
| [RFC 8949](https://www.rfc-editor.org/rfc/rfc8949.html) | CBOR core specification |
| [RFC 8746](https://www.rfc-editor.org/rfc/rfc8746.html) | Typed arrays and multi-dimensional arrays |
| [IANA CBOR Tags](https://www.iana.org/assignments/cbor-tags/cbor-tags.xhtml) | Registered semantic tags |

## Typed Arrays (RFC 8746)

Glaze automatically uses RFC 8746 typed arrays for contiguous numeric containers, enabling bulk memory operations for maximum performance.

```c++
std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
std::string buffer{};
glz::write_cbor(values, buffer);  // Uses typed array with bulk memcpy
```

### Supported Typed Array Tags

| Type | Little-Endian Tag | Big-Endian Tag |
|------|-------------------|----------------|
| `uint8_t` | 64 | 64 |
| `uint16_t` | 69 | 65 |
| `uint32_t` | 70 | 66 |
| `uint64_t` | 71 | 67 |
| `int8_t` | 72 | 72 |
| `int16_t` | 77 | 73 |
| `int32_t` | 78 | 74 |
| `int64_t` | 79 | 75 |
| `float` | 85 | 81 |
| `double` | 86 | 82 |

Glaze automatically selects the correct tag based on the native endianness of the system. When reading, Glaze performs automatic byte-swapping if the data was written on a system with different endianness.

## Multi-Dimensional Arrays (RFC 8746)

Glaze supports RFC 8746 multi-dimensional arrays using semantic tags:

| Tag | Description |
|-----|-------------|
| 40 | Row-major multi-dimensional array |
| 1040 | Column-major multi-dimensional array |

### Eigen Matrix Support

Glaze provides native CBOR support for Eigen matrices and vectors:

```c++
#include "glaze/ext/eigen.hpp"

// Fixed-size matrix
Eigen::Matrix3d m;
m << 1, 2, 3,
     4, 5, 6,
     7, 8, 9;

std::string buffer{};
glz::write_cbor(m, buffer);

Eigen::Matrix3d result;
glz::read_cbor(result, buffer);
```

```c++
// Dynamic matrix
Eigen::MatrixXd m(3, 4);
// ... fill matrix ...

std::string buffer{};
glz::write_cbor(m, buffer);

Eigen::MatrixXd result;
glz::read_cbor(result, buffer);  // Automatically resizes
```

The CBOR format for matrices is:
```
tag(40 or 1040) [
   [rows, cols],      // dimensions array
   typed_array(data)  // data as RFC 8746 typed array
]
```

- Tag 40 is used for row-major matrices
- Tag 1040 is used for column-major matrices (Eigen default)

This format is self-describing and interoperable with other CBOR implementations.

## Complex Numbers

Glaze supports complex numbers using IANA-registered CBOR tags:

| Tag | Description | Format |
|-----|-------------|--------|
| 43000 | Single complex number | `[real, imag]` |
| 43001 | Complex array | Interleaved typed array `[r0, i0, r1, i1, ...]` |

See: [IANA CBOR Tags Registry](https://www.iana.org/assignments/cbor-tags/cbor-tags.xhtml)

### Single Complex Numbers

```c++
std::complex<double> c{1.0, 2.0};
std::string buffer{};
glz::write_cbor(c, buffer);

std::complex<double> result;
glz::read_cbor(result, buffer);
// result.real() == 1.0, result.imag() == 2.0
```

The CBOR format is:
```
tag(43000) [real, imag]
```

### Complex Arrays

Complex arrays use tag 43001 with an interleaved typed array for bulk memory operations:

```c++
std::vector<std::complex<double>> values = {
   {1.0, 0.5},
   {2.0, 1.0},
   {3.0, 1.5}
};

std::string buffer{};
glz::write_cbor(values, buffer);  // Uses bulk memcpy

std::vector<std::complex<double>> result;
glz::read_cbor(result, buffer);
```

The CBOR format is:
```
tag(43001) tag(typed_array_tag) bstr([r0, i0, r1, i1, ...])
```

This format leverages the fact that `std::complex<T>` stores real and imaginary parts contiguously in memory, enabling efficient bulk serialization.

### Complex Eigen Matrices

Complex Eigen matrices are fully supported:

```c++
Eigen::MatrixXcd m(2, 2);
m << std::complex<double>(1, 1), std::complex<double>(2, 2),
     std::complex<double>(3, 3), std::complex<double>(4, 4);

std::string buffer{};
glz::write_cbor(m, buffer);

Eigen::MatrixXcd result;
glz::read_cbor(result, buffer);
```

## Bitsets

`std::bitset` is serialized as a CBOR byte string:

```c++
std::bitset<32> bits = 0b10101010'11110000'00001111'01010101;
std::string buffer{};
glz::write_cbor(bits, buffer);

std::bitset<32> result;
glz::read_cbor(result, buffer);
```

## Floating-Point Preferred Serialization

Following RFC 8949 preferred serialization, Glaze automatically uses the smallest floating-point representation that exactly represents the value:

- Half-precision (16-bit) when possible
- Single-precision (32-bit) when half is insufficient
- Double-precision (64-bit) as fallback

This reduces message size while preserving exact values.
