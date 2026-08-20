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

An Erlang binary holds arbitrary bytes by definition, and `binary_as_base64` is off by default, so a binary is emitted as a JSON string. A control character (0x00 to 0x1F) among those bytes has no two-character JSON escape, so the conversion fails with `error_code::invalid_control_character` rather than produce output that will not re-parse.

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

See [Untrusted Strings](../binary.md#untrusted-strings) for what that trade buys and costs.

Only strings, atoms, and binaries are accepted as object keys, since a JSON key must be a string. The atoms `true` and `false` emit as bare JSON literals in value position, and stay quoted in key position.
