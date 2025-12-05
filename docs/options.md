# Compile Time Options

Glaze has a large number of available compile time options. These options are passed as a non-type template parameter to `read/write` and `from/to` calls.

In order to keep template errors more succinct and allow more customization, Glaze allows (and in some cases requires) users to build their own option structs.

Example:

```c++
struct custom_opts : glz::opts // Inherit from glz::opts to get basic options
{
  // Add known Glaze options that do not exist in the base glz::opts
  bool validate_trailing_whitespace = true;
};
```

In use:

```c++
// Using custom_opts while specifying a base option (prettify) 
glz::write<custom_opts{{glz::opts{.prettify = true}}, true}>(obj);
```

## How Custom Options Work

Glaze uses C++20 concepts to detect if an option exists in the provided options struct, if it doesn't exist, a default value is returned.

```c++
// Code from Glaze demonstrating compile time option check:
consteval bool check_validate_trailing_whitespace(auto&& Opts) {
  if constexpr (requires { Opts.validate_trailing_whitespace; }) {
     return Opts.validate_trailing_whitespace;
  } else {
     return false; // Default value (may be different for various options)
  }
}
```

## Default `glz::opts` Options

These are the default options provided by `glz::opts` in `include/glaze/core/opts.hpp`. You can inherit from `glz::opts` to create custom option structs and override these defaults or add new options.

```c++
struct opts
{
   uint32_t format = JSON;
   // The default format for reading/writing (JSON)

   bool null_terminated = true;
   // Whether the input buffer is null terminated
   
   bool comments = false;
   // Support reading in JSONC style comments

   bool error_on_unknown_keys = true;
   // Error when an unknown key is encountered

   bool skip_null_members = true;
   // Skip writing out params in an object if the value is null
   // This applies to nullable types like std::optional, std::shared_ptr, and raw pointers (T*)

   bool prettify = false;
   // Write out prettified JSON

   bool minified = false;
   // Require minified input for JSON, which results in faster read performance

   char indentation_char = ' ';
   // Prettified JSON indentation char

   uint8_t indentation_width = 3;
   // Prettified JSON indentation size

   bool new_lines_in_arrays = true;
   // Whether prettified arrays should have new lines for each element

   bool error_on_missing_keys = false;
   // Require all non nullable keys to be present in the object.
   // Use skip_null_members = false to require nullable members
   // Can be customized per-type using meta<T>::requires_key(key, is_nullable) function

   bool quoted_num = false;
   // Treat numbers as quoted or array-like types as having quoted numbers
   
   bool number = false;
   // Treats all types like std::string as numbers: read/write these quoted numbers
   
   bool raw = false;
   // write out string like values without quotes
   
   bool raw_string = false;
   // Do not decode/encode escaped characters for strings (improves read/write performance)
   
   bool structs_as_arrays = false;
   // Handle structs (reading/writing) without keys, which applies

   bool partial_read = false;
   // Reads into the deepest structural object and then exits without parsing the rest of the input
};
```

## Available Options Outside of `glz::opts`

The supported, but not default provided options are listed after `glz::opts` and include:

```c++
bool validate_skipped = false;
// If full validation should be performed on skipped values

bool bools_as_numbers = false;
// Read and write booleans with 1's and 0's

bool write_member_functions = false;
// Serialize member function pointers referenced in glz::meta instead of skipping them
// JSON/BEVE/TOML writers skip member function pointers by default for safety

bool validate_trailing_whitespace = false;
// If, after parsing a value, we want to validate the trailing whitespace

bool concatenate = true;
// Concatenates ranges of std::pair into single objects when writing

bool allow_conversions = true;
// Whether conversions between convertible types are allowed in binary, e.g. double -> float

bool write_type_info = true;
// Write type info for meta objects in variants

bool shrink_to_fit = false;
// Shrinks dynamic containers to new size to save memory

bool append_arrays = false;
// When reading into an array the data will be appended if the type supports it

bool error_on_const_read = false;
// Error if attempt is made to read into a const value, by default the value is skipped without error

bool hide_non_invocable = true;
// Hides non-invocable members from the cli_menu (may be applied elsewhere in the future)

bool escape_control_characters = false;
// Escapes control characters like 0x01 or null characters with proper unicode escape sequences.
// The default behavior does not escape these characters for performance and safety
// (embedding nulls can cause issues, especially with C APIs)
// Glaze will error when parsing non-escaped control character (per the JSON spec)
// This option allows escaping control characters to avoid such errors.

float_precision float_max_write_precision{};
// The maximum precision type used for writing floats, higher precision floats will be cast down to this precision
```

## CSV Options (`glz::opts_csv`)

For CSV, use `glz::opts_csv` to access CSV-specific options and defaults.

```c++
struct opts_csv {
  uint32_t format = CSV;                 // CSV format selector
  static constexpr bool null_terminated = true;
  uint8_t layout = rowwise;              // rowwise | colwise
  bool use_headers = true;               // write/read with headers for structs
  bool raw_string = false;               // do not escape/unescape string contents
  bool skip_header_row = false;          // skip first row on read
  bool validate_rectangular = false;     // enforce equal column counts on 2D reads
  uint32_t internal{};                   // internal flags
};
```

Notes:
- `layout`: `rowwise` treats each row as a record; `colwise` uses columns (2D arrays are transposed on IO).
- `use_headers`: when `false`, vectors of structs read/write without headers in declaration order.
- `skip_header_row`: skip the first row during read (useful when ingesting headered CSV into headerless targets).
- `validate_rectangular`: for 2D arrays, fail reads when row lengths differ (`constraint_violated`).
- `append_arrays`: appends parsed values to existing containers when supported (add to custom opts struct).
- Use as a template parameter: `glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(...)`.

## See Also

- [Field Validation](field-validation.md) - Customizing required field validation with `requires_key`
