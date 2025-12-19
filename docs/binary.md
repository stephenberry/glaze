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

## Untagged Binary

By default Glaze will handle structs as tagged objects, meaning that keys will be written/read. However, structs can be written/read without tags by using the option `structs_as_arrays` or the functions `glz::write_beve_untagged` and `glz::read_beve_untagged`.

## BEVE to JSON Conversion

`glaze/binary/beve_to_json.hpp` provides `glz::beve_to_json`, which directly converts a buffer of BEVE data to a buffer of JSON data.

### Member Function Pointers

Objects that expose member function pointers through `glz::meta` are skipped by the BEVE writer by default. This mirrors JSON/TOML behaviour and avoids emitting unusable callable placeholders in binary payloads.

If you want the key present, use `write_member_functions = true`.

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
