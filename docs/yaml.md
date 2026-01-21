# YAML

Glaze provides a YAML 1.2 reader and writer. The same compile-time reflection metadata used for JSON works for YAML, so you can reuse your `glz::meta` specializations without additional boilerplate.

## Getting Started

The header `glaze/yaml.hpp` exposes the high-level helpers. The example below writes and reads a configuration struct:

```cpp
#include "glaze/yaml.hpp"

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
std::string yaml{};
auto write_error = glz::write_yaml(cfg, yaml);
if (write_error) {
   const auto message = glz::format_error(write_error, yaml);
   // handle the error message
}

app_config loaded{};
auto read_error = glz::read_yaml(loaded, yaml);
if (read_error) {
   const auto message = glz::format_error(read_error, yaml);
   // handle the error message
}
```

`glz::write_yaml` and `glz::read_yaml` return an `error_ctx`. The object becomes truthy when an error occurred; pass it to `glz::format_error` to obtain a human-readable explanation.

## YAML Output Example

The `app_config` structure above produces:

```yaml
host: "127.0.0.1"
port: 8080
retry:
  attempts: 5
  backoff_ms: 250
features:
  - metrics
```

## YAML Input Example

Glaze supports both block style (with indentation) and flow style (compact) for reading:

**Block style:**
```yaml
host: localhost
port: 9000
retry:
  attempts: 6
  backoff_ms: 500
features:
  - metrics
  - debug
```

**Flow style:**
```yaml
host: localhost
port: 9000
retry: {attempts: 6, backoff_ms: 500}
features: [metrics, debug]
```

## Scalar Types

Glaze YAML supports the following scalar representations:

### Strings

```yaml
# Plain scalars (unquoted)
name: hello world

# Double-quoted with escape sequences
message: "Hello\nWorld"

# Single-quoted (literal, only '' escapes to ')
path: 'C:\Users\name'
```

Double-quoted strings support YAML escape sequences including:
- `\\`, `\"`, `\/`, `\n`, `\r`, `\t`, `\b`, `\f`
- `\0` (null), `\a` (bell), `\v` (vertical tab), `\e` (escape)
- `\xXX` (hex byte), `\uXXXX` (Unicode), `\UXXXXXXXX` (Unicode)
- `\N` (next line U+0085), `\_` (non-breaking space U+00A0)
- `\L` (line separator U+2028), `\P` (paragraph separator U+2029)

### Numbers

```yaml
integer: 42
negative: -17
float: 3.14
scientific: 6.022e23
hex: 0x1A
octal: 0o755
binary: 0b1010
infinity: .inf
neg_infinity: -.inf
not_a_number: .nan
```

### Booleans

```yaml
enabled: true
disabled: false
```

YAML 1.2 Core Schema is used, so `true`/`false` (case-insensitive) are recognized as booleans.

### Null

```yaml
value: null
also_null: ~
```

## Collections

### Sequences (Arrays)

**Block style:**
```yaml
items:
  - first
  - second
  - third
```

**Flow style:**
```yaml
items: [first, second, third]
```

### Mappings (Objects)

**Block style:**
```yaml
person:
  name: John
  age: 30
```

**Flow style:**
```yaml
person: {name: John, age: 30}
```

## Document Markers

Glaze supports YAML document markers:

```yaml
---
# Document start marker (optional)
key: value
...
# Document end marker (optional)
```

The `---` marker indicates the start of a document and is skipped during parsing. The `...` marker indicates the end of a document; any content after it is ignored.

## File Helpers

Glaze provides file-oriented helpers:

```cpp
std::string buffer{};
glz::write_file_yaml(cfg, "config.yaml"); // writes to disk

app_config loaded{};
glz::read_file_yaml(loaded, "config.yaml");
```

## Using the Generic API

You can use the generic `glz::read`/`glz::write` with YAML by setting the format option:

```cpp
app_config cfg{};
auto ec = glz::read<glz::opts{.format = glz::YAML}>(cfg, yaml_text);
if (ec) {
   const auto message = glz::format_error(ec, yaml_text);
   // handle error
}
```

```cpp
std::string yaml{};
auto ec = glz::write<glz::opts{.format = glz::YAML}>(cfg, yaml);
```

## YAML Options

The `glz::yaml::yaml_opts` struct provides YAML-specific options:

| Option | Default | Description |
|--------|---------|-------------|
| `error_on_unknown_keys` | `true` | Error on unknown YAML keys |
| `skip_null_members` | `true` | Skip null values when writing |
| `indent_width` | `2` | Spaces per indent level |
| `flow_style` | `false` | Use flow style (compact) output |

Example with flow style output:

```cpp
auto ec = glz::write<glz::yaml::yaml_opts{.flow_style = true}>(cfg, yaml);
// Output: {host: "127.0.0.1", port: 8080, retry: {attempts: 5, backoff_ms: 250}, features: [metrics]}
```

## Supported Types

YAML support leverages the same type system as JSON. All types that work with `glz::read_json`/`glz::write_json` also work with YAML:

- Arithmetic types (integers, floating-point)
- `std::string`, `std::string_view`
- `std::vector`, `std::array`, `std::deque`, `std::list`
- `std::map`, `std::unordered_map`
- `std::optional`, `std::unique_ptr`, `std::shared_ptr`
- `std::variant`
- `std::tuple`
- User-defined types with `glz::meta` or pure reflection
- Enums (as integers or strings with `glz::meta`)

## Limitations

The current YAML implementation has some limitations compared to the full YAML 1.2 specification:

### Not Supported

- **Anchors and aliases** (`&anchor`, `*alias`) - These will produce a `feature_not_supported` error
- **Tags** (`!tag`, `!!str`) - Custom type tags are not processed
- **Multi-document streams** - Only single documents are supported
- **Literal block scalars** (`|`) and folded block scalars (`>`)
- **Complex keys** - Only simple scalar keys are supported

### Tab Indentation

YAML forbids tab characters for indentation. Using tabs will produce a `syntax_error`:

```yaml
# This will error:
key:
	value  # Tab character - not allowed!
```

Always use spaces for indentation in YAML.
