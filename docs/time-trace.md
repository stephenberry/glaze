# Time Trace and Profiling

Use Google's [Perfetto](https://ui.perfetto.dev/) to visualize time traces, enabling simple profiling of your C++. Glaze writes out JSON traces conformant to Google's [Trace Event Format](https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview).

> The glz::trace class is entirely thread safe.

Glaze provides `begin` and `end` members for duration traces. Asynchronous traces use `async_begin` and `async_end`.

An example of the API in use:

```c++
#include "glaze/trace/trace.hpp"

glz::trace trace{}; // make a structure to store a trace

trace.begin("my event name")
// compute something here
trace.end("my event name")
  
auto ec = glz::write_file_json(trace, "trace.json", std::string{});
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

## Disabling trace

```c++
trace.disabled = true; // no data will be collected
```

## Scoped Tracing

`auto scoper = trace.scope("event name");` automatically ends the event when it goes out of scope.

`auto scoper = trace.async_scope("event name");` automatically ends the async event when it goes out of scope.

## Global tracing

It can be bothersome to pass around a `glz::trace` instance. A global instance can be made for convenience:

```c++
inline glz::trace global_trace{};
```
