# CBOR (Concise Binary Object Representation)

Glaze provides comprehensive support for [CBOR](https://cbor.io/) (RFC 8949), a binary data serialization format designed for small message size and extensibility. CBOR is an IETF standard that provides a self-describing binary format, making it ideal for interoperability with other languages and systems.

## Basic Usage

**Write CBOR**

```c++
#include "glaze/cbor.hpp"

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

## Date and Time (std::chrono)

Glaze serializes `std::chrono` types to CBOR using the standard semantic tags defined by RFC 8949. See [chrono documentation](./chrono.md) for the shared concepts; this section covers the CBOR-specific wire format.

### Wire format

| Type | Encoding | Source |
|------|----------|--------|
| `std::chrono::duration<Rep, Period>` | Bare integer count in the duration's native units | — |
| `std::chrono::system_clock::time_point` | Tag 0 + text string (RFC 3339 date/time) | RFC 8949 §3.4.1 |
| `glz::epoch_seconds` (Period ≥ 1s) | Tag 1 + integer seconds since epoch | RFC 8949 §3.4.2 |
| `glz::epoch_millis` / `epoch_micros` / `epoch_nanos` | Tag 1 + float64 seconds since epoch | RFC 8949 §3.4.2 |
| `std::chrono::steady_clock::time_point` | Bare integer count (no tag) | — |

RFC 8949 §3.4.2 restricts tag 1 content to integers (major types 0/1) or floating-point numbers (major type 7, additional info 25/26/27). Nested tags — including tag 4 decimal fractions — are explicitly forbidden as tag 1 content, so sub-second precision is carried via float64.

### Example

```c++
#include "glaze/cbor.hpp"
#include <chrono>

struct Event {
    std::chrono::system_clock::time_point timestamp;  // tag 0 + RFC 3339 string
    glz::epoch_millis logged_at;                      // tag 1 + float64 seconds
    std::chrono::milliseconds duration;               // bare integer
};

Event e{};
e.timestamp  = std::chrono::system_clock::now();
e.logged_at  = std::chrono::system_clock::now();
e.duration   = std::chrono::milliseconds{2500};

std::string buffer;
glz::write_cbor(e, buffer);

Event parsed;
glz::read_cbor(parsed, buffer);
```

The canonical RFC 8949 §3.4.2 example `2013-03-21T20:04:00Z` encoded as `glz::epoch_seconds` produces exactly `0xC1 0x1A 0x51 0x4B 0x67 0xB0`.

### Precision

Float64 has ~15–17 significant decimal digits:

- `epoch_millis` — lossless round-trip for any realistic modern epoch.
- `epoch_micros` — lossless through approximately year 2255.
- `epoch_nanos` — for modern timestamps, round-trip error is on the order of ~100 ns (the mantissa is exceeded once nanoseconds-since-1970 surpasses 2^53). Applications needing lossless nanosecond wire precision can use a bare integer type or build an RFC 9581 tag 1001 wrapper.

`system_clock::time_point` (tag 0 string) carries precision matching the time point's `Duration` template parameter — seconds, milliseconds, microseconds, or nanoseconds fractional digits are written as appropriate.

### Decoder behavior

**`system_clock::time_point` accepts:**
- Tag 0 + text string (canonical, what Glaze writes)
- Tag 1 + integer or float seconds (converted from epoch)
- Bare text string without a tag (lenient — for producers that omit tag 0)

Bare numbers and other shapes are rejected, since a unitless number has no defined meaning for a calendar time point.

**`epoch_time<Duration>` accepts:**
- Tag 1 + integer or float16/32/64 seconds (canonical)
- Bare integer or float without a tag (lenient — same semantics)

Nested tags inside tag 1 are rejected per RFC 8949 §3.4.2.

Note: `system_clock::time_point` and `epoch_time<Duration>` do **not** cross-read — they pick different wire tags on write, so the type you read into must match the wire form you expect. Pick the type on both sides to match your protocol.

### Safety

- **NaN / Infinity** on tag-1 float payloads are rejected as meaningless timestamps.
- **Integer and float seconds** are bounds-checked against the range `system_clock::duration` can represent, so an adversarial wire value cannot overflow `std::chrono::duration_cast`.
- **Out-of-range years** on write — RFC 3339 requires a 4-digit year; times outside `[0000, 9999]` return an error instead of emitting corrupt digits.
- **Leap seconds** (`:60`) are rejected on read, matching the JSON backend. `std::chrono::system_clock` uses Unix time and does not model leap seconds.
