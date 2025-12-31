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

| Format | Output Streaming | Input Streaming |
|--------|-----------------|-----------------|
| JSON   | `ostream_buffer` | `istream_buffer` |
| BEVE   | `ostream_buffer` | `istream_buffer` |
| NDJSON | `ostream_buffer` | `json_stream_reader` |

## See Also

- [Writing](writing.md) - Writing to buffers and error handling
- [Reading](reading.md) - Reading from buffers
