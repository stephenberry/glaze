# TOML (Tom's Obvious, Minimal Language)

Glaze ships with a fast TOML 1.0 reader and writer. The same compile-time reflection metadata you already use for JSON works for TOML, so you can reuse your `glz::meta` specializations without additional boilerplate.

## Getting Started

The header `glaze/toml.hpp` exposes the high-level helpers. The example below writes and reads a configuration struct:

```cpp
#include "glaze/toml.hpp"

struct retry_policy
{
   int attempts = 5;
   int backoff_ms = 250;
};

template <>
struct glz::meta<retry_policy>
{
   using T = retry_policy;
   static constexpr auto value = object(&T::attempts, &T::backoff_ms);
};

struct app_config
{
   std::string host = "127.0.0.1";
   int port = 8080;
   retry_policy retry{};
   std::vector<std::string> features{"metrics"};
};

template <>
struct glz::meta<app_config>
{
   using T = app_config;
   static constexpr auto value = object(&T::host, &T::port, &T::retry, &T::features);
};

app_config cfg{};
std::string toml{};
auto write_error = glz::write_toml(cfg, toml);
if (write_error) {
   const auto message = glz::format_error(write_error, toml);
   // handle the error message
}

app_config loaded{};
auto read_error = glz::read_toml(loaded, toml);
if (read_error) {
   const auto message = glz::format_error(read_error, toml);
   // handle the error message
}
```

`glz::write_toml` and `glz::read_toml` return an `error_ctx`. The object becomes truthy when an error occurred; pass it to `glz::format_error` to obtain a human-readable explanation.

## TOML Input Example

The `app_config` structure above accepts both inline tables and dotted keys. Either of the snippets below will populate the same object:

```toml
host = "0.0.0.0"
port = 9000
features = ["metrics", "debug"]

retry = { attempts = 6, backoff_ms = 500 }
```

```toml
host = "0.0.0.0"
port = 9000
features = ["metrics", "debug"]

retry.attempts = 6
retry.backoff_ms = 500
```

Glaze understands standard TOML number formats (binary, octal, hex), quoted and multiline strings, arrays, inline tables, and comments (`#`).

## Array of Tables

Glaze supports TOML's array-of-tables syntax (`[[array_name]]`) for serializing and deserializing `std::vector` of objects. This provides a clean, readable format for arrays of structured data.

### Basic Array of Tables

```cpp
struct product
{
   std::string name;
   int sku;
};

template <>
struct glz::meta<product>
{
   using T = product;
   static constexpr auto value = object(&T::name, &T::sku);
};

struct catalog
{
   std::string store_name;
   std::vector<product> products;
};

template <>
struct glz::meta<catalog>
{
   using T = catalog;
   static constexpr auto value = object(&T::store_name, &T::products);
};

catalog c{
   "Hardware Store",
   {{"Hammer", 738594937}, {"Nail", 284758393}}
};

std::string toml{};
glz::write_toml(c, toml);
```

Output:

```toml
store_name = "Hardware Store"
[[products]]
name = "Hammer"
sku = 738594937

[[products]]
name = "Nail"
sku = 284758393
```

### Nested Array of Tables

Glaze produces TOML-spec-compliant output for nested arrays using dotted paths (`[[parent.child]]`):

```cpp
struct variety
{
   std::string name;
};

struct fruit
{
   std::string name;
   std::vector<variety> varieties;
};

struct fruit_basket
{
   std::vector<fruit> fruits;
};

fruit_basket basket{
   {{"apple", {{"red delicious"}, {"granny smith"}}},
    {"banana", {{"cavendish"}}}}
};

std::string toml{};
glz::write_toml(basket, toml);
```

Output:

```toml
[[fruits]]
name = "apple"
[[fruits.varieties]]
name = "red delicious"

[[fruits.varieties]]
name = "granny smith"

[[fruits]]
name = "banana"
[[fruits.varieties]]
name = "cavendish"
```

### Reading Array of Tables

