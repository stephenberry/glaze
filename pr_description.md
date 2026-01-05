# Optimization Levels for Binary Size Control

Adds an `optimization_level` option to control the trade-off between binary size and runtime performance, useful for embedded systems.

## New Feature

Two optimization levels:
- `normal` (default) - Maximum performance using large lookup tables
- `size` - Minimized binary size for embedded systems

### Usage

```cpp
// Default (normal) - maximum performance
auto json = glz::write_json(obj);

// Size-optimized for embedded systems
auto json = glz::write<glz::opts_size{}>(obj);
```

## Implementation Details

| Component | Normal Mode | Size Mode |
|-----------|-------------|-----------|
| Integer serialization | `to_chars_40kb` (40KB tables) | `to_chars` (400B tables) |
| Float serialization | dragonbox (~238KB tables) | `std::to_chars` (no tables) |
| Key lookup | Hash tables | Linear search |

Approximate binary size savings with `opts_size`: **~277KB**

## Files Changed

### New Files
- `include/glaze/core/optimization_level.hpp` - `optimization_level` enum definition

### Modified Files
- `include/glaze/core/opts.hpp` - Added `check_optimization_level()`, `is_size_optimized()`, and `opts_size` preset
- `include/glaze/core/write_chars.hpp` - Size-aware integer/float serialization with platform detection for `std::to_chars` float support
- `include/glaze/util/itoa.hpp` - Renamed from `itoa_small.hpp`, contains `to_chars` (400B tables)
- `include/glaze/util/itoa_40kb.hpp` - Renamed from `itoa.hpp`, contains `to_chars_40kb` (40KB tables)

### Documentation
- `docs/optimization-levels.md` - New documentation page
- `docs/options.md` - Added optimization level reference
- `docs/optimizing-performance.md` - Added optimization level section
- `README.md` - Added optimization levels section
- `mkdocs.yml` - Added optimization-levels page to navigation

### Tests
- `tests/int_parsing/int_parsing.cpp` - Updated to test both `to_chars` and `to_chars_40kb` algorithms
- `fuzzing/json_roundtrip_int.cpp` - Updated to fuzz test both algorithms
- `fuzzing/json_exhaustive_roundtrip_int.cpp` - Updated to exhaustively test both algorithms

## Platform Support

Float size optimization (`std::to_chars`) requires platform support:
- Supported: GCC/libstdc++, Clang/libc++ on recent systems, MSVC
- Not supported: Older Apple platforms (iOS < 16.3, macOS < 13.3)

On unsupported platforms, Glaze automatically falls back to dragonbox for floats.
