# Binary Format (BEVE)

Glaze provides a binary format to send and receive messages like JSON, but with significantly improved performance and message size savings.

The binary specification is known as [BEVE](https://github.com/beve-org/beve).

**Write BEVE**

```c++
my_struct s{};
std::vector<std::byte> buffer{};
auto ec = glz::write_beve(s, buffer);
if (!ec) {
   // Success: ec.count contains bytes written
}
```

**Read BEVE**

```c++
my_struct s{};
auto ec = glz::read_beve(s, buffer);
if (!ec) {
   // Success
}
```

> [!NOTE]
>
> Reading binary is safe for invalid input and does not require null terminated buffers.

**Recursion depth**

BEVE spends as little as two bytes per nesting level, so a small hostile buffer could otherwise drive the reader deep enough to overflow the stack. The readers, the value skipper, and `glz::beve_to_json` all cap nesting at `max_recursive_depth_limit` (256 wire levels) and return `error_code::exceeded_max_recursive_depth` beyond it. Each object and each array counts as a level, so a struct holding a vector of itself reaches the cap at 128 struct levels.

## Calculate BEVE Size

The `glz::beve_size` function calculates the exact number of bytes needed to serialize a value to BEVE format, without actually performing the serialization. This is useful when you need to pre-allocate a buffer of the exact size, such as when writing to shared memory for inter-process communication.

**Basic Usage**

```c++
my_struct s{};
std::size_t size = glz::beve_size(s);
// size contains the exact number of bytes needed to serialize s
```

**Shared Memory Example**

```c++
#include "glaze/beve.hpp"

struct SensorData {
   std::uint64_t timestamp;
   double temperature;
   std::vector<double> readings;
};

// Calculate size before allocation
SensorData data{12345, 98.6, {1.0, 2.0, 3.0}};
std::size_t size = glz::beve_size(data);

// Allocate shared memory with exact size
void* shm = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);

// Serialize directly to pre-allocated buffer
std::span<char> buffer{static_cast<char*>(shm), size};
glz::write_beve(data, buffer);
```

**Untagged Size Calculation**

For untagged serialization (structs written as arrays without keys), use `glz::beve_size_untagged`:

```c++
my_struct s{};
std::size_t tagged_size = glz::beve_size(s);           // with keys
std::size_t untagged_size = glz::beve_size_untagged(s); // without keys (smaller)
```

**Compressed Integer Size Helper**

BEVE uses variable-length encoding for integers in size/count fields. The `glz::compressed_int_size` helper calculates how many bytes a given integer will occupy:

```c++
glz::compressed_int_size(0);           // 1 byte (values < 64)
glz::compressed_int_size(63);          // 1 byte
glz::compressed_int_size(64);          // 2 bytes (values < 16384)
glz::compressed_int_size(16383);       // 2 bytes
glz::compressed_int_size(16384);       // 4 bytes (values < 1073741824)
glz::compressed_int_size(1073741824);  // 8 bytes

// Compile-time version
static_assert(glz::compressed_int_size<100>() == 2);
```

**Supported Types**

`glz::beve_size` supports all types that can be serialized to BEVE:

- Primitives: `bool`, integers, floating-point, `char`, enums
- Strings: `std::string`, `std::string_view`, `const char*`
- Containers: `vector`, `array`, `map`, `set`, `list`, `deque`, `span`
- Nullable: `optional`, `shared_ptr`, `unique_ptr`, raw pointers
- Compound: `variant`, `tuple`, `pair`, `complex`
- Glaze types: structs with `glz::meta`, `glz::obj`, `glz::raw_json`
- Nested structures of arbitrary depth

## Peek BEVE Header

The `glz::beve_peek_header` function examines a BEVE buffer's header to extract type and element count information **without performing full deserialization**. This is the counterpart to `glz::beve_size` - while `beve_size` calculates the size before writing, `beve_peek_header` inspects the size/count after receiving data.

**Use Cases**

- **Pre-allocation**: Know how many elements to `reserve()` before deserializing a vector
- **Buffer validation**: Check structure and bounds before committing to full parse
- **Routing decisions**: Determine the type of incoming data before processing
- **Memory budgeting**: Verify element counts against limits before allocation

**Basic Usage**

```c++
#include "glaze/beve.hpp"

std::vector<int> data{1, 2, 3, 4, 5};
auto buffer = glz::write_beve(data).value();

// Peek at header without deserializing
auto result = glz::beve_peek_header(buffer);
if (result) {
   // result->type == glz::tag::typed_array (4)
   // result->count == 5 (number of elements)
   // result->header_size == 2 (bytes consumed by header)
}
```

**Return Type: `glz::beve_header`**

```c++
struct beve_header {
   uint8_t tag{};        // Raw tag byte
   uint8_t type{};       // Base type (see below)
   uint8_t ext_type{};   // For extensions: subtype (variant, complex, etc.)
   std::size_t count{};       // Element count, string length, variant index, etc.
   std::size_t header_size{}; // Bytes consumed by tag + count encoding
};
```

**Base Types (`type` field)**

| Value | Type | `count` meaning |
|-------|------|-----------------|
| 0 | null/boolean | 0 for null, 1 for boolean |
| 1 | number | 1 |
| 2 | string | String length in bytes |
| 3 | object | Number of key-value pairs |
| 4 | typed_array | Number of elements |
| 5 | generic_array | Number of elements |
| 6 | extensions | See extension subtypes below |

**Extension Subtypes (`ext_type` field when `type == 6`)**

| ext_type | Name | `count` meaning | `header_size` |
|----------|------|-----------------|---------------|
| `glz::extension::delimiter` (0) | Delimiter | 0 | 1 |
| `glz::extension::variant` (1) | Variant (Version 1 only) | Variant index | 1 + compressed_int size |
| `glz::extension::complex` (3) | Complex number | 2 (real + imag) | 2 |
| `glz::extension::complex` (3) | Complex array | Element count | 2 + compressed_int size |

For complex types, distinguish single complex vs array by checking if `count == 2` and `header_size == 2` (single) or `header_size > 2` (array).

**Pre-allocation Example**

```c++
// Receive BEVE data from network/file
std::string buffer = receive_beve_data();

// Peek to get element count
auto header = glz::beve_peek_header(buffer);
if (!header) {
   handle_error(header.error());
   return;
}

// Pre-allocate based on peeked count
std::vector<double> values;
values.reserve(header->count);

// Now deserialize - vector won't need to reallocate
glz::read_beve(values, buffer);
```

**Validation Example**

```c++
auto header = glz::beve_peek_header(buffer);
if (!header) {
   return unexpected(header.error());
}

// Reject oversized arrays
constexpr std::size_t max_elements = 10000;
if (header->type == glz::tag::typed_array && header->count > max_elements) {
   return unexpected(error_code::invalid_length);
}

// Proceed with deserialization
return glz::read_beve<std::vector<int>>(buffer);
```

**Variant Example**

A variant is written as an ordinary self-describing value (see [Variants](#variants) below), so peeking a
variant returns the header of whatever the active alternative is, not a variant-specific header:

```c++
using MyVariant = std::variant<int, std::string, double>;
std::string buffer = receive_data();

auto header = glz::beve_peek_header(buffer);
if (header && header->type == glz::tag::string) {
   std::cout << "Contains a string of " << header->count << " bytes\n";
}

MyVariant value;
glz::read_beve(value, buffer);
```

Buffers written by Glaze prior to BEVE Version 2 instead begin with the `glz::extension::variant`
header, where `count` is the positional variant index. Such buffers still read back normally:

```c++
auto header = glz::beve_peek_header(buffer);
if (header && header->type == glz::tag::extensions
           && header->ext_type == glz::extension::variant) {
   // Version 1 encoding: header->count is the variant index
}
```

**Raw Pointer Overload**

For C-style buffers:

```c++
const void* data = /* ... */;
std::size_t size = /* ... */;

auto result = glz::beve_peek_header(data, size);
```

**Peek Header at Offset**

Use `glz::beve_peek_header_at` to peek at headers at arbitrary byte offsets without slicing or copying the buffer:

```c++
std::string buffer = /* BEVE data */;
std::size_t offset = /* position to inspect */;

auto header = glz::beve_peek_header_at(buffer, offset);
if (header) {
   std::cout << "Type: " << (int)header->type
             << ", Count: " << header->count
             << ", Header size: " << header->header_size << "\n";
}
```

This is useful for:

- **Buffers with custom prefixes**: Skip past application headers to the BEVE payload
- **Memory-mapped files**: Seek to specific positions without copying data
- **Resuming partial reads**: Continue parsing from where you left off
- **Concatenated/delimited streams**: Inspect each object before deserializing
- **Embedded BEVE**: Parse BEVE data within larger binary structures
- **Validation**: Check element counts at specific offsets against limits

```c++
// Example: Buffer with 8-byte custom header followed by BEVE data
std::string buffer = /* custom_header (8 bytes) + BEVE payload */;

auto header = glz::beve_peek_header_at(buffer, 8);  // Skip custom header
if (header) {
   // header->type, header->count, header->header_size
}
```

```c++
// Example: Multiple values in one buffer
std::int32_t val1 = 42;
std::string val2 = "hello";

auto buffer1 = glz::write_beve(val1).value();
auto buffer2 = glz::write_beve(val2).value();
std::string combined = buffer1 + buffer2;

// Peek at first value (offset 0)
auto header1 = glz::beve_peek_header_at(combined, 0);
// header1->type == glz::tag::number

// Peek at second value
auto header2 = glz::beve_peek_header_at(combined, buffer1.size());
// header2->type == glz::tag::string
// header2->count == 5 ("hello" has 5 characters)
```

Raw pointer overload with offset:

```c++
auto result = glz::beve_peek_header_at(data, size, offset);
```

**Error Handling**

Returns `glz::expected<beve_header, error_ctx>`. Possible errors:

- `error_code::unexpected_end` - Buffer too small to contain header
- `error_code::syntax_error` - Invalid tag byte

## Aligned Typed Arrays (Zero-Copy)

BEVE typed arrays store contiguous numerical data in a compact layout. With aligned typed arrays, the data payload is padded so it begins at a memory offset that satisfies the element type's alignment requirement. This enables zero-copy access — a decoder can hand back a `std::span<const T>` pointing directly into the message buffer with no copies or allocations.

### Writing Aligned Arrays

Enable alignment with the `aligned_arrays` option:

```c++
struct aligned_opts : glz::opts
{
   bool aligned_arrays = true;
};

std::vector<double> data = {1.0, 2.0, 3.0, 4.0};
std::string buffer;
constexpr aligned_opts opts{glz::opts{.format = glz::BEVE}};
glz::write<opts>(data, buffer);
```

Single-byte types (`int8_t`, `uint8_t`) use the standard typed array format since alignment provides no benefit.

### Zero-Copy Reading with `std::span<const T>`

Read directly into a `std::span<const T>` to get a zero-copy view into the buffer:

```c++
std::span<const double> span;
glz::read<glz::opts{.format = glz::BEVE}>(span, buffer);

// span.data() points directly into buffer — no copy was made
// span is properly aligned for the element type
```

The span specialization requires an aligned typed array in the buffer. Reading a standard (non-aligned) typed array into `std::span<const T>` produces an error, since alignment cannot be guaranteed.

> [!IMPORTANT]
> The buffer must outlive the span. The span points into the buffer's memory.

### Zero-Copy Struct Members

Structs can contain `std::span<const T>` members alongside owning types. Write with owning containers, read back with spans:

```c++
// Write-side struct (owns data)
struct sensor_packet {
   std::string name;
   std::vector<double> readings;
};

// Read-side struct (zero-copy view)
struct sensor_view {
   std::string name;
   std::span<const double> readings;  // points into buffer
};

// Write
sensor_packet packet{"sensor_1", {1.0, 2.0, 3.0}};
std::string buffer;
glz::write<opts>(packet, buffer);

// Read — readings is zero-copy, name is copied
sensor_view view;
glz::read<glz::opts{.format = glz::BEVE}>(view, buffer);
// view.readings.data() points into buffer
```

> [!NOTE]
> Zero-copy span reading requires little-endian systems (BEVE wire format is little-endian). On big-endian systems, use `std::vector` which copies and byte-swaps as needed.

### Wire Format

The aligned typed array layout is:

```
ALIGNED_HEADER | NUMERIC_HEADER | SIZE | PADDING_LENGTH | PADDING | DATA
```

- **ALIGNED_HEADER** — 1 byte (`0x5C`), identifies this as an aligned typed array
- **NUMERIC_HEADER** — 1 byte, encodes the element type (same as a standard numeric typed array header)
- **SIZE** — compressed unsigned integer, element count
- **PADDING_LENGTH** — 1 byte, number of padding bytes that follow
- **PADDING** — 0 to `alignment - 1` bytes so DATA starts at an aligned offset
- **DATA** — raw element data, identical to a standard typed array payload

The overhead is 2 extra bytes (numeric header + padding length) plus at most `alignment - 1` bytes of padding. For large arrays this is negligible.

## std::chrono Durations

`std::chrono::duration` values serialize as their bare numeric count (the `rep`), so a duration is written byte-for-byte like the equivalent BEVE number. This makes a duration interchangeable with a numeric field in a shared schema:

```c++
std::chrono::milliseconds ms{12345};
std::string buffer{};
glz::write_beve(ms, buffer);

std::chrono::milliseconds decoded{};
glz::read_beve(decoded, buffer); // decoded.count() == 12345
```

Because the encoding is the numeric `rep`, durations also work as numeric BEVE map and pair keys (e.g. `std::map<std::chrono::seconds, V>`), producing the same wire form as the equivalent rep-keyed container. See [std::chrono Support](./chrono.md#durations) for the full cross-format behavior.

## Untagged Binary

By default Glaze will handle structs as tagged objects, meaning that keys will be written/read. However, structs can be written/read without tags by using the option `structs_as_arrays` or the functions `glz::write_beve_untagged` and `glz::read_beve_untagged`.

## Variants

BEVE Version 2 writes a `std::variant` as an ordinary, self-describing value. Which shape it takes is fixed by the variant's tagging representation, declared once in `glz::meta` and applied to every alternative — see [Choosing a Tagging Representation](./variant-handling.md#choosing-a-tagging-representation) for the full rules:

| `glz::meta` declares | Representation | BEVE shape |
|---|---|---|
| nothing | none | the alternative's own value, bare |
| `tag` | internal | object with the discriminator merged in as the first member |
| `tag` and `content` | adjacent | object of exactly two members, `{tag: id, content: value}` |

Nothing variant-specific appears on the wire, so `glz::beve_to_json` of a variant produces the same JSON as `glz::write_json` of the same value, for every representation.

Internal tagging requires every alternative to be object-shaped, since there is nowhere to merge a key into `[1, 2]` or `42`; declaring `tag` alone for a variant with a scalar, array, map, or pair alternative is a compile error directing you to `content`. Adjacent tagging carries the discriminator for every alternative type, so alternatives that share a wire shape — `std::variant<std::vector<double>, std::deque<double>>` — round-trip under it.

Two BEVE-specific limits, both compile errors rather than silent divergence from JSON:

- **Internal tagging cannot be written for a custom-serialized alternative.** BEVE objects are length-prefixed, and the member count of a body produced by someone else's `to<BEVE, T>` is not knowable in advance. (JSON merges into such a body because JSON objects are not counted.) Declare `content`, which nests the custom value rather than merging into it.
- **`glz::beve_size` cannot size a custom-serialized type**, for the same reason: only your `to<BEVE, T>` knows its length. Specialize `glz::calculate_size<glz::BEVE, T>` beside it, or size the value by writing it.

On read, the alternative is recovered from the discriminator when one is present, otherwise from the object's key set, otherwise from the value's own type header. Adjacent tagging skips all of that: the discriminator names the alternative outright, so nothing is deduced. For the other representations, several consequences are worth knowing:

- Alternatives are matched on their **exact** type header first, so `std::variant<int32_t, int64_t>` round-trips without narrowing. If no alternative matches exactly, a second pass allows numeric conversions.
- Alternatives that are genuinely indistinguishable on the wire collapse to the first one. This covers two structs with identical field sets, two empty structs, and containers with the same encoding (`std::vector<int>` and `std::deque<int>` are byte-identical, so the value survives but the alternative index may not). To tell them apart, declare a discriminator: `tag` and `ids` for two structs, or `tag`, `content` and `ids` when any of the colliding alternatives is not an object (two containers, for instance, which internal tagging cannot represent at all).
- A map or pair alternative accepts any key set, so it competes with the struct alternatives for the same wire shape. It wins whenever the object carries a key that no struct alternative declares, and loses otherwise. A map whose keys are all field names of some struct alternative is therefore indistinguishable from that struct, and resolves to the struct — an empty map being the degenerate case. The same rule applies to alternatives that encode as objects but expose no key set at all: a nested `std::variant`, an `std::optional<Struct>`, or a type with a custom reader.

- Where BEVE and the JSON reader disagree, BEVE is the stricter of the two: it prefers a map alternative on a foreign key where JSON prefers the struct, it treats an explicit discriminator as authoritative rather than cross-checking it against the keys seen so far, and it retries other candidates when a deduction fails. The two agree on the collapse rules above. One consequence to know: adding a map or pair alternative to a variant changes how struct data carrying an unknown key decodes, and `error_on_unknown_keys = false` no longer makes such data read as the struct, because the foreign key is taken as evidence the map wrote it.

- Retrying is limited to failures that say the shape does not fit. A `missing_key` failure is your own `error_on_missing_keys` strictness being enforced, so it is reported rather than treated as the wrong alternative. A tagged empty-struct alternative is likewise never reached by a retry: it reads by skipping the whole object, so it would match anything and silently discard the payload.
- Deduction from keys is a guess, not a guarantee: if the deduced alternative fails to parse, the remaining alternatives are tried before the read is reported as an error. An explicit discriminator is authoritative and is never second-guessed this way.

### Variants Under `structs_as_arrays`

Positional writes (`structs_as_arrays`, `glz::write_beve_untagged`) have no keys, so there is nothing for a discriminator to merge into and no key set to deduce from. Adjacent tagging projects here to a two element array holding the id and the value:

```c++
using shape = std::variant<circle, rectangle>;
// meta: tag = "type", content = "value", ids = {"circle", "rectangle"}

glz::write_beve_untagged(shape{rectangle{2.0, 3.0}});
// ["rectangle", [2.0, 3.0]]
```

This is an ordinary BEVE generic array, not the deprecated type tag extension. Since the id sits beside the value rather than inside it, it works for every alternative type including scalars and arrays. The discriminator is authoritative here: an id that names no alternative is an error rather than a fallback to structural guessing, because positional data carries nothing to guess from.

Internal tagging has nothing to project in this mode — its entire mechanism is a key, and there are none — so declaring `tag` without `content` and then writing or reading positionally is a compile error. Add `content` to get the array form above.

A variant with no discriminator is still written bare. Alternatives with distinguishable positional shapes round-trip by trying each in turn, but alternatives that share a shape (same arity and same element types) collapse to the first one — the value survives, the alternative index does not. Declare `tag`, `content` and `ids` if that distinction matters.

### Version 1 Compatibility

Version 1 encoded a variant as a type-tag extension (header byte `0x0E`) followed by a compressed positional index. The reader dispatches on the leading byte and accepts both encodings, so **reading needs no option** and existing Version 1 buffers continue to work.

Writing is Version 2 only. Version 2 output is not decodable as a variant by a Glaze release that predates Version 2, so if you have a peer pinned to such a release, either upgrade it or pin this side to a matching older Glaze until both ends move.

## BEVE to JSON Conversion

`glaze/binary/beve_to_json.hpp` provides `glz::beve_to_json`, which directly converts a buffer of BEVE data to a buffer of JSON data.

An empty buffer holds no value, which is not a document, and fails with `error_code::unexpected_end` rather than converting to empty output. On success the returned `error_ctx::count` is the number of bytes written, which is how a fixed-size output buffer learns how much of it holds JSON.

A buffer holding several values converts to one JSON document per line. See [Delimiter Format](#delimiter-format).

### String Escaping

A BEVE string can hold any byte, including control characters (0x00–1F). JSON cannot. `\n`, `\t`, `\b`, `\f` and `\r` are escaped normally, but the rest have no short escape, and by default the conversion fails rather than write something that will not parse:

```c++
// a BEVE string holding a 0x01 byte
std::string json{};
auto ec = glz::beve_to_json(beve, json);
// ec.ec == glz::error_code::invalid_control_character
```

To keep those bytes, turn on `escape_control_characters`. They are written as `\uXXXX` and the document round-trips:

```c++
struct escaping : glz::opts {
   bool escape_control_characters = true;
};

std::string json{};
auto ec = glz::beve_to_json<escaping{}>(beve, json);   // {"k":"a\u0001b"}
```

Which to pick:

- **Your data legitimately contains control bytes.** Use `escape_control_characters`. A `std::string` holding a `\0` round-trips through BEVE, and escaping is the only way JSON can carry it.
- **The bytes are arbitrary,** as in an Erlang binary. Use `binary_as_base64` instead, so no raw bytes end up in a JSON string at all.
- **You want to know when they appear.** Keep the default and check for `invalid_control_character`.

One thing to weigh when escaping: `\u0000` is valid JSON that every parser accepts, so a NUL in your data reaches whatever reads the JSON back. Embedded nulls can truncate C strings and split log lines.

The same applies to `glz::cbor_to_json`, `glz::bson_to_json`, and `glz::eetf_to_json`. `glz::jsonb_to_json` always escapes, because a JSONB blob stores a JSON document that may have contained `\uXXXX` to begin with.

**Other notes**

- `raw_string` and `unquoted` are ignored by these converters. Both would produce output that is not JSON.
- UTF-8 is not checked, except by `glz::jsonb_to_json`. Reading the JSON checks it, and `validate_utf8` is on by default there.

### Function Pointers

Objects that expose function pointers (both member and non-member) through `glz::meta` are skipped by the BEVE writer by default. This mirrors JSON/TOML behaviour and avoids emitting unusable callable placeholders in binary payloads.

If you want the key present, use `write_function_pointers = true`.

## Custom Map Keys

BEVE can serialize map-like containers whose key types expose a value through Glaze metadata. This allows “strong ID” wrappers to keep a user-defined type while the binary payload stores the underlying numeric representation.

```c++
struct ModuleID {
   std::uint64_t value{};
   auto operator<=>(const ModuleID&) const = default;
};

template <>
struct glz::meta<ModuleID> {
   static constexpr auto value = &ModuleID::value;
};

std::map<ModuleID, std::string> modules{{ModuleID{42}, "life"}, {ModuleID{9001}, "power"}};

std::string beve{};
glz::write_beve(modules, beve);
```

Glaze inspects the metadata, reuses the underlying `std::uint64_t`, and emits the numeric BEVE map header so the payload decodes as a regular number key. The same behaviour works for `std::unordered_map` and concatenated ranges such as `std::vector<std::pair<ModuleID, T>>`.

If you prefer to keep a custom conversion in your metadata, `glz::cast` works as well:

```c++
template <>
struct glz::meta<ModuleID> {
   static constexpr auto value = glz::cast<&ModuleID::value, std::uint64_t>;
};
```

## Partial Objects

It is sometimes desirable to write out only a portion of an object. This is permitted via an array of JSON pointers, which indicate which parts of the object should be written out.

```c++
static constexpr auto partial = glz::json_ptrs("/i",
                                               "/d",
                                               "/sub/x",
                                               "/sub/y");
std::vector<std::byte> out;
glz::write_beve<partial>(s, out);
```

## Delimited BEVE (Multiple Objects in One Buffer)

Similar to [NDJSON](https://github.com/ndjson/ndjson-spec) for JSON, BEVE supports storing multiple objects in a single buffer using a delimiter. The BEVE specification defines a **Data Delimiter** extension (type 6, subtype 0) specifically for this purpose.

This is useful for:

- Streaming multiple messages over a connection
- Appending records to a buffer without re-encoding existing data
- Log files with multiple serialized entries
- Message queues with batched records

### Quick Reference

**Writing Functions**

| Function | Description |
|----------|-------------|
| `write_beve_delimiter(buffer)` | Writes a single delimiter byte (0x06) |
| `write_beve_append(value, buffer)` | Appends a BEVE value to existing buffer. Returns `error_ctx` with `count` field for bytes written. |
| `write_beve_append_with_delimiter(value, buffer)` | Writes delimiter + value. Returns `error_ctx` with bytes written including delimiter. |
| `write_beve_delimited(container, buffer)` | Writes all container elements with delimiters between them. Returns `error_ctx`. |

**Reading Functions**

| Function | Description |
|----------|-------------|
| `read_beve_delimited(container, buffer)` | Reads all delimiter-separated values into a container |
| `read_beve_at(value, buffer, offset)` | Reads a single value at offset. Returns bytes consumed. Skips leading delimiter if present. |

### Writing Delimited BEVE

#### Append a Single Value

Use `write_beve_append` to add a value to an existing buffer without clearing it:

```c++
std::string buffer{};

// Write first object
auto result1 = glz::write_beve_append(my_struct{1, "first"}, buffer);
// result1.count contains bytes written

// Append delimiter and second object
auto result2 = glz::write_beve_append_with_delimiter(my_struct{2, "second"}, buffer);

// Append delimiter and third object
auto result3 = glz::write_beve_append_with_delimiter(my_struct{3, "third"}, buffer);
```

The `write_beve_append` function returns `glz::error_ctx` where `ec.count` contains the number of bytes written.

#### Write a Delimiter

You can manually write just the delimiter byte:

```c++
std::string buffer{};
glz::write_beve_append(obj1, buffer);
glz::write_beve_delimiter(buffer);  // Writes single 0x06 byte
glz::write_beve_append(obj2, buffer);
```

#### Write a Container with Delimiters

To write all elements of a container with delimiters between them:

```c++
std::vector<my_struct> objects = {
   {1, "first"},
   {2, "second"},
   {3, "third"}
};

std::string buffer{};
auto ec = glz::write_beve_delimited(objects, buffer);

// Or get the buffer directly:
auto result = glz::write_beve_delimited(objects);
if (result) {
   std::string buffer = std::move(*result);
}
```

### Reading Delimited BEVE

#### Read All Values into a Container

Use `read_beve_delimited` to read all delimiter-separated values:

```c++
std::string buffer = /* delimited BEVE data */;

std::vector<my_struct> objects{};
auto ec = glz::read_beve_delimited(objects, buffer);

// Or get the container directly:
auto result = glz::read_beve_delimited<std::vector<my_struct>>(buffer);
if (result) {
   for (const auto& obj : *result) {
      // process each object
   }
}
```

#### Read at a Specific Offset

For manual control over reading, use `read_beve_at` which returns the number of bytes consumed:

```c++
std::string buffer = /* delimited BEVE data */;
std::size_t offset = 0;

while (offset < buffer.size()) {
   my_struct obj{};
   auto result = glz::read_beve_at(obj, buffer, offset);
   if (!result) {
      break;  // Error or end of data
   }
   offset += *result;  // Advance by bytes consumed

   // Process obj...
}
```

> [!NOTE]
> `read_beve_at` automatically skips a delimiter byte if one is present at the given offset. The returned byte count **includes** the skipped delimiter, so `offset += *result` correctly advances to the next value.

#### Bytes Consumed Tracking

The standard `read_beve` function tracks bytes consumed via `ec.count`:

```c++
my_struct obj{};
auto ec = glz::read_beve(obj, buffer);
if (!ec) {
   std::size_t bytes_consumed = ec.count;  // Number of bytes read on success
}
```

On error, `count` indicates where the parse error occurred. This field is available for all read operations (`read_beve`, `read_json`, `read_cbor`, `read_jsonb`, `read_msgpack`).

### Example: Streaming Workflow

```c++
struct Message {
   int id{};
   std::string content{};
};

// Producer: append messages to a buffer
std::string buffer{};
for (int i = 0; i < 100; ++i) {
   Message msg{i, "message " + std::to_string(i)};
   if (i == 0) {
      glz::write_beve_append(msg, buffer);
   } else {
      glz::write_beve_append_with_delimiter(msg, buffer);
   }
}

// Consumer: read all messages
std::vector<Message> messages{};
auto ec = glz::read_beve_delimited(messages, buffer);
// messages now contains all 100 Message objects
```

### Delimiter Format

The BEVE delimiter is a single byte: `0x06` (extensions type 6 with subtype 0). When converting delimited BEVE to JSON via `glz::beve_to_json`, each delimiter is converted to a newline character (`\n`), producing NDJSON-compatible output.

Values concatenated without delimiters, which `glz::read_beve_delimited` also accepts, get the same newline between them. Two JSON documents never run together into text that is no longer JSON.

## Lazy BEVE Parsing

For scenarios where you need to extract a few fields from large BEVE documents without full deserialization, Glaze provides `glz::lazy_beve`. This offers on-demand parsing with zero upfront processing.

```cpp
std::vector<std::byte> buffer;
glz::write_beve(large_struct, buffer);

auto result = glz::lazy_beve(buffer);
if (result) {
    // Access fields lazily - only parses what you access
    auto name = (*result)["user"]["name"].get<std::string_view>();
    auto age = (*result)["user"]["age"].get<int64_t>();

    // Check container size without parsing elements
    std::size_t count = (*result)["items"].size();
}
```

See [Lazy BEVE](./lazy-beve.md) for full documentation.
