# Optimizing Performance

Glaze is focused on delivering peak performance through compile time evaluation, reducing dynamic memory allocations, and giving the developer deep control over serialization/deserialization. Simultaneously, Glaze tries to provide a simpler interface by default than many libraries, which makes it easy to get excellent performance with little effort.

## Compiler Flags

Building with optimizations is the most important for performance: `-O2` or `-O3`

Use `-march=native` if you will be running your executable on the build platform.

### SIMD Architecture Flags

Glaze automatically detects the target architecture using compiler-predefined macros and defines the appropriate SIMD flags:

All of the x86 flags below are additionally conditional on the x86-64 branch (`__x86_64__` or `_M_X64`), and every flag is suppressed by `GLZ_DISABLE_SIMD`. A 32-bit x86 build defines none of them even with `-mavx2`.

| Flag | Detected When | Architecture |
|------|--------------|--------------|
| `GLZ_USE_SSE2` | always, on x86-64 | x86-64 (always has SSE2) |
| `GLZ_USE_SSSE3` | `__SSSE3__`, or `_MSC_VER` with `__AVX__` | x86-64 with byte-granular shuffle |
| `GLZ_USE_AVX2` | `__AVX2__` | x86-64 with AVX2 |
| `GLZ_USE_AVX512BW` | `__AVX512BW__`, or `_MSC_VER` with `__AVX512F__` | x86-64 with AVX-512BW |
| `GLZ_USE_NEON` | `__aarch64__`, `_M_ARM64`, or `__ARM_NEON` | ARM with NEON |
| `GLZ_USE_NEON64` | `__aarch64__` or `_M_ARM64` | AArch64 (has the full 16-byte table lookup) |
| `GLZ_USE_WASM_SIMD128` | `__wasm_simd128__` | WebAssembly |

These macros are set by the compiler based on the target architecture, so they work correctly when cross-compiling (e.g., an x86 host building for ARM will not define `__x86_64__`).

The flags are cumulative rather than exclusive. When AVX2 is available, `GLZ_USE_SSE2` and `GLZ_USE_AVX2` are both defined, and string escaping uses them together: the AVX2 path handles 32-byte chunks, then the SSE2 path handles the 16-byte remainder.

### Querying the Selected Backend

`glz::simd_info` reports which SIMD path each accelerated subsystem compiled to. It is reflectable, so a benchmark harness can emit the whole thing:

```c++
std::string report;
std::ignore = glz::write_json(glz::simd_info, report);
// {"detected":"AVX512BW","utf8_validation":"AVX512BW","string_escape":"AVX2","float_write":"SSE4.1"}
```

Each field is a `std::string_view`, so they compare by value and work in `constexpr` contexts.

| Field | Meaning | Values |
|---|---|---|
| `detected` | Widest instruction set the flags above enabled | `AVX512BW`, `AVX2`, `SSSE3`, `SSE2`, `NEON64`, `NEON`, `WASM_SIMD128`, `scalar` |
| `utf8_validation` | UTF-8 validator | `AVX512BW`, `AVX2`, `SSSE3`, `NEON64`, `WASM_SIMD128`, `scalar` |
| `string_escape` | Widest JSON string-escape helper | `AVX2`, `SSE2`, `NEON`, `SWAR` |
| `float_write` | Float serialization, via the zmij writer | `NEON`, `SSE4.1`, `SSE2`, `scalar` |

`SWAR` means SIMD-within-a-register: eight bytes at a time packed into a `uint64_t`, needing no intrinsics. It is Glaze's fallback everywhere, so `scalar` in the other fields does not mean the work is done a byte at a time.

`float_write` describes the default float path only. Setting the `float_format` option routes floats through `std::format` instead, which no field reports.

