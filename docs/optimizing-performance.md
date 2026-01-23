# Optimizing Performance

Glaze is focused on delivering peak performance through compile time evaluation, reducing dynamic memory allocations, and giving the developer deep control over serialization/deserialization. Simultaneously, Glaze tries to provide a simpler interface by default than many libraries, which makes it easy to get excellent performance with little effort.

## Compiler Flags

Building with optimizations is the most important for performance: `-O2` or `-O3`

Use `-march=native` if you will be running your executable on the build platform.

The CMake option `glaze_DISABLE_SIMD_WHEN_SUPPORTED` can be set to `ON` to disable SIMD optimizations (e.g., AVX2) even when the target supports them. This is useful when cross-compiling for Arm or other architectures. If you aren't using CMake, define the macro `GLZ_DISABLE_SIMD` to disable SIMD optimizations. The macro `GLZ_USE_AVX2` is automatically defined when AVX2 support is detected and SIMD is not disabled.

> [!NOTE]
>
> Glaze uses SIMD Within A Resgister (SWAR) for most optimizations, which are fully cross-platform and does not require any SIMD instrisics or special compiler flags. So, SIMD flags don't usually have a large impact with Glaze.

## Reducing Compilation Time

Glaze aggressively uses forced inlining (`GLZ_ALWAYS_INLINE`) for peak runtime performance. If compilation time is more important than peak performance, you can disable forced inlining:

```cmake
set(glaze_DISABLE_ALWAYS_INLINE ON)
```

Or define `GLZ_DISABLE_ALWAYS_INLINE` before including Glaze headers.

This causes `GLZ_ALWAYS_INLINE` and `GLZ_FLATTEN` to fall back to regular `inline` hints, letting the compiler decide what to inline.

> [!NOTE]
>
> This option primarily reduces **compilation time**, not binary size. Modern compilers typically inline hot paths anyway using their own heuristics, so binary size reduction is often minimal. Forced inlining is automatically disabled in debug builds (when `NDEBUG` is not defined).

## Reducing Binary Size

### Optimization Levels

Glaze provides an `optimization_level` option for controlling the trade-off between binary size and performance. For embedded systems, WebAssembly, or mobile apps where binary size matters:

```cpp
// Use the size-optimized preset
auto ec = glz::read<glz::opts_size{}>(value, buffer);
auto json = glz::write<glz::opts_size{}>(obj);
```

The `size` optimization level:

- Uses `std::to_chars` instead of lookup tables for number serialization
- Defaults to linear search for key matching
- Can reduce binary size by ~280KB or more

See [Optimization Levels](optimization-levels.md) for full details on available levels and their characteristics.

### Linear Search Option

For finer control, you can enable linear search independently:

```cpp
struct small_binary_opts : glz::opts {
   bool linear_search = true;  // Use linear key search instead of hash tables
};

auto ec = glz::read<small_binary_opts{}>(value, buffer);
```

This provides significant binary size reduction (typically 40-50% smaller than default) by:
- Using fold-expression dispatch instead of jump tables
- Eliminating hash table generation for each struct type
- Reducing template instantiation bloat

The trade-off is slightly slower parsing for objects with many fields, though performance remains competitive with other JSON libraries. For objects with few fields (< 10), the difference is negligible.

## Best Data Structures/Layout

It is typically best to match the JSON and C++ object layout. C++ structs should mimic JSON objects, and vice versa.

What you know at compile time should be represented in your C++ data types. So, if you know names at compile time then use a C++ `struct`. If you know an array is a fixed size that can exist on the stack then use a `std::array`. Normal C++ tips for performance apply as Glaze is just an interface layer over your C++ data structures. Prefer `std::unordered_map` over `std::map` for performance if ordering isn't important.

Only use `glz::generic` if you know nothing about the JSON you are ingesting.

> [!NOTE]
>
> Note that structs with more fields often require more complex hash algorithms, so if you can reduce the number of fields in a struct then you'll typically get better hash performance.

## Reducing Memory Allocations

Use the read/write APIs that do not internally allocate the object and buffer. Instead, reuse previous buffers and objects if you are making repeated calls to serialize or parse.

For writing:

```c++
auto ec = glz::write_json(value, buffer); // Prefer this API if calls happen more than once
auto result = glz::write_json(value); // Allocates the buffer within the call
```

For reading:

```c++
auto ec = glz::read_json(value, buffer); // Prefer this API if calls happen more than once
auto result = glz::read_json<Value>(buffer); // Allocates the Value type within the call
```

## Buffers

It is recommended to use a non-const `std::string` as your input and output buffer. When reading, Glaze will automatically pad the `std::string` for more efficient SIMD/SWAR and resize back to the original once parsing is finished.

## Compile Time Options

Glaze provides options that are handled at compile time and directly affect the produced assembly. The default options for Glaze tend to favor performance.

Don't switch these from their defaults unless needed.

- `null_terminated = true`: Using null terminated buffers allows the code to have fewer end checks and therefore branches.
- `error_on_unknown_keys = true`: Erroring on unknown keys allows more efficient hashing, because it can reject all unknown keys and doesn't require additional handling.
- `error_on_missing_keys = false`: Erroring on missing keys requires looking up whether all keys were read from a bitmap, this adds overhead.

## std::string_view Fields

You can use `std::string_view` as your JSON fields if you do not want to copy data from the buffer or allocate new memory.

> [!IMPORTANT]
>
> `std::string_view` will point to the JSON string in your original input buffer. You MUST keep your input buffer alive as long as you are reading from these values.

When you parse into a `std::string_view` Glaze does not unescape JSON strings with escapes (e.g. escaped unicode). Instead the JSON string is left in its escaped form and no allocations are made (the original buffer must be kept alive). You can always read a `std::string_view` into a `std::string` if you want to unescape. Typically users should use a `std::string`, as Glaze very efficiently unescapes JSON and allocates more memory as needed. But, if you need peak performance and know that your buffer will stay alive for as long as you access your `std::string_view`, or want to avoid heap allocations, the option is there to use a `std::string_view`.

## Performance FAQ

> Does the order of members in a struct matter?

No, Glaze uses compile time generated hash maps for C++ structs. Accessing fields out of order has little to no performance effect on Glaze.