Glaze reads array-of-tables syntax correctly, including:
- Multiple `[[name]]` sections that append to the same array
- Empty table entries (`[[name]]` followed immediately by another `[[name]]`)
- Nested dotted paths like `[[parent.child]]`

```cpp
std::string input = R"(
[[products]]
name = "Hammer"
sku = 738594937

[[products]]

[[products]]
name = "Nail"
sku = 284758393
)";

catalog c{};
glz::read_toml(c, input);
// c.products.size() == 3 (second entry is empty/default)
```

### Write Ordering

Glaze writes TOML in spec-compliant order: scalar key-value pairs appear before tables and array-of-tables sections. This ensures the output is valid TOML that can be parsed by any compliant reader.

## Using the Generic API

The convenience wrappers call into the generic `glz::read`/`glz::write` pipeline. You can reuse the same options struct you already use for JSON while switching the format to TOML:

```cpp
std::string_view config_text = R"(
host = "0.0.0.0"
port = 9000
retry.attempts = 4
retry.backoff_ms = 200
extra.flag = true
)";

app_config cfg{};
auto ec = glz::read<glz::opts{.format = glz::TOML, .error_on_unknown_keys = false}>(cfg, config_text);
if (ec) {
   const auto message = glz::format_error(ec, config_text);
   // handle unknown field or parse problems
}
```

Setting `.error_on_unknown_keys = false` allows dotted keys that do not correspond to reflected members to be skipped gracefully. Any other option in `glz::opts` (for example `.skip_null_members` or `.error_on_missing_keys`) can be combined the same way.

The write side uses the same mechanism:

```cpp
std::string toml{};
auto write_ec = glz::write<glz::opts{.format = glz::TOML, .skip_null_members = false}>(cfg, toml);
if (write_ec) {
   const auto message = glz::format_error(write_ec, toml);
   // handle write problems
}
```

Both `glz::read` and `glz::write` return `error_ctx`, so remember to check the result in production code.

## File Helpers and Buffers

For convenience Glaze also provides file-oriented helpers:

```cpp
std::string buffer{};
glz::write_file_toml(cfg, "config.toml", buffer); // writes to disk when serialization succeeds

app_config loaded{};
glz::read_file_toml(loaded, "config.toml", buffer);
```

`glz::read_toml` works with `std::string`, `std::string_view`, or any contiguous character buffer.

## Datetime Support

