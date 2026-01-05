# Compile Time Options

Glaze provides extensive compile time options to customize serialization/deserialization behavior. This document serves as a comprehensive reference for all available options.

## Quick Reference Tables

The tables below list **all** compile time options, organized by category:

- **Core Options**: Available in the default `glz::opts` struct
- **Inheritable Options**: Must be added to a custom options struct (see [How to Use Inheritable Options](#how-to-use-inheritable-options))
- **CSV Options**: Available in `glz::opts_csv`

### Core Options (`glz::opts`)

| Option | Default | Description |
|--------|---------|-------------|
| `uint32_t format` | `JSON` | Format selector (`JSON`, `BEVE`, `CSV`, `TOML`, etc.) |
| `bool null_terminated` | `true` | Whether the input buffer is null terminated |
| `bool comments` | `false` | Support reading JSONC style comments |
| `bool error_on_unknown_keys` | `true` | Error when an unknown key is encountered |
| `bool skip_null_members` | `true` | Skip writing out members if their value is null |
| `bool prettify` | `false` | Write out prettified JSON |
| `bool minified` | `false` | Require minified input for faster read performance |
| `char indentation_char` | `' '` | Prettified JSON indentation character |
| `uint8_t indentation_width` | `3` | Prettified JSON indentation size |
| `bool new_lines_in_arrays` | `true` | Whether prettified arrays have new lines per element |
| `bool error_on_missing_keys` | `false` | Require all non-nullable keys to be present |
| `bool quoted_num` | `false` | Treat numbers as quoted or arrays as having quoted numbers |
| `bool number` | `false` | Treat string-like types as numbers |
| `bool raw` | `false` | Write string-like values without quotes |
| `bool raw_string` | `false` | Skip escape sequence encoding/decoding for strings |
| `bool structs_as_arrays` | `false` | Handle structs without keys (as arrays) |
| `bool partial_read` | `false` | Exit after reading the deepest structural object |

### Inheritable Options

These options are **not** in `glz::opts` by default. Add them to a custom options struct to use them.

| Option | Default | Description |
|--------|---------|-------------|
| `bool validate_skipped` | `false` | Perform full validation on skipped values |
| `bool validate_trailing_whitespace` | `false` | Validate whitespace after parsing completes |
| `bool bools_as_numbers` | `false` | Read/write booleans as `1` and `0` |
| `bool write_member_functions` | `false` | Serialize member function pointers in `glz::meta` (off by default for safety) |
| `bool concatenate` | `true` | Concatenate ranges of `std::pair` into single objects |
| `bool allow_conversions` | `true` | Allow type conversions in BEVE (e.g., `double` → `float`) |
| `bool write_type_info` | `true` | Write type info for meta objects in variants |
| `bool append_arrays` | `false` | Append to arrays instead of replacing contents |
| `bool shrink_to_fit` | `false` | Shrink dynamic containers after reading |
| `bool error_on_const_read` | `false` | Error when attempting to read into a const value |
| `bool hide_non_invocable` | `true` | Hide non-invocable members from `cli_menu` |
| `bool escape_control_characters` | `false` | Escape control characters as unicode sequences |
| `float_precision float_max_write_precision` | `full` | Maximum precision for writing floats |
| `static constexpr std::string_view float_format` | (none) | Format string for float output using `std::format` (C++23) |
| `bool skip_null_members_on_read` | `false` | Skip null values when reading (preserve existing value) |
| `bool skip_self_constraint` | `false` | Skip `self_constraint` validation during reading |
| `bool assume_sufficient_buffer` | `false` | Skip bounds checking for fixed-size buffers (caller guarantees space) |
| `bool linear_search` | `false` | Use linear key search instead of hash tables for smaller binary size |
| `optimization_level optimization_level` | `normal` | Controls speed vs binary size tradeoff. See [Optimization Levels](optimization-levels.md) |
| `size_t max_string_length` | `0` | Maximum string length when reading (0 = no limit). See also [Runtime Constraints](security.md#runtime-limits-via-custom-context). |
| `size_t max_array_size` | `0` | Maximum array size when reading (0 = no limit). See also [Runtime Constraints](security.md#runtime-limits-via-custom-context). |
| `size_t max_map_size` | `0` | Maximum map size when reading (0 = no limit). See also [Runtime Constraints](security.md#runtime-limits-via-custom-context). |
| `bool allocate_raw_pointers` | `false` | Allocate memory for null raw pointers during deserialization. See also [Runtime Control](security.md#runtime-raw-pointer-allocation-control). |

### CSV Options (`glz::opts_csv`)

For CSV format, use `glz::opts_csv` which provides CSV-specific defaults.

| Option | Default | Description |
|--------|---------|-------------|
| `uint32_t format` | `CSV` | Format selector (fixed to CSV) |
| `bool null_terminated` | `true` | Whether the input buffer is null terminated |
| `uint8_t layout` | `rowwise` | Data layout: `rowwise` or `colwise` |
| `bool use_headers` | `true` | Read/write with column headers |
| `bool raw_string` | `false` | Skip escape sequence encoding/decoding |
| `bool skip_header_row` | `false` | Skip first row when reading |
| `bool validate_rectangular` | `false` | Ensure all rows have equal column counts |

**Notes:**
- `layout`: `rowwise` treats each row as a record; `colwise` uses columns (2D arrays are transposed on IO).
- `use_headers`: when `false`, vectors of structs read/write without headers in declaration order.
- `skip_header_row`: skip the first row during read (useful when ingesting headered CSV into headerless targets).
- `validate_rectangular`: for 2D arrays, fail reads when row lengths differ (`constraint_violated`).
- `append_arrays`: appends parsed values to existing containers when supported (add to custom opts struct).
- Use as a template parameter: `glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(...)`.

---

## How to Use Inheritable Options

Glaze uses C++20 concepts to detect if an option exists. If an option is not present in your options struct, a default value is used. This allows you to create custom option structs with only the options you need.

### Creating a Custom Options Struct

Inherit from `glz::opts` and add the options you need:

```c++
struct my_opts : glz::opts
{
   bool validate_trailing_whitespace = true;
   bool skip_self_constraint = true;
};
```

### Using Custom Options

```c++
constexpr my_opts opts{};
auto ec = glz::read<opts>(obj, buffer);
```

Or specify base options inline:

```c++
// Set both a base option (prettify) and an inheritable option
struct my_write_opts : glz::opts {
   bool escape_control_characters = true;
};
glz::write<my_write_opts{.prettify = true, .escape_control_characters = true}>(obj, buffer);
```

### How Detection Works

Glaze uses `consteval` functions to check for option existence at compile time:

```c++
consteval bool check_validate_trailing_whitespace(auto&& Opts) {
   if constexpr (requires { Opts.validate_trailing_whitespace; }) {
      return Opts.validate_trailing_whitespace;
   } else {
      return false; // Default value
   }
}
```

---

## Option Details

### Format & Parsing Options

#### `format`
Selects the serialization format. Built-in formats include:
- `glz::JSON` (10) - JSON format
- `glz::BEVE` (1) - Binary Efficient Versatile Encoding
- `glz::CSV` (10000) - Comma Separated Values
- `glz::TOML` (400) - Tom's Obvious Minimal Language
- `glz::NDJSON` (100) - Newline Delimited JSON

#### `null_terminated`
When `true` (default), Glaze assumes input buffers are null-terminated, enabling certain optimizations. Set to `false` for non-null-terminated buffers with a small performance cost.

#### `comments`
Enable JSONC-style comment parsing (`//` and `/* */`).

#### `minified`
When `true`, assumes input JSON has no unnecessary whitespace. This skips all whitespace checking during parsing, providing faster performance and slightly smaller binary size. Will fail on prettified input with whitespace between tokens.

### Validation Options

#### `error_on_unknown_keys`
When `true` (default), parsing fails if the JSON contains keys not present in your C++ struct. Set to `false` to silently skip unknown keys.

#### `error_on_missing_keys`
When `true`, parsing fails if required keys are missing from the JSON input. Works with `skip_null_members = false` to also require nullable members. Can be customized per-type using `meta<T>::requires_key(key, is_nullable)` function (see [Field Validation](field-validation.md)).

#### `validate_skipped`
Performs full JSON validation on values that are skipped (unknown keys). Without this, Glaze only validates that skipped content is structurally valid.

#### `validate_trailing_whitespace`
Validates that content after the parsed value contains only valid whitespace.

#### `skip_self_constraint`
Skips `self_constraint` validation during deserialization. Useful for performance when data is known to be valid. See [Wrappers](wrappers.md) for more on constraints.

#### `error_on_const_read`
When `true`, attempting to read into a const value produces an error. By default, const values are silently skipped.

#### `assume_sufficient_buffer`
When `true`, skips bounds checking for fixed-size buffers (`std::array`, `std::span`). Use this when you've pre-validated that the buffer is large enough, for maximum write performance. Has no effect on resizable buffers (`std::string`, `std::vector`) or raw pointers (`char*`).

```c++
struct fast_opts : glz::opts {
   bool assume_sufficient_buffer = true;
};

std::array<char, 8192> large_buffer;
auto result = glz::write<fast_opts{}>(obj, large_buffer);
// No bounds checking overhead - caller guarantees sufficient space
```

### Output Control Options

#### `skip_null_members`
When `true` (default), null values are omitted from JSON output. This applies to nullable types like `std::optional`, `std::shared_ptr`, `std::unique_ptr`, and raw pointers (`T*`).

#### `prettify`
When `true`, outputs formatted JSON with indentation and newlines.

#### `indentation_char` / `indentation_width`
Control prettified output formatting. Default is 3 spaces per level.

#### `new_lines_in_arrays`
When `true` (default), prettified arrays have each element on its own line.

#### `raw` / `raw_string`
Control string quoting and escape sequence handling. Useful for embedding pre-formatted content.

#### `escape_control_characters`
When `true`, control characters (0x00-0x1F) are escaped as `\uXXXX` sequences. The default (`false`) does not escape these characters for performance and safety (embedding nulls can cause issues, especially with C APIs). Glaze will error when parsing non-escaped control characters per the JSON spec—this option allows writing them as escaped unicode to avoid such errors on re-read.

### Performance Options

#### `partial_read`
Exits parsing after reading the deepest structural object. Useful for reading headers or partial documents.

#### `append_arrays`
When reading into arrays, appends new elements instead of replacing existing contents.

#### `shrink_to_fit`
Calls `shrink_to_fit()` on dynamic containers after reading to minimize memory usage.

### Binary Size Options

#### `linear_search`
When `true`, uses linear search for object key lookup instead of compile-time generated hash tables. This significantly reduces binary size (typically 40-50% smaller) at the cost of slightly slower parsing for objects with many fields.

```cpp
struct small_opts : glz::opts {
   bool linear_search = true;
};

auto ec = glz::read<small_opts{}>(obj, buffer);
```

**How it works:**

By default, Glaze generates perfect hash tables at compile time for each struct type, enabling O(1) key lookup. This provides excellent runtime performance but increases binary size because:
- Each struct type gets its own hash table stored in the binary
- Jump tables are generated for field dispatch
- Multiple template instantiations may be created for optimization

With `linear_search = true`:
- Keys are matched using simple linear comparison (O(n) where n = number of fields)
- Hash tables are not generated
- Fold-expression dispatch replaces jump tables
- Template instantiation bloat is reduced

**When to use:**

- Embedded systems with limited flash/ROM
- Applications where binary size matters more than peak parsing speed
- Objects with few fields (< 10) where linear search has negligible overhead
- Shipping smaller binaries to reduce download/install size

**Trade-offs:**

| Aspect | Default (hash) | `linear_search = true` |
|--------|----------------|------------------------|
| Key lookup | O(1) | O(n) |
| Binary size | Larger | ~40-50% smaller |
| Parse speed | Fastest | Slightly slower for large objects |
| Compile time | Longer | Shorter |

For objects with few fields, the performance difference is negligible. For objects with many fields (20+), hash-based lookup will be noticeably faster.

### Type Handling Options

#### `quoted_num`
Treats numbers as quoted strings (e.g., `"123"` instead of `123`).

#### `number`
Treats string-like types as unquoted numbers.

#### `structs_as_arrays`
Serializes/deserializes structs as JSON arrays (without keys), using field order.

#### `bools_as_numbers`
Reads/writes boolean values as `1` and `0` instead of `true` and `false`.

#### `write_type_info`
When `true` (default), includes type discriminator information for variant types.

#### `concatenate`
When `true` (default), ranges of `std::pair` are written as a single object with pairs as key-value entries.

#### `allow_conversions`
When `true` (default), BEVE allows implicit type conversions (e.g., reading a `double` into a `float`).

#### `allocate_raw_pointers`
When `true`, allows Glaze to allocate memory for null raw pointers during deserialization using `new`. By default (`false`), Glaze refuses to read into null raw pointers because it would need to allocate memory without any mechanism to free it, making memory leaks likely.

```c++
struct alloc_opts : glz::opts {
   bool allocate_raw_pointers = true;
};

struct example { int x, y, z; };

// Without the option, this fails with invalid_nullable_read
std::vector<example*> vec;
std::string json = R"([{"x":1,"y":2,"z":3}])";
auto ec = glz::read<alloc_opts{}>(vec, json);
// vec[0] is now allocated and populated

// IMPORTANT: You must manually delete allocated pointers
for (auto* p : vec) delete p;
```

**Runtime control**: You can also control this option at runtime using a custom context. This is useful for applications that need to toggle raw pointer allocation based on the trust level of the data source:

```c++
struct secure_context : glz::context {
   bool allocate_raw_pointers = false;
};

secure_context ctx;
ctx.allocate_raw_pointers = is_trusted_source;
auto ec = glz::read<glz::opts{}>(obj, buffer, ctx);
```

See [Runtime Raw Pointer Allocation Control](security.md#runtime-raw-pointer-allocation-control) for more details.

> [!CAUTION]
> When using this option, **you are responsible for freeing the allocated memory**. Glaze uses `new` to allocate but cannot track or delete the memory. Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) when possible instead.

This option works with JSON, BEVE, CBOR, and MSGPACK formats. See [Nullable Types](nullable-types.md) for more details.

#### `float_max_write_precision`
Limits the precision used when writing floating-point numbers. Options: `full`, `float32`, `float64`, `float128`.

#### `float_format`
Formats all floating-point numbers using `std::format` syntax (C++23). This provides control over decimal places, scientific notation, and other formatting options.

```c++
struct format_opts : glz::opts
{
   static constexpr std::string_view float_format = "{:.2f}";
};

std::vector<double> values{3.14159, 2.71828, 1.41421};
std::string json = glz::write<format_opts{}>(values).value_or("error");
// Output: [3.14,2.72,1.41]
```

Common format specifiers:
- `{:.2f}` - Fixed-point with 2 decimal places
- `{:.0f}` - Fixed-point with no decimals (rounds)
- `{:.2e}` - Scientific notation (lowercase)
- `{:.4g}` - General format (auto-selects fixed or scientific)

> [!NOTE]
> The format string is validated at compile time via `std::format_string`. Invalid format strings will cause compilation errors.

For per-member formatting control, see the [`glz::float_format` wrapper](wrappers.md#float_format).

---

## See Also

- [Optimization Levels](optimization-levels.md) - Control binary size vs performance tradeoff
- [Wrappers](wrappers.md) - Per-field options using wrappers
- [Field Validation](field-validation.md) - Customizing required field validation
- [Partial Read](partial-read.md) - Reading partial documents
- [Optimizing Performance](optimizing-performance.md) - Binary size and compilation time optimization
