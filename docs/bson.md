# BSON

Glaze ships with first-class BSON support. Any type that already reflects for JSON — aggregate-initializable structs, `glz::meta`-annotated classes, STL containers, `std::optional`, `std::variant` (see [Variants](#variants) for requirements), custom types — serializes to and from BSON without additional metadata.

## Specification Compliance

Glaze implements the [BSON 1.1 specification](https://bsonspec.org/spec.html). Element types supported:

| Code | Type | C++ mapping |
|------|------|-------------|
| 0x01 | double | `float`, `double`, `long double` |
| 0x02 | UTF-8 string | `std::string`, `std::string_view`, `const char*` |
| 0x03 | embedded document | reflected struct, `glz::meta`-annotated class, `std::map<std::string, T>` |
| 0x04 | array | `std::vector`, `std::array`, `std::list`, any `writable_array_t` |
| 0x05 | binary | `glz::bson::binary<Bytes>`, `glz::uuid` (subtype 0x04) |
| 0x07 | ObjectId | `glz::bson::object_id` |
| 0x08 | boolean | `bool` |
| 0x09 | UTC datetime | `glz::bson::datetime`, `std::chrono::system_clock::time_point` |
| 0x0A | null | `std::optional`, `std::nullptr_t`, empty pointers |
| 0x0B | regex | `glz::bson::regex` |
| 0x0D | JavaScript | `glz::bson::javascript` |
| 0x10 | int32 | 8-, 16-, and 32-bit integers (signed or unsigned <32-bit) |
| 0x11 | timestamp | `glz::bson::timestamp` |
| 0x12 | int64 | 64-bit integers, `uint32_t`, `uint64_t` (with range check) |
| 0x13 | decimal128 | `glz::bson::decimal128` (opaque 16-byte wrapper) |
| 0x7F / 0xFF | MinKey / MaxKey | `glz::bson::max_key` / `glz::bson::min_key` |

Deprecated element types (0x06 undefined, 0x0C DBPointer, 0x0E symbol, 0x0F code-with-scope) are skipped on read and cannot be written.

## Quick Start

```cpp
#include "glaze/bson.hpp"

struct point
{
   double x{};
   double y{};
};

point p{.x = 4.2, .y = 1.3};

// Write into a std::string (or std::vector<char> / std::vector<std::byte>)
auto encoded = glz::write_bson(p);
if (!encoded) {
   // glz::format_error(encoded.error()) for a message
}

// Read the buffer back
point decoded{};
auto ec = glz::read_bson(decoded, std::string_view{encoded.value()});
if (ec) {
   const auto message = glz::format_error(ec, encoded.value());
}
```

`glz::write_bson` and `glz::read_bson` mirror the JSON helpers:

- They work with any reflected type, STL container, map with string keys, or custom specialization.
- Overloads exist for output buffers (`std::string`, `std::vector<char>`, `std::vector<std::byte>`, etc.).
- Write functions return an `error_ctx` whose `count` field records bytes written; read functions return the same type.
- Success is the falsy case: `if (!ec) { /* ok */ }`.

BSON's top-level value is always a document. Primitives (`int`, `std::string`, `double`, …) are rejected at the call site with a `no matching function for call to 'write_bson'` compile error. Wrap them in a struct or map if you need to ship a single value.

## Integer Encoding

BSON has two integer element types. Glaze picks between them by the target's range, not the runtime value, so the encoding is deterministic from the schema alone:

| C++ type | BSON element |
|---|---|
| `int8_t`, `int16_t`, `int32_t`, `uint8_t`, `uint16_t` | int32 (0x10) |
| `int64_t`, `uint32_t`, `long`, `long long` | int64 (0x12) |
| `uint64_t` | int64 (0x12) with range check — values above `INT64_MAX` fail with `error_code::invalid_length` on write |

On read, both int32 and int64 wire tags are accepted for any integer target and range-checked at runtime; mismatched sizes return `error_code::parse_number_failure`. Integer wire tags are also accepted into `float`/`double` targets as a widening conversion.

## `glz::bson::object_id`

Opaque 12-byte ObjectId. Glaze treats it as a fixed-size blob — MongoDB assigns meaning to the bytes (timestamp, random, counter) but the library does not generate them.

```cpp
struct document
{
   glz::bson::object_id _id{};
   std::string name{};
};
```

## `glz::bson::datetime` and `std::chrono::system_clock::time_point`

BSON's UTC datetime is a signed int64 holding milliseconds since the Unix epoch. Two C++ types map to it:

- `glz::bson::datetime` — direct wrapper over `int64_t ms_since_epoch` for code that wants the raw value.
- `std::chrono::system_clock::time_point` — auto-mapped on read and write. Precisions finer than milliseconds are truncated via `duration_cast<milliseconds>`.

```cpp
struct event
{
   std::string name{};
   std::chrono::system_clock::time_point when{};
};

event e{"login", std::chrono::system_clock::now()};
auto encoded = glz::write_bson(e);
```

## `glz::bson::timestamp`

MongoDB-internal timestamp used for replication and sharding. Distinct from `datetime`: the wire format is a uint64 with the low 32 bits holding an increment counter and the high 32 bits seconds since epoch.

```cpp
glz::bson::timestamp ts{.increment = 7, .seconds = 1700000000};
```

## `glz::bson::regex`, `glz::bson::javascript`

```cpp
glz::bson::regex re{.pattern = "^glaze", .options = "i"};    // case-insensitive
glz::bson::javascript js{.code = "function(x) { return x; }"};
```

Regex options follow MongoDB convention (alphabetical, single characters): `i` case-insensitive, `l` locale-dependent, `m` multiline, `s` dotall, `u` Unicode, `x` verbose.

## `glz::bson::decimal128`

Opaque 16-byte IEEE 754-2008 decimal128. This is **not** the same encoding as `std::float128_t` — that type is binary128 (`§3.6`), whereas BSON decimal128 is `§3.5` (decimal significand, base-10 exponent). The wrapper carries the raw bytes for round-trip interop; arithmetic and string conversion are out of scope.

## `glz::bson::binary`

Arbitrary binary data plus a subtype byte.

```cpp
glz::bson::binary<std::vector<std::byte>> payload{
   .data = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}},
   .subtype = glz::bson::binary_subtype::generic,
};
```

Subtype constants (from `glz::bson::binary_subtype`):

| Constant | Value | Meaning |
|---|---|---|
| `generic` | 0x00 | Default |
| `function` | 0x01 | Function data |
| `uuid_old` | 0x03 | Deprecated UUID encoding |
| `uuid` | 0x04 | RFC 9562 UUID (canonical byte order) |
| `md5` | 0x05 | MD5 hash |
| `encrypted` | 0x06 | Encrypted value |
| `column` | 0x07 | Compressed BSON column |
| `sensitive` | 0x08 | Sensitive data |
| `vector` | 0x09 | Dense numeric vector |
| `user_defined_min` | 0x80 | First reserved user-defined code |

The container type is your choice — `std::vector<std::byte>`, `std::vector<uint8_t>`, `std::string`, etc.

## `glz::uuid`

`glz::uuid` is a 16-byte UUID (RFC 9562) defined in `glaze/util/uuid.hpp`. Today a custom serializer exists only for BSON: on write it emits binary subtype 0x04 (canonical RFC 9562 byte order); on read it accepts 0x04 and rejects 0x03 (`uuid_old`) with `error_code::syntax_error`. 0x03 is refused deliberately — its byte layout is driver-dependent (Java, C#, and Python historically laid the same UUID out in incompatible orders), so decoding without knowing the origin driver would silently scramble the bytes. To consume legacy 0x03 data, re-encode to 0x04 at the source, or read the field as `glz::bson::binary<...>` and reorder the bytes yourself.

In other Glaze formats (JSON, BEVE, CBOR, JSONB, MsgPack, …) `glz::uuid` has no dedicated specialization — it falls through to aggregate reflection over its 16-byte array and serializes as `{"bytes":[...]}`. Round-trips work, but the wire shape is not the canonical hyphenated string. If you need the textual form in those formats, use `glz::to_string(uuid)` / `glz::parse_uuid` around a `std::string` field.

```cpp
#include "glaze/util/uuid.hpp"

glz::uuid id{};
glz::parse_uuid("550e8400-e29b-41d4-a716-446655440000", id);

struct record { glz::uuid id{}; std::string name{}; };
record r{id, "example"};
auto encoded = glz::write_bson(r);
```

The canonical textual representation is the hyphenated 8-4-4-4-12 hex form; parsing accepts upper- and lower-case hex digits and `glz::to_string(uuid)` always emits lower-case.

## Arrays

BSON encodes arrays as documents whose keys are the decimal strings `"0"`, `"1"`, `"2"`, … in order. Glaze handles the key generation internally; from user code, an `std::vector<int>` round-trips like any other container.

```cpp
std::vector<int32_t> xs{1, 2, 3};
auto encoded = glz::write_bson(xs);
// total length | 04 '0' 00 <i32> | 04 '1' 00 <i32> | 04 '2' 00 <i32> | 00
```

## Optionals and Missing Keys

Empty `std::optional<T>` fields follow Glaze's standard `skip_null_members` behavior:

- **Default** (`skip_null_members = true`): the key is omitted entirely from the document.
- **Explicit null** (`skip_null_members = false`): the key is written with element type 0x0A (null).

On read, a missing key leaves the target's optional at whatever it already was; an explicit `null` on the wire resets it.

```cpp
struct profile { std::string name{}; std::optional<int> age{}; };

profile p{"alice", std::nullopt};
auto encoded = glz::write_bson(p);    // omits "age" entirely
```

## Unknown Keys

By default Glaze errors on unknown keys (`error_code::unknown_key`). To tolerate extra fields — e.g., when reading documents produced by a newer writer — pass a permissive opts:

```cpp
constexpr auto permissive = glz::opts{.error_on_unknown_keys = false};
auto ec = glz::read_bson<permissive>(value, buffer);
```

## Variants

`std::variant<...>` is auto-deduced on read from the element's BSON type byte — no wrapper or tag field is added to the wire format. The writer serializes the active alternative directly; the reader maps the incoming tag to the first alternative of the matching BSON category.

Each BSON category (null, bool, int, float, string, document, array, binary, uuid, object_id, datetime, timestamp, regex, javascript, decimal128, min_key, max_key) may contain **at most one** variant alternative. This is enforced at compile time with a `static_assert`.

```cpp
struct message
{
   std::variant<int32_t, std::string> payload{};
};

struct maybe
{
   std::variant<std::monostate, int32_t> count{};   // monostate → null on the wire
};

struct record
{
   std::variant<double, std::vector<int32_t>> value{};
};
```

Notes:

- `std::monostate` (and `std::nullptr_t`, `std::nullopt_t`) count as the **null** category — the writer emits BSON `null` (0x0A).
- `std::optional<T>` in a variant only matches the null category, not `T`'s category. For "maybe an int or a string," prefer `std::variant<std::monostate, int, std::string>`.
- `int` is widened to a `float` alternative when the variant has only float (not vice versa).
- `glz::uuid` and `glz::bson::binary<Bytes>` may coexist in one variant; the reader prefers `binary<Bytes>` because its wire subtype check is permissive.
- Rejected at compile time: two alternatives in the same category, e.g., `std::variant<int32_t, int64_t>` or `std::variant<struct_a, struct_b>`. The writer and reader cannot tell them apart from the wire tag alone. A tagged-variant convention (as in JSON) is a planned follow-up.

## Maps

Maps with string-like keys round-trip as BSON documents. Non-string key types are rejected at compile time — BSON keys are cstrings on the wire, and silent lossy key conversion is undesirable.

```cpp
std::map<std::string, int32_t> m{{"a", 1}, {"b", 2}};
auto encoded = glz::write_bson(m);
```

## File Helpers

```cpp
std::vector<std::byte> buffer{};
glz::write_file_bson(value, "payload.bson", buffer);

decltype(value) restored{};
glz::read_file_bson(restored, "payload.bson", buffer);
```

## Interop Example

The canonical `{"hello": "world"}` document from the spec:

```
16 00 00 00                       // total length = 22
02 68 65 6C 6C 6F 00              // string element, key "hello"
06 00 00 00 77 6F 72 6C 64 00     // string length = 6, "world\0"
00                                // document terminator
```

Glaze produces exactly this sequence:

```cpp
struct hello_t { std::string hello{}; };
hello_t h{"world"};
auto encoded = glz::write_bson(h);
// *encoded equals the 22 bytes above, byte-for-byte.
```

## Unsupported / Future Work

- **MongoDB Extended JSON** (canonical / relaxed modes) — follow-up; this documentation covers the binary wire format only.
- **Decimal128 arithmetic and string conversion** — `glz::bson::decimal128` is currently an opaque byte buffer. A future version may add lossless text parsing.
- **Deprecated element types** — 0x06 undefined, 0x0C DBPointer, 0x0E symbol, 0x0F code-with-scope — are skipped on read to preserve forward compatibility but cannot be written.
