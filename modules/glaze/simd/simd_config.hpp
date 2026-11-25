// Glaze Library
// For the license information refer to glaze.ixx
// Internal textual configuration for SIMD module global fragments.
//
// Feature-selection macros cannot cross a named-module import, while the intrinsic implementations
// must be conditionally parsed before their module declarations. Keep that preprocessor-only layer
// here and expose its C++ representation from glaze.simd.backends.

#pragma once

#if !defined(GLZ_DISABLE_SIMD)
#if defined(__x86_64__) || defined(_M_X64)
#define GLZ_USE_SSE2
// SSSE3 adds _mm_shuffle_epi8 and _mm_alignr_epi8, which byte-granular table lookups need.
// MSVC has no __SSSE3__ macro; anything targeting AVX implies SSSE3 there.
#if defined(__SSSE3__) || (defined(_MSC_VER) && defined(__AVX__))
#define GLZ_USE_SSSE3
#endif
#if defined(__AVX2__)
#define GLZ_USE_AVX2
#endif
// AVX-512BW supplies the byte-granular shuffle and saturating subtract at 512 bits.
//
// The _MSC_VER arm is for MSVC, which reports AVX-512 only as __AVX512F__. clang-cl also defines
// _MSC_VER but does define __AVX512BW__, so it must not take that arm: __AVX512F__ does not imply
// __AVX512BW__, and /clang:-mavx512f would otherwise select _mm512_shuffle_epi8 without the feature
// and fail to compile. The SSSE3 fallback above needs no such guard, because __AVX__ does imply
// __SSSE3__.
//
// Covered by the simd-backends workflow, which runs the UTF-8 suite under Intel SDE.
#if defined(__AVX512BW__) || (defined(_MSC_VER) && !defined(__clang__) && defined(__AVX512F__))
#define GLZ_USE_AVX512BW
#endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON)
#define GLZ_USE_NEON
// vqtbl1q_u8 (a full 16 byte table lookup) is AArch64 only; 32 bit NEON has no equivalent.
#if defined(__aarch64__) || defined(_M_ARM64)
#define GLZ_USE_NEON64
#endif
#elif defined(__wasm_simd128__)
#define GLZ_USE_WASM_SIMD128
#endif
#endif

// The UTF-8 validator additionally needs a byte-granular table lookup. Keep this selection in the
// textual configuration so parse's interface can choose its private SIMD implementation without
// making that implementation part of a BMI.
#if defined(GLZ_USE_AVX512BW) || defined(GLZ_USE_AVX2) || defined(GLZ_USE_SSSE3) || defined(GLZ_USE_NEON64) || \
   defined(GLZ_USE_WASM_SIMD128) || (defined(GLZ_UTF8_GENERIC_WIDTH) && !defined(GLZ_DISABLE_SIMD))
#define GLZ_UTF8_SIMD
#endif
