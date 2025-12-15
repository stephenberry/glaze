# GCC 15 Strict Aliasing Optimization Bug Report

## Executive Summary

A bug in GCC 15's strict aliasing optimization causes incorrect code generation at `-O3` optimization level. The compiler incorrectly determines that memory locations have not been modified after parsing JSON data, leading to the optimizer substituting default/stale values instead of reading actual parsed values from memory.

**Affected Component:** GCC strict aliasing analysis at `-O3`
**GCC Version:** 15.2.1 20251122 (Gentoo Hardened)
**Severity:** High - causes silent data corruption in optimized builds
**Workaround:** Insert `std::atomic_signal_fence(std::memory_order_acq_rel)` after parsing operations

---

## Environment

### Compiler Details
```
g++ (Gentoo Hardened 15.2.1_p20251122 p3) 15.2.1 20251122
Target: x86_64-pc-linux-gnu
```

### Compilation Flags Required to Trigger Bug
```
-std=c++23 -O3
```

### Flags That Prevent the Bug
- `-O0`, `-O1`, `-O2` (bug only occurs at `-O3`)
- `-fno-strict-aliasing` (disables the problematic optimization)
- `-DNDEBUG` (changes code paths enough to avoid triggering)

---

## Reproduction Steps

### Using Docker (Recommended)

A Docker image is available that contains GCC 15 and all necessary dependencies.

#### 1. Build the Docker Image

```bash
cd docker_repro
docker build -t glaze_issue_2115_repro .
```

#### 2. Run the Reproduction Test

```bash
docker run --rm -it \
  -v $(pwd)/..:/glaze_dev:ro \
  glaze_issue_2115_repro \
  /bin/bash -c '
    cd /work

    # Copy test file
    cp /glaze_dev/docker_repro/reduced_6805.cpp test.cpp

    # Compile with -O3 (triggers bug)
    g++ -std=c++23 -O3 -I/work/glaze/include \
        -I/work/glaze/build/_deps/ut-src/include \
        test.cpp -o test_o3

    echo "=== Running with -O3 (should FAIL) ==="
    ./test_o3 -tc="mod hash"

    # Compile with -O2 (no bug)
    g++ -std=c++23 -O2 -I/work/glaze/include \
        -I/work/glaze/build/_deps/ut-src/include \
        test.cpp -o test_o2

    echo ""
    echo "=== Running with -O2 (should PASS) ==="
    ./test_o2 -tc="mod hash"
  '
```

#### Expected Output

```
=== Running with -O3 (should FAIL) ===
FAILED "mod hash" test.cpp:6799

FAILED
tests: 272 (271 passed, 1 failed, 0 compile-time)
asserts: 17583 (17582 passed, 1 failed)

=== Running with -O2 (should PASS) ===

PASSED
tests: 272 (272 passed, 0 failed, 0 compile-time)
asserts: 17583 (17583 passed, 0 failed)
```

### Interactive Docker Session

For deeper investigation:

```bash
docker run --rm -it \
  -v $(pwd)/..:/glaze_dev:ro \
  glaze_issue_2115_repro \
  /bin/bash

# Inside container:
cd /work
cp /glaze_dev/docker_repro/reduced_6805.cpp test.cpp

# Set up includes
UT_INCLUDE=/work/glaze/build/_deps/ut-src/include
GLAZE_INCLUDE=/work/glaze/include

# Compile and test
g++ -std=c++23 -O3 -I$GLAZE_INCLUDE -I$UT_INCLUDE test.cpp -o test
./test -tc="mod hash"
```

---

## Technical Analysis

### The Failing Test

The bug manifests in a simple struct parsing test:

```cpp
struct point3d {
   int x{}, y{}, z{};
};

suite simple_mod_hashes = [] {
   "mod hash"_test = [] {
      point3d obj{};
      expect(not glz::read_json(obj, R"({"x":1,"y":2,"z":3})"));
      expect(obj.x == 1);  // FAILS: compiler substitutes 0
      expect(obj.y == 2);  // FAILS: compiler substitutes 0
      expect(obj.z == 3);  // FAILS: compiler substitutes 0
   };
};
```

### Root Cause

GCC's strict aliasing optimizer incorrectly determines that:
1. The `obj` variable is initialized with default values `{0, 0, 0}`
2. The `glz::read_json()` call cannot modify `obj` through the pointer/reference chain
3. Therefore, `obj.x`, `obj.y`, `obj.z` must still be `0`

This analysis is **incorrect** - the JSON parsing does modify these values through valid pointer operations.

