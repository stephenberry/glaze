// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>

#if !defined(GLZ_DISABLE_SIMD)
#if defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif
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
// MSVC has no __AVX512BW__ macro; /arch:AVX512 defines __AVX512F__ alongside it.
// Covered by the simd-backends workflow, which runs the UTF-8 suite under Intel SDE.
#if defined(__AVX512BW__) || (defined(_MSC_VER) && defined(__AVX512F__))
#define GLZ_USE_AVX512BW
#endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON)
#include <arm_neon.h>
#define GLZ_USE_NEON
// vqtbl1q_u8 (a full 16 byte table lookup) is AArch64 only; 32 bit NEON has no equivalent.
#if defined(__aarch64__) || defined(_M_ARM64)
#define GLZ_USE_NEON64
#endif
#elif defined(__wasm_simd128__)
#include <wasm_simd128.h>
#define GLZ_USE_WASM_SIMD128
#endif
#endif

namespace glz
{
   // The widest instruction set the detection above enabled for this translation unit.
   //
   // Glaze has no runtime dispatch: every SIMD path is chosen in the preprocessor, so this is fixed
   // for the binary and reports what it was compiled for rather than what the host can execute. A
   // build that reports "AVX2" runs AVX2 on a machine that supports AVX-512, and crashes on one
   // that supports neither.
   //
   // This is the detection result, which is an upper bound rather than the name of a single code
   // path. Subsystems consume the GLZ_USE_* macros independently and do not all reach the same
   // level: JSON string escaping has no AVX-512 helper, so an AVX-512 build reports "AVX512BW" here
   // while escaping runs the AVX2 and SSE2 ones. Use `glz::utf8_validation_backend`, declared in
   // simd/utf8_validation.hpp, for what the UTF-8 validator itself selected -- it names the
   // width-generic backend, which this constant cannot see.
   //
   // These spellings are public API. Renaming one breaks every equality comparison against it, and
   // inserting a wider entry changes what an already-working build reports, so treat both as
   // deliberate rather than incidental.
#if defined(GLZ_USE_AVX512BW)
   inline constexpr std::string_view simd_isa = "AVX512BW";
#elif defined(GLZ_USE_AVX2)
   inline constexpr std::string_view simd_isa = "AVX2";
#elif defined(GLZ_USE_SSSE3)
   inline constexpr std::string_view simd_isa = "SSSE3";
#elif defined(GLZ_USE_SSE2)
   inline constexpr std::string_view simd_isa = "SSE2";
#elif defined(GLZ_USE_NEON64)
   inline constexpr std::string_view simd_isa = "NEON64";
#elif defined(GLZ_USE_NEON)
   inline constexpr std::string_view simd_isa = "NEON";
#elif defined(GLZ_USE_WASM_SIMD128)
   inline constexpr std::string_view simd_isa = "WASM_SIMD128";
#else
   // No vector path: either GLZ_DISABLE_SIMD, or a target the detection above does not cover.
   // Glaze still uses SWAR everywhere, which needs no intrinsics, so this is not "no acceleration".
   inline constexpr std::string_view simd_isa = "scalar";
#endif
}
