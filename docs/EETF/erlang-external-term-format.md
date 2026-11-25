# Erlang External Term Format

Glaze optionally supports the [Erlang External Term Format](https://www.erlang.org/doc/apps/erts/erl_ext_dist.html), abbreviated as EETF.

The Glaze CMake provides the option: `glaze_EETF_FORMAT`. Enable this option to link in the require Erlang libraries.

> In the long term the goal is to remove the requirement on the Erlang libraries, but they are required for now.

If you are not using CMake the macro `GLZ_ENABLE_EETF` must be added to utilize Glaze's EETF headers. The `glaze_EETF_FORMAT` will add the `GLZ_ENABLE_EETF` macro if selected.

## [See Unit Tests](https://github.com/stephenberry/glaze/blob/main/tests/eetf_test/eetf_test.cpp)

## EETF to JSON Conversion

`glz::eetf_to_json` converts a buffer of Erlang terms directly to a buffer of JSON.

```c++
std::string json{};
auto ec = glz::eetf_to_json(term, json);
```

`binary_as_base64` is off by default, so an Erlang binary is written as a JSON string. Binaries hold arbitrary bytes, and a control character (0x00–1F) among them has no short JSON escape, so the conversion fails with `error_code::invalid_control_character`.

Two ways across:

```c++
// binary_as_base64 is a member of eetf_opts, so set it inline.
// Usually the right answer for a binary: no raw bytes end up in a JSON string.
glz::eetf_to_json<glz::eetf::eetf_opts{.binary_as_base64 = true}>(term, json);

// escape_control_characters is inheritable, so add it to a custom opts struct.
// Keeps the bytes as JSON string content, escaped.
struct escaping : glz::eetf::eetf_opts {
   bool escape_control_characters = true;
};
glz::eetf_to_json<escaping{}>(term, json);
```

See [String Escaping](../binary.md#string-escaping).

Only strings, atoms, and binaries work as object keys, since a JSON key must be a string. The atoms `true` and `false` are written as bare JSON literals, except as a key, where they stay quoted.
