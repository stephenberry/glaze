# Time Trace and Profiling

Use Google's [Perfetto](https://ui.perfetto.dev/) to visualize time traces, enabling simple profiling of your C++. Glaze writes out JSON traces conformant to Google's [Trace Event Format](https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview).

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

## Global tracing

It can be bothersome to pass around a `glz::trace` instance. A global instance is available with a separate API to avoid needing to pass a trace instance around.

`glz::disable_trace()` - Turns off the global tracing.
`glz::enable_trace()` - Turns on the global tracing if it had been disabled (enabled by default).
`glz::trace_begin("my event")` - Add a begin event for "my event"
`glz::trace_end("my event")` - Add an end event for "my event"
`glz::trace_async_begin("my event")` - Add an async_begin event for "my event"
`glz::trace_async_end("my event")` - Add an aysnc_end event for "my event"

Two structs are also provided that will automatically add the end events when they leave scope:
`glz::duration_trace`
`glz::async_trace`

Example:
```c++
void my_function_to_profile()
{
   glz::duration_trace trace{"my function to profile"};
   // do stuff here
   // glz::duration_trace will automatically add the end event when it goes out of scope.
}
```

Then, somewhere at the end of your program, write your global trace to file:

```c++
const auto ec = glz::write_file_trace("trace.json", std::string{});
if (ec) {
  std::cerr << "trace output failed\n";
}
```

