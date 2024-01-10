# Frequently Asked Questions (FAQ)

### What happens when the JSON input is missing objects? 

The input JSON can partial. It could just include a single value. Only what is included in the JSON will be changed. JSON pointers are also supported, which can be used to change a single element in an array (not possible with normal JSON).

### How are unknown keys handled?

By default unknown keys are considered errors. A compile time switch will turn this off and just skip unknown keys:
`glz::read<glz::opts{.error_on_unknown_keys = false}>(...)`

### Can I specify default values?

Yes, whatever defaults are on your C++ structures will be the defaults of the JSON output.

```c++
struct my_struct {
   std::string my_string = "my default value";
};
```

### Do I need to specify the complete structure of the parsed JSON or just the parts I‘m interested in?

You only need to specify the portion that you want to be serialized. You can have whatever else in your class and choose to not expose it.

### Is the write/read API deterministic?

*If I parse and serialize the same JSON string multiple times, is the output guaranteed to be the same every time?*

For the most part, yes, glaze is deterministic. Structs are compile time known, so they're deterministic. The unordered map behavior just means that the input layout doesn't have to be in sequence, conforming to the JSON specification.

You can use std::map and std::unordered_map containers with the library. If you choose the former the sequence is deterministic, but not the latter.

The library is also deterministic from a round-trippable standpoint. Floating point numbers use round-trippable algorithms.
