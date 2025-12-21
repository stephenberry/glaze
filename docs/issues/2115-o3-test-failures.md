# Issue #2115: Test Failures with -O3 Optimization

**Issue:** https://github.com/stephenberry/glaze/issues/2115
**Reporter:** Arniiiii
**Environment:** Gentoo Hardened, GCC 15.2.1_p20251122 p3, x86_64

## Problem Description

When compiling Glaze v6.1.0 with `-O3` on Gentoo Hardened, specific tests fail:

**json_test failures:**
- "mod hash" (lines 10378-10380)
- "requires_key works with nullable types" (line 11181)

**json_reflection_test failures:**
- "full_hash" (line 1158)
- "front_32" (line 1167)
- "front_64" (line 1180)

All failures are related to hash-based key lookup during JSON parsing.

## Reporter's Environment

```
gcc version 15.2.1 20251122 (Gentoo Hardened 15.2.1_p20251122 p3)
Target: x86_64-pc-linux-gnu
```

Key GCC configure options:
- `--enable-default-pie`
- `--enable-default-ssp`
- `--enable-cet`
- `--enable-host-pie`
- `--enable-host-bind-now`
- Built with: `bootstrap-O3 bootstrap-lto bootstrap-cet`

## Testing Matrix

### Round 1: Basic Hardened Flags

| Configuration | Flags | Result |
|--------------|-------|--------|
| O2-baseline | `-O2` | ✅ PASS |
| O3-basic | `-O3` | ✅ PASS |
| O3-cf-protection | `-O3 -fcf-protection=full` | ✅ PASS |
| O3-hardened-basic | `-O3 -fcf-protection=full -fstack-protector-strong -D_FORTIFY_SOURCE=2` | ✅ PASS |
| O3-no-strict-aliasing | `-O3 -fno-strict-aliasing` | ✅ PASS |

### Round 2: Extended Gentoo-Specific Flags

| Configuration | Flags | Result |
|--------------|-------|--------|
| O3-stack-clash | `-O3 -fstack-clash-protection` | ✅ PASS |
| O3-fortify3 | `-O3 -D_FORTIFY_SOURCE=3` | ✅ PASS |
| O3-pie | `-O3 -fPIE` (+ `-pie` linker flag) | ✅ PASS |
| O3-no-plt | `-O3 -fno-plt` | ✅ PASS |
| O3-no-semantic-interposition | `-O3 -fno-semantic-interposition` | ✅ PASS |
| O3-retpoline | `-O3 -mindirect-branch=thunk -mfunction-return=thunk` | ✅ PASS |
| O3-sls-hardening | `-O3 -mharden-sls=all` | ✅ PASS |
| O3-auto-var-init | `-O3 -ftrivial-auto-var-init=zero` | ✅ PASS |

### Round 2b: Full Gentoo Hardened Profile Combinations

Note: `-fcf-protection` (CET) and `-mindirect-branch`/`-mfunction-return` (retpoline) are **mutually exclusive** - they are different approaches to the same security goal.

| Configuration | Flags | Result |
|--------------|-------|--------|
| O3-full-gentoo-hardened-cet | `-O3 -fcf-protection=full -fstack-protector-strong -fstack-clash-protection -D_FORTIFY_SOURCE=3 -fPIE -fno-plt -fno-semantic-interposition` | ✅ PASS |
| O3-full-gentoo-hardened-retpoline | `-O3 -mindirect-branch=thunk -mfunction-return=thunk -fstack-protector-strong -fstack-clash-protection -D_FORTIFY_SOURCE=3 -fPIE -fno-plt -fno-semantic-interposition` | ✅ PASS |

### Round 3: LTO Combinations

| Configuration | Flags | Result |
|--------------|-------|--------|
| O3-lto | `-O3 -flto=auto` | ✅ PASS |
| O3-lto-hardened | `-O3 -flto=auto -fPIE -fcf-protection=full -fstack-protector-strong -fstack-clash-protection -D_FORTIFY_SOURCE=3` | ✅ PASS |

## Diagnostic Test

A diagnostic test program outputs:
- Hash type selection for each test struct
- Key ordering in reflection
- Parsing results with detailed values

## Files

- Workflow: `.github/workflows/gcc-hardened-o3.yml`
- This tracking doc: `docs/issues/2115-o3-test-failures.md`

## Notes

- Issue cannot be reproduced on macOS (ARM) with GCC 15.1 or Clang
- Issue cannot be reproduced with standard GCC 15 Docker image + any tested flag combination
- **All 17 flag configurations tested pass on upstream GCC 15**
- Gentoo Hardened applies custom patches to GCC (`p3` suffix)
- Reporter offered to provide Docker image if needed

## Potential Causes (Narrowed Down)

Since all standard hardened flags pass on upstream GCC 15, the issue is likely caused by:

1. **Gentoo-specific GCC patches** - The `p3` suffix indicates Gentoo-specific patches not in upstream GCC. This is the most likely cause.
2. **Custom CFLAGS/CXXFLAGS** - Reporter may have additional flags in `/etc/portage/make.conf` not covered by our tests
3. **Different glibc** - Gentoo Hardened may use a patched glibc
4. **Specific CPU features** - Hardware-specific code generation differences

## Next Steps

1. ~~Test basic hardened flags~~ ✅ ALL PASS
2. ~~Test extended Gentoo-specific flags~~ ✅ ALL PASS
3. ~~Test full Gentoo hardened profile~~ ✅ ALL PASS
4. ~~Test LTO combinations~~ ✅ ALL PASS
5. **Request reporter's exact CFLAGS/CXXFLAGS from `emerge --info`**
6. **Request Gentoo Hardened Docker image from reporter** - This is likely needed to reproduce
