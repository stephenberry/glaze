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

## Calculate BEVE Size

The `glz::beve_size` function calculates the exact number of bytes needed to serialize a value to BEVE format, without actually performing the serialization. This is useful when you need to pre-allocate a buffer of the exact size, such as when writing to shared memory for inter-process communication.

**Basic Usage**

```c++
my_struct s{};
size_t size = glz::beve_size(s);
// size contains the exact number of bytes needed to serialize s
```

**Shared Memory Example**

```c++
#include "glaze/beve.hpp"

struct SensorData {
   uint64_t timestamp;
   double temperature;
   std::vector<double> readings;
};

// Calculate size before allocation
SensorData data{12345, 98.6, {1.0, 2.0, 3.0}};
size_t size = glz::beve_size(data);

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
size_t tagged_size = glz::beve_size(s);           // with keys
size_t untagged_size = glz::beve_size_untagged(s); // without keys (smaller)
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
   size_t count{};       // Element count, string length, variant index, etc.
   size_t header_size{}; // Bytes consumed by tag + count encoding
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
| `glz::extension::variant` (1) | Variant | Variant index | 1 + compressed_int size |
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
constexpr size_t max_elements = 10000;
if (header->type == glz::tag::typed_array && header->count > max_elements) {
   return unexpected(error_code::invalid_length);
}

// Proceed with deserialization
return glz::read_beve<std::vector<int>>(buffer);
```

**Variant Index Example**

For variants, `count` contains the variant index, allowing you to determine which type is stored before deserializing:

```c++
using MyVariant = std::variant<int, std::string, double>;
std::string buffer = receive_data();

auto header = glz::beve_peek_header(buffer);
if (header && header->type == glz::tag::extensions
           && header->ext_type == glz::extension::variant) {
   switch (header->count) {
      case 0: std::cout << "Contains int\n"; break;
      case 1: std::cout << "Contains string\n"; break;
      case 2: std::cout << "Contains double\n"; break;
   }
}

MyVariant value;
glz::read_beve(value, buffer);
```

**Raw Pointer Overload**

For C-style buffers:

```c++
const void* data = /* ... */;
size_t size = /* ... */;

auto result = glz::beve_peek_header(data, size);
```

**Peek Header at Offset**

Use `glz::beve_peek_header_at` to peek at headers at arbitrary byte offsets without slicing or copying the buffer:

```c++
std::string buffer = /* BEVE data */;
size_t offset = /* position to inspect */;

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
int32_t val1 = 42;
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

## Untagged Binary

By default Glaze will handle structs as tagged objects, meaning that keys will be written/read. However, structs can be written/read without tags by using the option `structs_as_arrays` or the functions `glz::write_beve_untagged` and `glz::read_beve_untagged`.

## BEVE to JSON Conversion

`glaze/binary/beve_to_json.hpp` provides `glz::beve_to_json`, which directly converts a buffer of BEVE data to a buffer of JSON data.

### Function Pointers

Objects that expose function pointers (both member and non-member) through `glz::meta` are skipped by the BEVE writer by default. This mirrors JSON/TOML behaviour and avoids emitting unusable callable placeholders in binary payloads.

If you want the key present, use `write_function_pointers = true`.

## Custom Map Keys

BEVE can serialize map-like containers whose key types expose a value through Glaze metadata. This allows “strong ID” wrappers to keep a user-defined type while the binary payload stores the underlying numeric representation.

```c++
struct ModuleID {
   uint64_t value{};
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

Glaze inspects the metadata, reuses the underlying `uint64_t`, and emits the numeric BEVE map header so the payload decodes as a regular number key. The same behaviour works for `std::unordered_map` and concatenated ranges such as `std::vector<std::pair<ModuleID, T>>`.

If you prefer to keep a custom conversion in your metadata, `glz::cast` works as well:

```c++
template <>
struct glz::meta<ModuleID> {
   static constexpr auto value = glz::cast<&ModuleID::value, uint64_t>;
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
size_t offset = 0;

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
   size_t bytes_consumed = ec.count;  // Number of bytes read on success
}
```

On error, `count` indicates where the parse error occurred. This field is available for all read operations (`read_beve`, `read_json`, `read_cbor`, `read_msgpack`).

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
    size_t count = (*result)["items"].size();
}
```

See [Lazy BEVE](./lazy-beve.md) for full documentation.
