// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

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
#if defined(GLZ_USE_AVX512BW)
   static constexpr const char* glz_cpu_backend{"AVX512BW"};
#elif defined(GLZ_USE_AVX2)
   static constexpr const char* glz_cpu_backend{"AVX2"};
#elif defined(GLZ_USE_SSSE3)
   static constexpr const char* glz_cpu_backend{"SSSE3"};
#elif defined(GLZ_USE_SSE2)
   static constexpr const char* glz_cpu_backend{"SSE2"};
#elif defined(GLZ_USE_NEON64)
   static constexpr const char* glz_cpu_backend{"NEON64"};
#elif defined(GLZ_USE_NEON)
   static constexpr const char* glz_cpu_backend{"NEON"};
#elif defined(GLZ_USE_WASM_SIMD128)
   static constexpr const char* glz_cpu_backend{"WASM_SIMD128"};
#else
   static constexpr const char* glz_cpu_backend{"Scalar"};
#endif
}