> [!IMPORTANT]
>
> **The fields disagree with each other, which is why this is a struct rather than one name.** `detected` is an upper bound, and reporting it alone would misdescribe most builds. For example:
>
> - **String escaping** has no AVX-512 helper and no WASM helper, so an AVX-512 build escapes with AVX2 and a WASM build escapes with SWAR.
> - **UTF-8 validation** needs a byte-granular shuffle, which plain SSE2 and 32-bit NEON lack. Those targets validate with the scalar validator while the rest of Glaze stays vectorized.
> - **Float writing** runs its own detection off `__SSE2__` / `__ARM_NEON` rather than Glaze's `GLZ_USE_*` macros, honouring only `GLZ_DISABLE_SIMD`. `detected` does not even bound it from above: a 32-bit x86 build with SSE2 reports `detected == "scalar"` and `float_write == "SSE2"`.
>
> This list is illustrative, not exhaustive — check the field you care about rather than inferring it from `detected`.

Selection happens entirely in the preprocessor, with no runtime dispatch, so this describes the *translation unit* rather than the machine running it. A build reporting `"AVX2"` runs AVX2 on a host that also supports AVX-512, and crashes on one that supports neither.

`glz::simd_info` has internal linkage, so each translation unit gets its own copy holding its own answer. A project compiling some files with `-mavx2` and others without gets accurate values in both, and the one you read is the one for the file you read it from.

`utf8_validation` has three further values — `generic16`, `generic32`, and `generic64` — which mean `GLZ_UTF8_GENERIC_WIDTH` selected a portable width-generic validator written in plain C++. That is a testing hook for exercising the algorithm at register sizes the host cannot execute; no ordinary build selects it.

For compile-time *branching*, prefer the `GLZ_USE_*` macros above. A `static_assert` on a string compares equal only to the exact spelling, so a typo produces a permanently satisfied assertion rather than an error.

### Disabling SIMD

To disable all SIMD intrinsics, use the CMake option:

```cmake
set(glaze_DISABLE_SIMD_WHEN_SUPPORTED ON)
```

This sets the `GLZ_DISABLE_SIMD` compile definition as an INTERFACE property, so it automatically propagates to all targets that link against `glaze::glaze`. If you aren't using CMake, define `GLZ_DISABLE_SIMD` before including Glaze headers.

> [!NOTE]
>
> Glaze uses SIMD Within A Register (SWAR) for most optimizations, which are fully cross-platform and do not require any SIMD intrinsics or special compiler flags. SIMD intrinsics are used for specific hot paths (e.g., JSON string escaping), so disabling them typically has a modest impact on overall performance.

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

- Uses smaller integer lookup tables and drops the float pow-10 tables (recomputed on the fly)
- Defaults to linear search for key matching
- Can reduce binary size by ~55KB or more

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

### Reusing Context Across Calls

Glaze uses a `glz::context` object internally during parsing and serialization. This context contains a scratch buffer that is used as temporary storage in specific cases — such as parsing object keys that require unescaping, reading `quoted` wrapped values, `escape_bytes` decoding, and filesystem path deserialization. Most parsing and serialization does **not** use the scratch buffer, so for simple structs with plain string or numeric fields, reusing a context provides little benefit. By default, a fresh context is created for each call:

```c++
// Each call creates and destroys its own context (and scratch buffer)
for (auto& item : items) {
   auto ec = glz::read_json(item, buffers[i]);
}
```

For hot loops, you can create a context once and pass it to every call. The scratch buffer grows as needed and is reused across calls, avoiding repeated allocations:

```c++
glz::context ctx{};
for (auto& item : items) {
   auto ec = glz::read<glz::opts{}>(item, buffers[i], ctx);
}
```

This is most beneficial when parsing data that exercises the scratch buffer (e.g., objects with escaped keys, `quoted` fields, or filesystem paths) in a tight loop.

> [!NOTE]
>
> The context stores error state, so if you reuse a context you should check for errors after each call. The error state is overwritten by each subsequent call.

Custom contexts that inherit from `glz::context` also benefit from scratch buffer reuse:

```c++
struct my_context : glz::context {
   size_t max_string_length = 1024;
};

my_context ctx{};
for (auto& msg : messages) {
   auto ec = glz::read<glz::opts{}>(msg.value, msg.buffer, ctx);
   if (ec) { /* handle error */ }
}
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
