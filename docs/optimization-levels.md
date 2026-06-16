# Optimization Levels

Glaze provides an `optimization_level` option that controls the trade-off between binary size and runtime performance. This is useful for embedded systems and other size-constrained environments.

## Quick Start

```cpp
// Default (normal) - maximum performance
auto json = glz::write_json(obj);

// Size-optimized for embedded systems
auto json = glz::write<glz::opts_size{}>(obj);
```

## Available Levels

| Level | Preset | Integer Serialization | Float Serialization | Key Lookup |
|-------|--------|----------------------|---------------------|------------|
| `normal` | (default) | `to_chars_40kb` (40KB tables) | zmij (~17KB pow-10 tables) | Hash tables |
| `size` | `opts_size` | `to_chars` (400B tables) | zmij `OptSize=true` (recomputed, ~1.4KB) | Linear search |

## Optimization Level Details

### Normal (Default)

The default optimization level, optimized for maximum runtime performance.

```cpp
// These are equivalent:
glz::write_json(obj, buffer);
glz::write<glz::opts{}>(obj, buffer);
```

**Characteristics:**

- Uses 40KB digit lookup tables for fast integer-to-string conversion
- Uses zmij with ~17KB pow-10 lookup tables for float-to-string
- Uses compile-time generated hash tables for O(1) key lookup
- Best throughput for high-volume JSON processing

### Size

Minimizes binary size for embedded systems.

```cpp
glz::write<glz::opts_size{}>(obj, buffer);
glz::read<glz::opts_size{}>(obj, buffer);
```

**Characteristics:**

- Uses `glz::to_chars` for integers (400B lookup tables, 1.6x faster than `std::to_chars`)
- Uses `glz::to_chars<T, OptSize=true>` for floats (zmij with the pow-10 tables dropped; recomputed on the fly)
- Defaults to linear search for key matching (no hash tables)
- Significantly smaller binary size

**Approximate Binary Size Savings:**

- ~39KB from using smaller integer lookup tables (400B vs 40KB)
- ~15.8KB from dropping zmij's pow-10 tables (`OptSize=true`)
- Variable savings from hash table elimination

## Build-wide Default (Embedded)

Passing `glz::opts_size{}` at every call site works, but a single forgotten call left on the default options (for example `glz::write_json(obj, buffer)` or a transitive `glz::format_error`) silently relinks the large tables. For flash-constrained targets you usually want `size` to be the default *everywhere* instead of opt-in per call.

Enable the CMake option:

```cmake
set(glaze_DEFAULT_OPTIMIZATION_SIZE ON)
# or: cmake -Dglaze_DEFAULT_OPTIMIZATION_SIZE=ON ...
```

This injects `GLZ_DEFAULT_OPTIMIZATION_SIZE` as an `INTERFACE` compile definition, which flips the default optimization level for any options type that does not specify one (such as a plain `glz::opts{}`). Integers, floats, and per-struct hash-table elimination all switch together, reusing the same machinery as `opts_size`.

Without CMake, define the macro directly before including Glaze (it must be defined uniformly across the whole build):

```cpp
#define GLZ_DEFAULT_OPTIMIZATION_SIZE
#include "glaze/glaze.hpp"
```

Key properties:

- **Per-call options still win.** An explicit `optimization_level::normal` keeps the fast tables even in a size-default build, so you can opt specific hot paths back into speed. Because the only way back to the large tables is an explicit, greppable `normal`, accidental inclusion is eliminated.
- **The guarantee is verifiable.** With the size default and no explicit `normal`, nothing ODR-uses `glz::to_chars_40kb`, so the 40 KB `digit_quads` table is never emitted, even without `-Wl,--gc-sections`. You can confirm with `nm -C <binary> | grep digit_quads` (expect no output).
- **Advanced override.** Define `GLZ_DEFAULT_OPTIMIZATION_LEVEL` directly to any `glz::optimization_level` value if you need finer control than the boolean.

## Custom Options Struct

You can create a custom options struct with the optimization level:

```cpp
struct my_opts : glz::opts
{
   glz::optimization_level optimization_level = glz::optimization_level::size;
   // Add other options as needed
   bool prettify = true;
};

glz::write<my_opts{}>(obj, buffer);
```

## Combining with Other Options

The optimization level works with all other Glaze options:

```cpp
struct embedded_opts : glz::opts
{
   glz::optimization_level optimization_level = glz::optimization_level::size;
   bool minified = true;  // Also skip whitespace parsing for smaller code
   bool error_on_unknown_keys = false;  // More lenient parsing
};
```

## Platform Considerations

Both optimization levels use `glz::to_chars` (zmij) for floats. The only
difference is whether the pow-10 tables are linked in (`normal`) or
recomputed on the fly (`size`). Both variants work on all platforms,
including bare-metal and older Apple targets.

### When to Use Each Level

| Use Case | Recommended Level |
|----------|-------------------|
| Server-side JSON processing | `normal` |
| Desktop applications | `normal` |
| Embedded systems | `size` |

## See Also

- [Compile Time Options](options.md) - All available options
- [Optimizing Performance](optimizing-performance.md) - General performance tips
- [linear_search option](options.md#linear_search) - Binary size reduction via linear key search
