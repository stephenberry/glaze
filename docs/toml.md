# TOML (Tom's Obvious, Minimal Language)

Glaze ships with a fast TOML 1.1 reader and writer. The same compile-time reflection metadata you already use for JSON works for TOML, so you can reuse your `glz::meta` specializations without additional boilerplate.

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
- Sub-tables of an element, written `[name.sub]`, which fill in the most recent `[[name]]`

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

A `[name.sub]` header names a sub-table of the element the most recent `[[name]]` opened, per TOML v1.0.0:

```toml
[[products]]
name = "Hammer"

[products.origin]     # belongs to "Hammer"
country = "US"

[[products]]
name = "Nail"

[products.origin]     # belongs to "Nail"
country = "DE"
```

### Write Ordering

Glaze writes TOML in spec-compliant order: scalar key-value pairs appear before tables and array-of-tables sections. This ensures the output is valid TOML that can be parsed by any compliant reader.

### Inline Tables

By default, `std::vector` of objects uses array-of-tables (`[[name]]`) syntax. There are two ways to use inline table syntax (`[{...}, {...}]`) instead:

#### Global Option: `glz::toml_opts`

Use `glz::toml_opts` with the standard `write<>` interface to write all arrays of objects using inline syntax:

```cpp
struct product
{
   std::string name;
   int sku;
};

struct catalog
{
   std::string store_name;
   std::vector<product> products;
};

catalog c{"My Store", {{"Widget", 100}, {"Gadget", 200}}};
std::string toml{};
glz::write<glz::toml_opts{true}>(c, toml);  // inline_arrays = true
```

Output:

```toml
store_name = "My Store"
products = [{name = "Widget", sku = 100}, {name = "Gadget", sku = 200}]
```

The `glz::toml_opts` struct inherits from `glz::opts`, following the recommended pattern for format-specific options. For repeated use, create a named constant:

```cpp
constexpr glz::toml_opts inline_toml{true};
glz::write<inline_toml>(c, toml);
```

#### Per-Field Option: `glz::inline_table` Wrapper

For fine-grained control, use the `glz::inline_table` wrapper in your `glz::meta` definition to specify which fields use inline syntax:

```cpp
template <>
struct glz::meta<catalog>
{
   using T = catalog;
   // Use inline_table wrapper for this specific field
   static constexpr auto value = object(&T::store_name, "products", glz::inline_table<&T::products>);
};

catalog c{"My Store", {{"Widget", 100}, {"Gadget", 200}}};
std::string toml{};
glz::write_toml(c, toml);  // Regular write_toml, but products uses inline syntax
```

This is useful when you want a more compact representation or when the array contains simple objects with few fields.

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

## Required Keys

`.error_on_missing_keys = true` requires every non-nullable reflected field to be assigned by the document, and reports `glz::error_code::missing_key` naming the first one that was not:

```cpp
struct config
{
   std::string host{};
   uint16_t port{};
   std::string topic{};
};

std::string_view toml = R"(
host = "localhost"
port = 1883
)";

config cfg{};
auto ec = glz::read<glz::opts{.format = glz::TOML, .error_on_missing_keys = true}>(cfg, toml);
// ec.ec == glz::error_code::missing_key, ec.custom_error_message == "topic"
```

Which fields count as required follows the same rules as every other format, including the `glz::meta<T>::requires_key` customization point. See [Field Validation](field-validation.md).

A key counts as assigned however TOML lets the document reach it: a key-value line, a dotted key, a `[table]` header, or an inline table. Because a struct can be filled from more than one place in a document, the check runs once the whole document has been read rather than where a table body ends:

```toml
[server]
host = "h"
port = 1

[logging]      # an unrelated table between the two halves of [server]
level = "info"

[server.tls]   # still fills in server.tls
enabled = true
```

Each element of an array of tables is checked on its own, so a single incomplete `[[items]]` entry is an error even when the others are complete.

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

## Variant and Generic Type Support

Glaze supports `std::variant` and the generic JSON types (`glz::generic`, `glz::generic_i64`, `glz::generic_u64`) for TOML serialization and deserialization. This enables schema-less parsing where the structure of the data is not known at compile time.

### std::variant Support

Any `std::variant` can be serialized to TOML. When writing, the currently held alternative is serialized directly:

```cpp
#include "glaze/toml.hpp"

std::variant<int, double, std::string, bool> value = 42;
std::string toml = glz::write_toml(value).value();
// Output: 42

value = "hello";
toml = glz::write_toml(value).value();
// Output: "hello"

value = true;
toml = glz::write_toml(value).value();
// Output: true
```

When reading, Glaze automatically detects the TOML value type and selects the appropriate variant alternative:

```cpp
std::variant<int64_t, double, std::string, bool> value;

glz::read_toml(value, "42");        // value holds int64_t{42}
glz::read_toml(value, "3.14");      // value holds double{3.14}
glz::read_toml(value, "\"text\"");  // value holds std::string{"text"}
glz::read_toml(value, "true");      // value holds bool{true}
```

An inline table tells Glaze the value is an object, but not which object alternative it is. Glaze parses the first object alternative and, if that does not fit, rewinds and tries each of the others, taking the first that reads cleanly:

```cpp
struct point { int x{}; int y{}; };
struct pair { std::string a{}; int b{}; };

struct config
{
   std::variant<point, pair> v{};
};

config cfg{};
glz::read_toml(cfg, R"(v = { a = "s", b = 2 })");  // cfg.v holds pair
```

