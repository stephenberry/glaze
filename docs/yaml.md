# YAML

Glaze provides a YAML 1.2 reader and writer. The same compile-time reflection metadata used for JSON works for YAML, so you can reuse your `glz::meta` specializations without additional boilerplate.

> **Note:** YAML support requires a separate include. It is not included in `glaze/glaze.hpp`.

## Getting Started

Include `glaze/yaml.hpp` to access the YAML API:

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

# Literal block scalar (preserves newlines)
description: |
  This is line 1.
  This is line 2.
  This is line 3.

# Folded block scalar (folds newlines to spaces)
summary: >
  This is a long
  paragraph that will
  be folded into one line.
```

**Block scalar modifiers:**
- `|` (literal): Preserves all newlines exactly
- `>` (folded): Converts newlines to spaces (paragraph style)
- Chomping: `-` strips trailing newlines, `+` keeps all, default clips to one
- Indentation indicator: `|2` or `>4` for explicit indent level

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

## Tag Validation

Glaze supports YAML Core Schema tags and validates them against the C++ types being parsed. This catches type mismatches at parse time.

### Supported Tags

| Tag | Verbatim Form | Valid C++ Types |
|-----|---------------|-----------------|
| `!!str` | `!<tag:yaml.org,2002:str>` | `std::string`, `std::string_view` |
| `!!int` | `!<tag:yaml.org,2002:int>` | `int`, `int64_t`, `uint32_t`, etc. |
| `!!float` | `!<tag:yaml.org,2002:float>` | `float`, `double` |
| `!!bool` | `!<tag:yaml.org,2002:bool>` | `bool` |
| `!!null` | `!<tag:yaml.org,2002:null>` | `std::optional`, `std::unique_ptr`, pointers |
| `!!seq` | `!<tag:yaml.org,2002:seq>` | `std::vector`, `std::array`, etc. |
| `!!map` | `!<tag:yaml.org,2002:map>` | `std::map`, structs, objects |

### Examples

```yaml
# Valid: tag matches C++ type
name: !!str "Alice"
count: !!int 42
rate: !!float 3.14
enabled: !!bool true
items: !!seq [1, 2, 3]
config: !!map {key: value}
```

```cpp
// When parsing into int, !!str tag causes an error
std::string yaml = "!!str 42";
int value{};
auto ec = glz::read_yaml(value, yaml);
// ec.ec == glz::error_code::syntax_error (tag mismatch!)
```

### Tag Compatibility

- `!!int` is valid for floating-point types (widening conversion allowed)
- `!!float` is NOT valid for integer types
- Unknown or custom tags (e.g., `!mytag`, `!!custom`) are ignored and values parse normally
- Malformed tags (e.g., unterminated `!<...` or invalid tag token syntax) produce `syntax_error`

## Anchors and Aliases

Glaze supports YAML anchors (`&name`) and aliases (`*name`), which let you define a value once and reference it elsewhere in the document. This is useful for avoiding repetition in configuration files.

### Basic Usage

```yaml
defaults: &defaults
  timeout: 30
  retries: 3

production:
  host: prod.example.com
  <<: *defaults  # Not yet supported (merge keys)

# Simple scalar anchors and aliases:
database_host: &db_host db.example.com
primary: *db_host
replica: *db_host
```

### Scalar Anchors

Anchor a scalar value and reuse it via alias:

```yaml
first: &name Alice
second: *name
```

Parses into `{"first": "Alice", "second": "Alice"}`. This works with all scalar types: strings, numbers, booleans, and enums.

```cpp
std::string yaml = R"(
anchor: &val 42
alias: *val)";
std::map<std::string, int> obj{};
auto ec = glz::read_yaml(obj, yaml);
// obj["anchor"] == 42, obj["alias"] == 42
```

### Anchors on Collections

Anchors can be placed on flow-style sequences and mappings:

```yaml
# Flow mapping anchor
defaults: &cfg {x: 1, y: 2}
copy: *cfg

# Flow sequence anchor
items: &list [a, b, c]
more: *list
```

```cpp
std::string yaml = R"(
items: &s [10, 20, 30]
copy: *s)";
std::map<std::string, std::vector<int>> obj{};
auto ec = glz::read_yaml(obj, yaml);
// obj["items"] == {10, 20, 30}, obj["copy"] == {10, 20, 30}
```

### Anchor Override

Redefining an anchor replaces the stored value. Aliases always resolve to the most recent definition:

```yaml
a: &x first
b: *x       # "first"
c: &x second
d: *x       # "second"
```

### Anchors in Sequences

```yaml
- &a hello
- &b world
- *a        # "hello"
- *b        # "world"
```

### Error Handling

An alias referencing an undefined anchor produces a `syntax_error`:

```cpp
std::string yaml = "value: *undefined";
glz::generic parsed{};
auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
// ec is truthy — undefined alias
```

### Current Anchor Limitations

- **Merge keys** (`<<: *alias`) are not yet supported
- **Anchors on empty/null nodes** (`key: &anchor` with no value) are not yet resolved via alias
- **Aliases as mapping keys** (`*alias: value`) have limited support
- **Anchors on block-style collections** (block mappings/sequences on subsequent lines) have limited support; flow-style anchors are fully supported
- **Tag + anchor combinations** (`!!tag &anchor value`) are not supported together

## Limitations

The current YAML implementation has some limitations compared to the full YAML 1.2 specification:

### Not Supported

- **Custom tags** (`!mytag`) - Only YAML Core Schema tags are supported
- **Multi-document streams** - Only single documents are supported
- **Complex keys** - Only simple scalar keys are supported
- **Merge keys** (`<<: *alias`) - Not yet supported

### Tab Indentation

YAML forbids tab characters for indentation. Using tabs will produce a `syntax_error`:

```yaml
# This will error:
key:
	value  # Tab character - not allowed!
```

Always use spaces for indentation in YAML.