Glaze fully supports [TOML v1.1.0 datetime types](https://toml.io/en/v1.1.0#local-date-time), which are first-class values in TOML (not quoted strings). This enables seamless serialization of `std::chrono` types with native TOML datetime format.

### TOML Datetime Types

TOML defines four datetime types, each mapping to specific C++ chrono types:

| TOML Type | C++ Type | Format Example |
|-----------|----------|----------------|
| Offset Date-Time | `std::chrono::system_clock::time_point` | `2024-06-15T10:30:45Z` |
| Local Date-Time | `std::chrono::system_clock::time_point` | `2024-06-15T10:30:45` |
| Local Date | `std::chrono::year_month_day` | `2024-06-15` |
| Local Time | `std::chrono::hh_mm_ss<Duration>` | `10:30:45.123` |

### Offset Date-Time (system_clock::time_point)

`std::chrono::system_clock::time_point` serializes as an unquoted TOML Offset Date-Time in UTC:

```cpp
#include "glaze/toml.hpp"
#include <chrono>

auto now = std::chrono::system_clock::now();
std::string toml = glz::write_toml(now).value();
// Output: 2024-12-13T15:30:45Z (unquoted)
```

The parser supports multiple RFC 3339 formats:

```cpp
std::chrono::system_clock::time_point tp;

// UTC with Z suffix
glz::read_toml(tp, "2024-12-13T15:30:45Z");

// Lowercase z is allowed
glz::read_toml(tp, "2024-12-13T15:30:45z");

// Space delimiter instead of T (per TOML spec)
glz::read_toml(tp, "2024-12-13 15:30:45Z");

// With timezone offset
glz::read_toml(tp, "2024-12-13T15:30:45+05:00");
glz::read_toml(tp, "2024-12-13T15:30:45-08:00");

// With fractional seconds
glz::read_toml(tp, "2024-12-13T15:30:45.123456Z");

// Without seconds (per TOML spec)
glz::read_toml(tp, "2024-12-13T15:30Z");

// Local Date-Time (no timezone - treated as UTC)
glz::read_toml(tp, "2024-12-13T15:30:45");
```

### Local Date (year_month_day)

`std::chrono::year_month_day` serializes as an unquoted TOML Local Date:

```cpp
using namespace std::chrono;

year_month_day date{year{2024}, month{6}, day{15}};
std::string toml = glz::write_toml(date).value();
// Output: 2024-06-15 (unquoted)

// Reading
year_month_day parsed;
glz::read_toml(parsed, "2024-12-25");
// parsed.year() == 2024, parsed.month() == December, parsed.day() == 25
```

### Local Time (hh_mm_ss)

`std::chrono::hh_mm_ss<Duration>` serializes as an unquoted TOML Local Time:

```cpp
using namespace std::chrono;

// Seconds precision
hh_mm_ss<seconds> time_sec{hours{10} + minutes{30} + seconds{45}};
std::string toml = glz::write_toml(time_sec).value();
// Output: 10:30:45

// Milliseconds precision
hh_mm_ss<milliseconds> time_ms{hours{10} + minutes{30} + seconds{45} + milliseconds{123}};
toml = glz::write_toml(time_ms).value();
// Output: 10:30:45.123
```

Reading supports fractional seconds and optional seconds:

```cpp
using namespace std::chrono;

hh_mm_ss<milliseconds> time{milliseconds{0}};

// Standard format
glz::read_toml(time, "23:59:59");

// With fractional seconds
glz::read_toml(time, "12:30:45.500");

// Without seconds (per TOML spec)
glz::read_toml(time, "14:30");
```

### Structs with Datetime Fields

Datetime types work seamlessly in structs:

```cpp
struct Event {
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::year_month_day date;
    std::chrono::hh_mm_ss<std::chrono::seconds> start_time;
};

Event event{
    "Meeting",
    std::chrono::system_clock::now(),
    std::chrono::year_month_day{std::chrono::year{2024}, std::chrono::month{6}, std::chrono::day{15}},
    std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{14} + std::chrono::minutes{30}}
};

auto toml = glz::write_toml(event).value();
```

Output:

```toml
name = "Meeting"
timestamp = 2024-06-15T14:30:00Z
date = 2024-06-15
start_time = 14:30:00
```

### Duration Types

`std::chrono::duration` types serialize as their numeric count value (not as TOML datetime):

```cpp
std::chrono::seconds sec{3600};
std::string toml = glz::write_toml(sec).value();  // "3600"

std::chrono::milliseconds ms{};
glz::read_toml(ms, "12345");  // ms.count() == 12345
```

This works with any duration type including custom periods:

```cpp
std::chrono::hours h{24};               // "24"
std::chrono::nanoseconds ns{123456789}; // "123456789"

// Floating-point rep
std::chrono::duration<double, std::milli> ms{123.456};  // "123.456"
```

### Steady Clock and High Resolution Clock

`std::chrono::steady_clock::time_point` and `std::chrono::high_resolution_clock::time_point` serialize as numeric counts, since their epochs are implementation-defined:

```cpp
auto start = std::chrono::steady_clock::now();
std::string toml = glz::write_toml(start).value();  // numeric count

std::chrono::steady_clock::time_point parsed;
glz::read_toml(parsed, toml);  // exact roundtrip
```

### Datetime Summary Table

| C++ Type | TOML Format | Example Output |
|----------|-------------|----------------|
| `system_clock::time_point` | Offset Date-Time | `2024-06-15T10:30:45Z` |
| `year_month_day` | Local Date | `2024-06-15` |
| `hh_mm_ss<seconds>` | Local Time | `10:30:45` |
| `hh_mm_ss<milliseconds>` | Local Time | `10:30:45.123` |
| `duration<Rep, Period>` | Numeric | `3600` |
| `steady_clock::time_point` | Numeric | `123456789012345` |
