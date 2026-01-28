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
#if defined(__AVX2__)
#define GLZ_USE_AVX2
#endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON)
#include <arm_neon.h>
#define GLZ_USE_NEON
#endif
#endif
