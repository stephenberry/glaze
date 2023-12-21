// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/inline.hpp"

#if defined(__CUDA_ARCH__)
   /* CUDA compiler */
   #define GLAZE_INTRINSIC __forceinline__ __device__
#elif defined(__OPENCL_VERSION__)
   /* OpenCL compiler */
   #define GLAZE_INTRINSIC inline static
#elif defined(__INTEL_COMPILER)
   /* Intel compiler, even on Windows */
   #define GLAZE_INTRINSIC inline static __attribute__((__always_inline__))
#elif defined(__GNUC__)
   /* GCC-compatible compiler (gcc/clang/icc) */
   #define GLAZE_INTRINSIC inline static __attribute__((__always_inline__))
#elif defined(_MSC_VER)
   /* MSVC-compatible compiler (cl/icl/clang-cl) */
   #define GLAZE_INTRINSIC __forceinline static
#else defined(__cplusplus)
   /* Generic C++ compiler */
   #define GLAZE_INTRINSIC inline static
#endif

#if defined(__GNUC__) || defined(__clang__)
   #if defined(__ARM_NEON__) || defined(__ARM_NEON)
      #include <arm_neon.h>
   #endif

   #if defined(__SSE2__)
      #include <emmintrin.h>
   #endif

   #if defined(__SSE3__)
      #include <pmmintrin.h>
   #endif

   #if defined(__SSSE3__)
      #include <tmmintrin.h>
   #endif

   #if defined(__SSE4_1__)
      #include <smmintrin.h>
   #endif

   #if defined(__SSE4_2__)
      #include <nmmintrin.h>
   #endif

   #if defined(__AVX__)
      #include <immintrin.h>
   #endif
#endif

#include <cstddef>
#include <cstdint>

namespace glz
{
#if defined(__GNUC__) || defined(__clang__)
#define GLAZE_SIMD 1
   typedef uint64_t simd_u64_128 __attribute__((vector_size(16), aligned(8)));
   
   GLAZE_INTRINSIC simd_u64_128 splat_u64(uint64_t c) {
      return { c, c };
   }
#endif
}