When no alternative fits, the error reported is the one from the first object alternative.

With `error_on_unknown_keys = true` a `missing_key` failure is not retried past: that is your own `error_on_missing_keys` strictness being enforced, so an incomplete `point` is reported as incomplete rather than answered with a `pair`. With unknown keys skipped the two cases are indistinguishable — a wrong alternative also fails with `missing_key` — so the remaining alternatives are still tried, and an alternative that is merely incomplete still reports its own error because nothing else fits.

Retrying is bounded by a per-read speculation budget, so an ambiguous nest of variants cannot cost exponential time. Once the budget is spent, the remaining alternatives are not tried and the failure stands.

Because any alternative may be tried, every object alternative's reader is instantiated. A variant holding an alternative that Glaze cannot read as TOML at all will not compile, even if no document ever selects it — the same as for JSON.

### Generic JSON Types

The generic JSON types provide a convenient way to parse arbitrary TOML data:

| Type | Integer Storage | Use Case |
|------|-----------------|----------|
| `glz::generic` | `double` | General purpose, preserves floating-point precision |
| `glz::generic_i64` | `int64_t` | When integers must be preserved exactly |
| `glz::generic_u64` | `std::uint64_t` for positive, `int64_t` for negative | When large positive integers are needed |

#### Using glz::generic

```cpp
#include "glaze/toml.hpp"

glz::generic data;
std::string input = R"(
name = "config"
port = 8080
rate = 0.5
enabled = true
tags = ["web", "api"]
)";

auto ec = glz::read_toml(data, input);

// Access the parsed data
auto& obj = std::get<glz::obj>(data);
auto& name = std::get<std::string>(obj["name"]);   // "config"
auto& port = std::get<double>(obj["port"]);        // 8080.0
auto& rate = std::get<double>(obj["rate"]);        // 0.5
auto& enabled = std::get<bool>(obj["enabled"]);    // true
auto& tags = std::get<glz::arr>(obj["tags"]);      // ["web", "api"]
```

#### Using glz::generic_i64

Use `glz::generic_i64` when you need exact integer preservation:

```cpp
glz::generic_i64 data;
glz::read_toml(data, "value = 9007199254740993");  // Larger than JS safe integer

auto& obj = std::get<glz::obj_i64>(data);
auto& value = std::get<int64_t>(obj["value"]);  // Exact: 9007199254740993
```

#### Using glz::generic_u64

Use `glz::generic_u64` when working with large unsigned integers:

```cpp
glz::generic_u64 data;
glz::read_toml(data, "big = 18446744073709551615");  // Max std::uint64_t

auto& obj = std::get<glz::obj_u64>(data);
auto& big = std::get<std::uint64_t>(obj["big"]);  // 18446744073709551615
```

Negative integers are stored as `int64_t` even in u64 mode:

```cpp
glz::generic_u64 data;
glz::read_toml(data, "negative = -42");

auto& obj = std::get<glz::obj_u64>(data);
auto& negative = std::get<int64_t>(obj["negative"]);  // -42 as int64_t
```

### Type Detection Rules

When reading into a variant or generic type, Glaze uses these rules to determine the TOML value type:

| TOML Syntax | Detected Type |
|-------------|---------------|
| `"..."` or `'...'` | String |
| `true` or `false` | Boolean |
| `[...]` | Array |
| Numbers with `.`, `e`, `E`, `inf`, `nan` | Float |
| Other numbers | Integer |

For integers in `glz::generic_u64` mode:
- Numbers starting with `-` are stored as `int64_t`
- Positive numbers are stored as `std::uint64_t`

### Writing Generic Types

Generic types can be written back to TOML:

```cpp
glz::generic_i64 data;
auto& obj = data.emplace<glz::obj_i64>();
obj["name"] = "example";
obj["count"] = int64_t{42};
obj["enabled"] = true;

std::string toml = glz::write_toml(data).value();
```

Output:

```toml
name = "example"
count = 42
enabled = true
```

### Nested Arrays

Nested arrays are supported for reading:

```cpp
glz::generic data;
glz::read_toml(data, "[[1, 2], [3, 4], [5, 6]]");

auto& arr = std::get<glz::arr>(data);
auto& inner = std::get<glz::arr>(arr[0]);
auto& val = std::get<double>(inner[0]);  // 1.0
```

### Map Types (std::map, std::unordered_map)

TOML documents can also be read directly into map types like `std::map<std::string, T>` or `std::unordered_map<std::string, T>`:

```cpp
std::map<std::string, int64_t> config;
std::string toml = R"(
port = 8080
timeout = 30
retries = 3
)";

auto ec = glz::read_toml(config, toml);
// config["port"] == 8080
// config["timeout"] == 30
// config["retries"] == 3
```

This also works with table sections:

```cpp
std::map<std::string, std::map<std::string, std::string>> config;
std::string toml = R"(
[database]
host = "localhost"
user = "admin"

[cache]
driver = "redis"
)";

auto ec = glz::read_toml(config, toml);
// config["database"]["host"] == "localhost"
// config["cache"]["driver"] == "redis"
```

### Limitations

- **Null values**: TOML has no native null type. When writing `std::nullptr_t` or a variant holding null, an empty string `""` is written.
- **Type coercion**: The parser does not coerce types. If the variant has no matching alternative for the detected type, an error is returned.
- **Array of tables in maps**: The `[[array_of_tables]]` syntax is not fully supported when reading into map types. Use struct-based types for this pattern.
