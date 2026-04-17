# JSONB (SQLite Binary JSON)

Glaze provides read/write support for the [SQLite JSONB](https://sqlite.org/jsonb.html) binary JSON format. JSONB is the native on-disk representation that SQLite uses for its `jsonb_*` functions — producing it directly from C++ avoids a JSON → JSONB conversion pass inside SQLite.

## Basic Usage

**Write JSONB**

```c++
#include "glaze/jsonb.hpp"

my_struct s{};
std::string buffer{};
auto ec = glz::write_jsonb(s, buffer);
if (!ec) {
   // buffer now contains a valid SQLite JSONB blob, ready to INSERT.
}
```

**Read JSONB**

```c++
my_struct s{};
auto ec = glz::read_jsonb(s, buffer);
if (!ec) {
   // Success
}
```

**Convert JSONB → JSON**

```c++
auto json = glz::jsonb_to_json(buffer);
if (json) {
   // json.value() is a std::string containing JSON text
}
```

## Element Types

JSONB encodes every value with a 1–9 byte header (type nibble + payload-size nibble) followed by the payload bytes. The 13 element types (codes 0–12) are:

| Code | Name | Glaze writes from | Glaze reads into |
|-----:|------|-------------------|------------------|
| 0 | NULL | `std::nullptr_t`, empty optional | any `null`-compatible type |
| 1 | TRUE | `bool{true}` | `bool` |
| 2 | FALSE | `bool{false}` | `bool` |
| 3 | INT | `std::integral` types | `std::integral`, `std::floating_point` |
| 4 | INT5 | (not emitted) | `std::integral`, `std::floating_point` |
| 5 | FLOAT | finite `float`, `double` | `std::floating_point`, `std::integral` |
| 6 | FLOAT5 | `NaN`, `Infinity`, `-Infinity` | `std::floating_point` |
| 7 | TEXT | (not emitted) | `std::string`, `std::string_view` |
| 8 | TEXTJ | (not emitted) | `std::string` (with JSON escape decoding) |
| 9 | TEXT5 | (not emitted) | `std::string` (with JSON5 escape decoding) |
| 10 | TEXTRAW | `std::string`, string literals, reflected field names | `std::string`, `std::string_view` |
| 11 | ARRAY | `std::vector`, tuples, ranges, `std::array`, reflected `glaze_array` | same |
| 12 | OBJECT | `std::map`, `std::unordered_map`, `std::pair`, reflected structs | same |

Codes 13–15 are reserved by the spec; Glaze returns `error_code::syntax_error` on read if any appear.

### Forward-compatibility: NULL / TRUE / FALSE with non-zero payload

Per the SQLite JSONB spec, legacy readers **must** interpret element types 0 (NULL), 1 (TRUE), and 2 (FALSE) as their nominal value even when the payload size is non-zero, so that future spec extensions that store information in those payload bytes don't break older readers. Glaze skips any payload bytes and keeps the nominal value — it does not reject such elements as malformed.

## Container Encoding Strategy

Arrays and objects store a **payload size in bytes**, not a child count. Since that size isn't known until after the children are written, Glaze's writer reserves a 9-byte header slot (the `u64_follows` form), writes the children, then patches the header with the actual size.

The spec permits non-minimal headers, so blobs Glaze writes are spec-compliant but may be a few bytes larger than the minimal encoding for small containers.

> **SQLite note:** `json_valid(blob, 4)` accepts Glaze's output (well-formed JSONB with non-minimal headers); `json_valid(blob, 8)` requires canonical minimal-header form and rejects it. All other SQLite JSON/JSONB operations — `json()`, `jsonb()`, `json_extract()`, `json_type()`, `json_array_length()`, path queries, and `WHERE` filters — work correctly against Glaze blobs. See `tests/jsonb_sqlite_test/` for validated cases.

## Floating-Point Special Values

Standard JSON has no representation for `NaN`, `Infinity`, or `-Infinity`. JSONB inherits JSON5's ability to represent them via the FLOAT5 element type, and Glaze writes them as FLOAT5 automatically. `jsonb_to_json` renders them as JSON `null` when emitting strict JSON.

## Variants

`std::variant` is serialized as a two-element array `[index, value]`, matching Glaze's CBOR convention.

## SQLite Interop

Store Glaze's output directly as a JSONB column:

```c++
glz::write_jsonb(my_value, buf);
sqlite3_bind_blob(stmt, 1, buf.data(), int(buf.size()), SQLITE_TRANSIENT);
// SQLite column type: JSONB
```

Read back with `SELECT json(col)` or pass the blob through Glaze's `jsonb_to_json` for a JSON string.

## High-Level API

```c++
// Write
error_ctx         glz::write_jsonb<T>(T&& value, Buffer& buffer);
expected<string>  glz::write_jsonb<T>(T&& value);   // returns a new string
error_code        glz::write_file_jsonb<T>(T&& value, sv filename, Buffer& scratch);

// Read
error_ctx         glz::read_jsonb<T>(T& value, const Buffer& buffer);
expected<T>       glz::read_jsonb<T>(const Buffer& buffer);
error_ctx         glz::read_file_jsonb<T>(T& value, sv filename, Buffer& scratch);

// Convert
error_ctx         glz::jsonb_to_json(const Buffer& jsonb, std::string& out);
expected<string>  glz::jsonb_to_json(const Buffer& jsonb);
```

Use `glz::set_jsonb<Opts>()` to compose `JSONB` with other `glz::opts` fields.

## Supported Options

The following `glz::opts` fields influence JSONB read/write:

| Option | Default | Effect |
|---|---|---|
| `error_on_unknown_keys` | `true` | Error vs silently skip unknown object keys on read |
| `error_on_missing_keys` | `false` | Error if a required (non-nullable) reflected field is absent on read |
| `skip_null_members` | `true` | Omit null `optional`/`unique_ptr`/`shared_ptr` fields when writing objects |
| `error_on_const_read` | *(custom)* | Error vs silently skip when the target is const |
| `skip_default_members` | *(custom)* | Omit fields equal to their zero/empty value when writing. Must be added via a custom opts struct |
| `max_string_length` | *(custom)* | Cap string allocation size on read |
| `write_function_pointers` | *(custom)* | Emit vs skip member function pointers in reflected objects |

Options marked *(custom)* are not part of the base `glz::opts` struct — define a custom opts struct inheriting from `glz::opts` and add the field.

Options that apply only to text formats (`prettify`, `minified`, `comments`, `null_terminated`, `indentation_char`, `escape_control_characters`) have no effect on JSONB.
