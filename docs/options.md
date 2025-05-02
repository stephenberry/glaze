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

## Available Options Outside of glz::opts

In `glaze/core/opts.hpp` you can see the default provided fields in `glz::opts`.

The supported, but not default provided options are listed after `glz::opts` and include:

```c++
bool validate_skipped = false;
// If full validation should be performed on skipped values

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

