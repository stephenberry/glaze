# Streaming I/O

Glaze supports streaming serialization and deserialization for processing large data with bounded memory usage. This enables reading/writing files of arbitrary size without loading everything into memory.

## Output Streaming (`basic_ostream_buffer`)

Write directly to files or output streams with incremental flushing:

```cpp
#include "glaze/core/ostream_buffer.hpp"

// Write to file with concrete type (enables devirtualization)
std::ofstream file("output.json");
glz::basic_ostream_buffer<std::ofstream> buffer(file);
auto ec = glz::write_json(obj, buffer);
if (ec || !file.good()) {
    // Handle error
}

// Write to any std::ostream (polymorphic)
glz::ostream_buffer<> buffer2(any_ostream);  // Alias for basic_ostream_buffer<std::ostream>
glz::write_json(obj, buffer2);
```

**Template parameters:**

```cpp
// basic_ostream_buffer<Stream, Capacity>
// - Stream: Output stream type (must satisfy byte_output_stream concept)
// - Capacity: Initial buffer size in bytes (default 64KB)

glz::basic_ostream_buffer<std::ofstream> buf1(file);           // Concrete type
glz::basic_ostream_buffer<std::ofstream, 4096> buf2(file);     // 4KB buffer
glz::ostream_buffer<> buf3(any_ostream);                       // Polymorphic, 64KB
glz::ostream_buffer<4096> buf4(any_ostream);                   // Polymorphic, 4KB
```

## Input Streaming (`basic_istream_buffer`)

Read directly from files or input streams with automatic refilling:

```cpp
#include "glaze/core/istream_buffer.hpp"

// Read from file with concrete type (enables devirtualization)
std::ifstream file("input.json");
glz::basic_istream_buffer<std::ifstream> buffer(file);
my_struct obj;
auto ec = glz::read_json(obj, buffer);
if (ec) {
    // Handle error
}

// Read from any std::istream (polymorphic)
glz::istream_buffer<> buffer2(any_istream);  // Alias for basic_istream_buffer<std::istream>
glz::read_json(obj, buffer2);
```

**Template parameters:**

```cpp
// basic_istream_buffer<Stream, Capacity>
// - Stream: Input stream type (must satisfy byte_input_stream concept)
// - Capacity: Buffer size in bytes (default 64KB)

glz::basic_istream_buffer<std::ifstream> buf1(file);           // Concrete type
glz::basic_istream_buffer<std::ifstream, 4096> buf2(file);     // 4KB buffer
glz::istream_buffer<> buf3(any_istream);                       // Polymorphic, 64KB
glz::istream_buffer<4096> buf4(any_istream);                   // Polymorphic, 4KB
```

## Streaming JSON/NDJSON (`json_stream_reader`)

Process streams of JSON objects (NDJSON or multiple JSON values) one at a time:

```cpp
#include "glaze/json/json_stream.hpp"

struct Event {
    int id;
    std::string type;
};

// Manual iteration
std::ifstream file("events.ndjson");
glz::json_stream_reader<Event> reader(file);
Event event;
while (!reader.read_next(event)) {
    process(event);
}

// Range-based for loop
for (auto&& event : glz::json_stream_reader<Event>(file)) {
    process(event);
}

// Convenience function to read all values
std::vector<Event> events;
auto ec = glz::read_json_stream(events, file);
```

**Key features:**

- Processes one complete value at a time (bounded memory)
- Supports NDJSON (newline-delimited) and consecutive JSON values
- Iterator interface for range-based for loops
- Automatic whitespace/newline skipping between values

## Stream Concepts

Only byte-oriented streams are supported. Wide character streams are rejected at compile time:

```cpp
// OK - byte streams
static_assert(glz::byte_output_stream<std::ostream>);
static_assert(glz::byte_output_stream<std::ofstream>);
static_assert(glz::byte_input_stream<std::istream>);
static_assert(glz::byte_input_stream<std::ifstream>);

// Compile error - wide streams not supported (JSON is UTF-8)
// glz::basic_ostream_buffer<std::wostream> bad(wstream);  // Error!
// glz::basic_istream_buffer<std::wistream> bad(wstream);  // Error!
```

## Format Support

Reading a document larger than the buffer window requires the format's reader to be able to refill partway through a parse. Not every reader can, and `glz::format_supports_streaming<Format>` says which:

| Format | Output streaming | Reads past one window |
|--------|------------------|-----------------------|
| JSON   | `ostream_buffer` | yes, refills between array elements and object members |
| NDJSON | `ostream_buffer` | yes, refills between records |
| BEVE   | `ostream_buffer` | no |
| others | `ostream_buffer` | no |

Passing an `istream_buffer` to any format is safe: the read is bounded by the window either way. A format whose reader cannot refill simply sees the first window, and a document that outruns it fails with `error_code::streaming_unsupported` rather than silently returning the part that fit.

### What still has to fit in the window

Refill points sit *between* values, never inside one. A single JSON string or number is read in one piece, so it has to fit in the window; the capacity is the ceiling on one token, not on the document. For NDJSON the unit is the record: a record and its newline have to fit, and a record only has to fit in the *window*, not in whatever the record before it left over. Outrunning the window reports `error_code::streaming_unsupported`, which names the buffer rather than blaming the document. Raise the capacity if you hit it:

```cpp
glz::basic_istream_buffer<std::ifstream, 1 << 20> buffer(file);  // 1 MB window
```

Leading whitespace is also read without refilling, so whitespace wider than the window stops a read before it reaches the value behind it.

### Non-owning types are unsafe to stream

A refill moves the window, so anything pointing into it dangles. Reading into a type that holds `std::string_view` or `glz::raw_json_view` — directly, or as a member, or as the element of a container — gives views into bytes that have since been overwritten, and the read still reports success. This is not currently diagnosed.

```cpp
std::vector<std::string_view> views{};
glz::read_json(views, buffer);  // silently wrong once the document outruns one window
```

Read into owning types (`std::string`, `glz::raw_json`) when the source is a stream. This applies to every streaming read, not just NDJSON.

## See Also

- [Writing](writing.md) - Writing to buffers and error handling
- [Reading](reading.md) - Reading from buffers
