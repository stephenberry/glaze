# Incremental JSON Arrays

> [!NOTE]
>
> This is a proposal, and the API is open to change. See [issue #1019](https://github.com/stephenberry/glaze/issues/1019).

`glz::json_array_reader` reads a JSON array out of a contiguous buffer one element at a time, keeping its place between calls. `glz::json_array_writer` appends values to a buffer as the elements of a JSON array. Elements go through the normal JSON read and write paths under the options you give.

```c++
#include "glaze/json/json_array.hpp"
```

## Which API for which job

| You want to | Use |
|---|---|
| Read or write a document too large for memory | [`basic_istream_buffer`, `basic_ostream_buffer`](./streaming.md) |
| Read separate values one after another (NDJSON) | [`json_stream_reader`](./streaming.md) |
| Parse data as it arrives, blocking until more comes | A custom `std::streambuf` with [`istream_buffer`](./streaming.md) |
| Fill the first few fields or elements, then stop | [`partial_read`](./partial-read.md) |
| Pull a few values out of a document, trusting its structure | [`glz::lazy_json`](./lazy-json.md) |
| Read a value at a known path | [JSON pointer syntax](./json-pointer-syntax.md), [JMESPath](./JMESPath.md) |
| Read a header element, then decide how to read the rest of a buffer, with errors reported | `json_array_reader` |
| Build up an array in a buffer as values become available | `json_array_writer` |
| Parse a partial buffer and resume later when more arrives | Not supported, see [what this does not do](#what-this-does-not-do) |

`glz::lazy_json` iterators also keep their place in an array, and are the better choice when the input is trusted. They do not look at the separators, so `[1 2]` iterates as two elements, and `[1,2` and `[` end the iteration with no error. `read_into` reports `count` relative to the element rather than the buffer. `json_array_reader` is the one that says so when the array is malformed.

## Reading

```c++
std::string buffer = R"([{"version":2,"kind":"records"},{"id":1,"name":"a"},{"id":2,"name":"b"}])";

glz::json_array_reader reader{buffer};

header h{};
if (auto ec = reader.read(h)) {
   std::cerr << glz::format_error(ec, buffer);
}

std::vector<record> records{};
if (auto ec = reader.read_rest(records)) {
   std::cerr << glz::format_error(ec, buffer);
}
```

| Call | Does |
|---|---|
| `read(value)` | Reads the next element into `value` |
| `skip()` | Steps over the next element without validating its contents |
| `read_rest(container)` | Appends all remaining elements to `container`, which needs `push_back` and a default constructible `value_type` |

Options go on the class, for example `glz::json_array_reader<glz::opts{.comments = true}>`.

Every call returns a `glz::error_ctx` whose `count` is a position in the whole buffer, so it can be handed to `glz::format_error` along with the buffer. On success it is the position just past what was read. Nothing before it is looked at again.

`read` and `skip` report a closed array as `error_code::end_reached`, which makes this the usual loop:

```c++
glz::error_ctx ec{};
while (!(ec = reader.read(value))) {
   process(value);
}
if (ec != glz::error_code::end_reached) {
   std::cerr << glz::format_error(ec, buffer);
}
```

`read_rest` reports a closed array as success.

### Defined behaviour

| Input | Result |
|---|---|
| `[]`, `[ ]` | The first `read` returns `end_reached`, `read_rest` succeeds and adds nothing |
| A read after the array closed | `end_reached` again, with the same `count` |
| Whitespace around the array and between elements | Skipped, and `count` stops just past the `]` |
| Other bytes after the `]` | Ignored, as `glz::read` ignores them. With `validate_trailing_whitespace` it is a `syntax_error`, as it is for `glz::read` |
| `[1 2]`, `[1;2]` | The `1` is read, then the next call returns `expected_bracket` at the bad byte |
| `[1,]`, `[,1]`, `[1,,2]` | `syntax_error` at the `]` or `,` that stands where an element should be, for every element type |
| `[1,2`, `[1,`, `[` | `unexpected_end`. An element that runs up to the end of the buffer fails rather than succeeding, because a number there may have been cut short |
| `{"a":1}`, `5` | `expected_bracket` at the first byte |
| An empty or blank buffer | `no_read_input` |
| An element that fails to parse | Its own error at its own byte, the same one a read of the whole array reports |

Other than for a missing element, the codes and positions are those of `glz::read` into a `std::vector` with `null_terminated = false`.

An error is final. The reader may have stopped inside the failed element, so every later call returns the same error, and `read_rest` leaves out the element that failed.

### Buffers

The buffer must outlive the reader, and a temporary `std::string` is rejected at compile time. Views read out of it, such as `std::string_view` elements, stay valid as long as the buffer does.

The reader always runs with bounds and never reads past `size()`, whatever the `null_terminated` and `is_padded` options say. A `std::string_view` over part of a larger buffer needs no options. The cost is the speed of a bounded read, even on a `std::string` where `glz::read` could rely on the terminator.

### partial_read

An element read under `partial_read` stops once it has what it wants. The reader then finds the end of the element by scanning it from its start, so the next call lands on the next element. This is the one case where bytes are looked at twice.

## Writing

```c++
std::string buffer{};
glz::json_array_writer writer{buffer}; // buffer is []

if (auto ec = writer.append(1)) { /* ... */ } // buffer is [1]
if (auto ec = writer.append(2)) { /* ... */ } // buffer is [1,2]
```

Options go after the buffer type, for example `glz::json_array_writer<std::string, glz::opts{.prettify = true}>`.

The array is closed after every call, and the next `append` overwrites the closing bracket, so **there is no close to call and none to forget**. Whatever point you stop at, the buffer holds a complete array.

- The buffer's existing contents are kept, and the array starts after them.
- A failed `append` restores the buffer to what it was before the call, and later appends may still succeed. On failure and on success, `count` is the size of the buffer.
- The result, with `prettify` too, is what `glz::write` produces for a `std::vector` of the same values. If you already hold all of them, write the vector: that is faster, since it does not have to keep a closed array after every value.
- Nothing else may change the buffer while the writer is in use.
- Any resizable output buffer works. Fixed size buffers are not supported, because they have no way to report the length of the array, and neither are streaming buffers, because a closing bracket that has been flushed cannot be overwritten.

## What this does not do

- **No resuming a partial value.** A buffer that ends part way through the array or an element is `unexpected_end`, and the reader cannot continue from there when more data arrives. There is no `incomplete` status. See [Streaming I/O](./streaming.md) for reading data as it arrives with a blocking source.
- **No objects.** Only arrays, and only one at a time. For an array inside a document, get it with `glz::lazy_json` and `raw_json()` and hand that to the reader. `count` is then relative to the view you gave it.
- **No validation of what is never reached.** Stopping after the header means the rest of the buffer has not been looked at.
- **No random access.** The reader moves forward only. See `.index()` in [Lazy JSON](./lazy-json.md).
- **Only JSON.**
