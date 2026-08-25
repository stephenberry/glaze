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

## Non-owning types cannot be streamed

A refill moves the window, so anything pointing into it dangles. Filling a `std::string_view`, a `glz::raw_json_view`, a `glz::text_view`, or a `std::span<const T>` from a stream is rejected at compile time, wherever that view sits in the destination type:

```cpp
std::vector<std::string_view> views{};
glz::read_json(views, buffer);  // compile error: fills a non-owning view
```

Read into the owning equivalent (`std::string`, `glz::raw_json`, `glz::text`) when the source is a stream. This is not a limitation that can be worked around by sizing the buffer up: whether a given view survives depends on where the refills happen to land, and a read that produces dangling views still reports success, so there is nothing to check at runtime.

The check is on the readers that point into the buffer rather than on the shape of the destination, so it applies equally to a view reached through a container, a `std::tuple`, a map key, or a `glz::custom` setter. A `std::span` over your own storage is not affected — only `std::span<const T>` is ever aimed at the input.

What the check is aimed at is a view the *caller* keeps. A reader that borrows a view of the string it just parsed and turns it into a value before returning — how `std::chrono::system_clock::time_point`, `std::chrono::year_month_day`, and `glz::date_format` fields are read, and how a tagged variant turns its discriminator into an alternative index — holds it across nothing that refills, so those types stream normally.

Buffered reads are unaffected. A buffer holds the whole document for the duration of the call, so views into it stay valid and remain a supported zero-copy idiom.

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

A `std::variant` of objects adds one more case. When the alternative is decided by the shape of the object, or by a tag that is not the first key, the reader scans the object once to work out which alternative it is and then reads it again from the opening brace. Adjacent tagging (`tag` plus `content`) always reads it twice, whatever the key order.

That second pass needs the opening brace to still be in the window. A refill releases whatever the parse has stepped over, so the object has to fit in the room the window has left *at the point it starts* — not in the window's full capacity, since the reader tops the window up between elements rather than at every byte. Where it does not fit, the read stops with `error_code::streaming_unsupported` rather than re-reading relocated bytes. Give the window several times the size of the largest such object, or put an internal tag first, which skips the second pass entirely and streams at any size:

```jsonc
{"type":"circle","points":[ ... a megabyte of them ... ]}  // streams in a 64 KB window
{"points":[ ... a megabyte of them ... ],"type":"circle"}  // needs room for the whole object
```

## See Also

- [Writing](writing.md) - Writing to buffers and error handling
- [Reading](reading.md) - Reading from buffers
