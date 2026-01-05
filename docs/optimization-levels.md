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
| `normal` | (default) | `to_chars_40kb` (40KB tables) | dragonbox (~238KB) | Hash tables |
| `size` | `opts_size` | `to_chars` (400B tables) | `std::to_chars` (no tables) | Linear search |

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
- Uses dragonbox algorithm with ~238KB tables for float-to-string
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
- Uses `std::to_chars` for floats (no dragonbox tables)
- Defaults to linear search for key matching (no hash tables)
- Significantly smaller binary size

**Approximate Binary Size Savings:**

- ~39KB from using smaller integer lookup tables (400B vs 40KB)
- ~238KB from avoiding dragonbox float tables
- Variable savings from hash table elimination

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

### Floating-Point Support

The size optimization for floats (`std::to_chars`) requires platform support:

- **Supported:** Most modern platforms (GCC/libstdc++, Clang/libc++ on recent systems, MSVC)
- **Not supported:** Older Apple platforms (iOS < 16.3, macOS < 13.3)

On unsupported platforms, Glaze automatically falls back to dragonbox for floats. Integer serialization uses `glz::to_chars` which works on all platforms.

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