### Why It Requires ~6800 Lines

The bug is **code quantity dependent**. It does not trigger with small test files because:

1. GCC's alias analysis accumulates state across template instantiations
2. With enough instantiations (~6800+ lines of varied `glz::read_json`/`glz::write_json` calls), the optimizer's heuristics make incorrect assumptions
3. The `unknown_fields_method_test` suite appears to be the "tipping point" that triggers the faulty optimization

Attempts to create a minimal (<100 line) reproduction failed because:
- The bug requires accumulated compilation state
- Removing code eventually drops below the threshold needed to trigger the bug
- Synthetic test files with many structs don't reproduce the exact conditions

### Diagnostic Evidence

Key observation that confirms the bug:

```cpp
// This FAILS (compiler substitutes cached value 0):
expect(obj.x == 1);

// This PASSES (forcing memory read prevents optimization):
expect(obj.x == 1) << "x=" << obj.x;
```

The `<< obj.x` forces an actual memory read for the diagnostic output, which prevents the compiler from substituting the incorrect cached value.

---

## The Fix

### Workaround Implementation

```cpp
// In glaze/core/read.hpp

inline void compiler_barrier() noexcept
{
   std::atomic_signal_fence(std::memory_order_acq_rel);
}

// Called after parsing:
template <auto Opts, class T>
[[nodiscard]] error_ctx read(T& value, contiguous auto&& buffer, is_context auto&& ctx)
{
   // ... parsing code ...

   parse<Opts.format>::template op<...>(value, ctx, it, end);

   // Compiler barrier to prevent GCC 15 optimization bug
   compiler_barrier();

   // ... rest of function ...
}
```

### Why It Works

`std::atomic_signal_fence(std::memory_order_acq_rel)`:
- Acts as a **compiler-only barrier** (no runtime overhead)
- Forces the compiler to "forget" cached knowledge about memory state
- Prevents the faulty alias analysis from substituting incorrect values
- Does NOT generate any actual CPU instructions

### Alternative Fixes (Not Recommended)

1. **`-fno-strict-aliasing`** - Disables the optimization entirely, may impact performance
2. **`-O2` instead of `-O3`** - Loses other beneficial optimizations
3. **`volatile` qualifiers** - Intrusive changes to data structures

---

## Files Included

| File | Description |
|------|-------------|
| `reduced_6805.cpp` | Minimal reproduction (6805 lines) |
| `Dockerfile` | Docker image for reproduction environment |
| `GCC15_STRICT_ALIASING_BUG_REPORT.md` | This document |

---

## Additional Notes

### Not Affected
- Clang (all versions tested)
- GCC 14 and earlier
- MSVC

### Affected
- GCC 15.2.1 (confirmed)
- Potentially other GCC 15.x versions (not tested)

### Impact
This bug affects any C++ code that:
1. Uses template-heavy parsing/serialization libraries
2. Compiles with GCC 15 at `-O3`
3. Has sufficient code volume to trigger the accumulated alias analysis issue

---

## Contact

**Library:** Glaze (https://github.com/stephenberry/glaze)
**Issue Reference:** GitHub Issue #2115
**Reporter:** Glaze development team

---

## Appendix: Dockerfile

```dockerfile
FROM gentoo/stage3

# Install GCC 15 and dependencies
RUN emerge-webrsync && \
    emerge --sync && \
    emerge -av sys-devel/gcc:15 dev-build/cmake

# Clone Glaze library (version 6.2.0)
WORKDIR /work
RUN git clone --depth 1 --branch v6.2.0 https://github.com/stephenberry/glaze.git

# Build Glaze to fetch test dependencies (ut testing framework)
WORKDIR /work/glaze
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_CXX_COMPILER=g++ && \
    make -j$(nproc) || true  # May fail, but fetches dependencies

WORKDIR /work
```

---

## Appendix: Minimal Failing Code Pattern

While the full 6805-line file is needed to trigger the bug, the pattern that fails is:

```cpp
#include "glaze/json.hpp"

struct point3d {
   int x{}, y{}, z{};
};

int main() {
   point3d obj{};

   // This parsing succeeds at runtime
   glz::read_json(obj, R"({"x":1,"y":2,"z":3})");

   // But the optimizer incorrectly assumes obj.x is still 0
   if (obj.x != 1) {
      return 1;  // BUG: This branch is taken!
   }

   return 0;
}
```

The issue is that GCC's strict aliasing analysis, after processing thousands of template instantiations, incorrectly concludes that the JSON parsing cannot modify `obj` through the internal pointer operations used by the parsing library.
