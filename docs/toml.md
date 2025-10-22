# TOML (Tom's Obvious, Minimal Language)

Glaze ships with a fast TOML 1.0 reader and writer. The same compile-time reflection metadata you already use for JSON works for TOML, so you can reuse your `glz::meta` specializations without additional boilerplate.

## Getting Started

The header `glaze/toml.hpp` exposes the high-level helpers. The example below writes and reads a configuration struct:

```cpp
#include "glaze/toml.hpp"

struct retry_policy
{
   int attempts = 5;
   int backoff_ms = 250;
};

template <>
struct glz::meta<retry_policy>
{
   using T = retry_policy;
   static constexpr auto value = object(&T::attempts, &T::backoff_ms);
};

struct app_config
{
   std::string host = "127.0.0.1";
   int port = 8080;
   retry_policy retry{};
   std::vector<std::string> features{"metrics"};
};

template <>
struct glz::meta<app_config>
{
   using T = app_config;
   static constexpr auto value = object(&T::host, &T::port, &T::retry, &T::features);
};

app_config cfg{};
std::string toml{};
auto write_error = glz::write_toml(cfg, toml);
if (write_error) {
   const auto message = glz::format_error(write_error, toml);
   // handle the error message
}

app_config loaded{};
auto read_error = glz::read_toml(loaded, toml);
if (read_error) {
   const auto message = glz::format_error(read_error, toml);
   // handle the error message
}
```

`glz::write_toml` and `glz::read_toml` return an `error_ctx`. The object becomes truthy when an error occurred; pass it to `glz::format_error` to obtain a human-readable explanation.

## TOML Input Example

The `app_config` structure above accepts both inline tables and dotted keys. Either of the snippets below will populate the same object:

```toml
host = "0.0.0.0"
port = 9000
features = ["metrics", "debug"]

retry = { attempts = 6, backoff_ms = 500 }
```

```toml
host = "0.0.0.0"
port = 9000
features = ["metrics", "debug"]

retry.attempts = 6
retry.backoff_ms = 500
```

Glaze understands standard TOML number formats (binary, octal, hex), quoted and multiline strings, arrays, inline tables, and comments (`#`).

## Using the Generic API

The convenience wrappers call into the generic `glz::read`/`glz::write` pipeline. You can reuse the same options struct you already use for JSON while switching the format to TOML:

```cpp
std::string_view config_text = R"(
host = "0.0.0.0"
port = 9000
retry.attempts = 4
retry.backoff_ms = 200
extra.flag = true
)";

app_config cfg{};
auto ec = glz::read<glz::opts{.format = glz::TOML, .error_on_unknown_keys = false}>(cfg, config_text);
if (ec) {
   const auto message = glz::format_error(ec, config_text);
   // handle unknown field or parse problems
}
```

Setting `.error_on_unknown_keys = false` allows dotted keys that do not correspond to reflected members to be skipped gracefully. Any other option in `glz::opts` (for example `.skip_null_members` or `.error_on_missing_keys`) can be combined the same way.

The write side uses the same mechanism:

```cpp
std::string toml{};
auto write_ec = glz::write<glz::opts{.format = glz::TOML, .skip_null_members = false}>(cfg, toml);
if (write_ec) {
   const auto message = glz::format_error(write_ec, toml);
   // handle write problems
}
```

Both `glz::read` and `glz::write` return `error_ctx`, so remember to check the result in production code.

## File Helpers and Buffers

For convenience Glaze also provides file-oriented helpers:

```cpp
std::string buffer{};
glz::write_file_toml(cfg, "config.toml", buffer); // writes to disk when serialization succeeds

app_config loaded{};
glz::read_file_toml(loaded, "config.toml", buffer);
```

`glz::read_toml` works with `std::string`, `std::string_view`, or any contiguous character buffer.
