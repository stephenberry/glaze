# v6.4.1

This release brings new formatting controls, TOML enum support, extended binary format validation, and comprehensive SSL/TLS networking improvements.

## Features

### Float Formatting Control

New `float_format` option provides flexible control over floating-point precision in JSON output using C++23 `std::format` specifiers.

**Global Option:**
```cpp
struct my_opts : glz::opts {
   static constexpr std::string_view float_format = "{:.2f}";
};

double pi = 3.14159265358979;
glz::write<my_opts{}>(pi); // "3.14"
```

**Per-Member Wrapper:**
```cpp
template <>
struct glz::meta<my_type> {
   using T = my_type;
   static constexpr auto value = glz::object(
      "lat", glz::float_format<&T::latitude, "{:.4f}">,
      "lon", glz::float_format<&T::longitude, "{:.4f}">
   );
};
```

[#2179](https://github.com/stephenberry/glaze/pull/2179)

### TOML Enum Support

Adds enum serialization and deserialization for TOML format, matching existing JSON enum functionality.

```cpp
enum class Status { Pending, Active, Completed };

template <>
struct glz::meta<Status> {
   using enum Status;
   static constexpr auto value = glz::enumerate(Pending, Active, Completed);
};

Status s = Status::Active;
auto toml = glz::write_toml(s);  // Returns: "Active"

Status parsed;
glz::read_toml(parsed, R"("Completed")");  // parsed == Status::Completed
```

[#2181](https://github.com/stephenberry/glaze/pull/2181)

### `error_on_missing_keys` for BEVE and MessagePack

The `error_on_missing_keys` option now works with BEVE and MessagePack formats, not just JSON. When enabled, deserialization fails with a `missing_key` error if required fields are absent. Optional/nullable fields (`std::optional`, `std::unique_ptr`) are still allowed to be missing.

[#2184](https://github.com/stephenberry/glaze/pull/2184)

### SSL WebSocket and HTTPS Streaming Support

Comprehensive SSL/TLS support for WebSockets (WSS) and HTTPS streaming.

**Key Features:**
- WSS server support for secure WebSocket connections
- HTTPS streaming with `streaming_connection_interface` type erasure
- `websocket_connection_interface` for socket-type agnostic handlers
- SSL verify mode configuration for WebSocket clients
- ASIO 1.32+ compatibility fix for SSL streams

```cpp
websocket_client client;
client.set_ssl_verify_mode(asio::ssl::verify_none);  // For self-signed certs
client.connect("wss://localhost:8443/ws");
```

[#2165](https://github.com/stephenberry/glaze/pull/2165)

## Improvements

- **API Consistency:** Added `glz::read_beve_untagged` to match `write_beve_untagged` naming convention. `read_binary_untagged` is now deprecated. [#2178](https://github.com/stephenberry/glaze/pull/2178)
- **`GLIBCXX_USE_CXX11_ABI=0` Support:** Glaze can now be used and tested with the legacy GCC ABI. [#2160](https://github.com/stephenberry/glaze/pull/2160)
- **MessagePack Fuzz Testing:** Added fuzz testing for MessagePack format. [#2159](https://github.com/stephenberry/glaze/pull/2159)

## Bug Fixes

- **Tagged Variant with Empty Structs:** Fixed roundtrip failure for tagged variants containing empty struct types. [#2180](https://github.com/stephenberry/glaze/pull/2180)
- **BEVE `std::array<bool, N>` Compilation:** Fixed constexpr compilation error when serializing `std::array<bool, N>` with BEVE format. [#2177](https://github.com/stephenberry/glaze/pull/2177)
- **Invalid Control Code Parsing:** Fixed parsing of strings containing invalid control character sequences. [#2169](https://github.com/stephenberry/glaze/pull/2169)
- **BEVE Skip Logic:** Fixed bug where boolean and string typed arrays were handled incorrectly when skipping unknown keys in BEVE format. [#2184](https://github.com/stephenberry/glaze/pull/2184)

**Full Changelog:** [v6.4.0...v6.4.1](https://github.com/stephenberry/glaze/compare/v6.4.0...v6.4.1)
