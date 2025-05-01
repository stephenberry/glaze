# Time Trace and Profiling

Use Google's [Perfetto](https://ui.perfetto.dev/) to visualize time traces, enabling simple profiling of your C++. Glaze writes out JSON traces conformant to Google's [Trace Event Format](https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview).

> The glz::trace class is entirely thread safe.

Glaze provides `begin` and `end` members for duration traces. Asynchronous traces use `async_begin` and `async_end`.

An example of the API in use:

```c++
#include "glaze/trace/trace.hpp"

glz::trace trace{}; // make a structure to store a trace

trace.begin("my event name");
// compute something here
trace.end("my event name");
  
auto ec = glz::write_file_json(trace, "trace.json", std::string{});
```

Or, for async events:

```c++
glz::trace trace{}; // make a structure to store a trace

trace.async_begin("my event name");
// compute something here
trace.async_end("my event name");
```

## Args (metadata)

Glaze allows `args` as an additional argument to `begin` and `end` methods, which will provide metadata in [Perfetto](https://ui.perfetto.dev/).

```c++
// arguments could be any type that can be serialized to JSON
trace.begin("my event name", arguments);
// do stuff
trace.end("my event name");
```

Example with a string:

```c++
trace.begin("my event name", "My event description");
// do stuff
trace.end("my event name");
```

## Runtime Disabling Trace

```c++
trace.disabled = true; // no data will be collected
```

## Compile Time Disabling Trace

A boolean template parameter `Enabled` defaults to `true`, but can be set to `false` to compile time disable tracing and thus remove the binary when compiled.

```c++
glz::trace<false> trace{};
```

When disabled, writing `glz::trace` as JSON will result in an empty JSON object: `{}`

## Scoped Tracing

Automatically ends the event when it goes out of scope:

```c++
{
  auto scoper = trace.scope("event name"); // trace.begin("event name") called
  // run event
}
// end("event name") automatically called when leaving scope
```

Automatically ends the async event when it goes out of scope:

```c++
{
  auto scoper = trace.async_scope("event name"); // trace.async_begin("event name") called
  // run event
}
// trace.async_end("event name") automatically called when leaving scope
```

## Global tracing

It can be bothersome to pass around a `glz::trace` instance. A global instance can be made for convenience:

```c++
inline glz::trace global_trace{};
```
