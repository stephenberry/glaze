// Glaze Library
// For the license information refer to glaze.hpp
// Textual dependencies for the SIMD UTF-8 validator.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "glaze/simd/simd_config.hpp"

#if !defined(GLZ_UTF8_GENERIC_WIDTH) && \
   (defined(GLZ_USE_AVX512BW) || defined(GLZ_USE_AVX2) || defined(GLZ_USE_SSSE3))
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif
#elif !defined(GLZ_UTF8_GENERIC_WIDTH) && defined(GLZ_USE_NEON64)
#include <arm_neon.h>
#elif !defined(GLZ_UTF8_GENERIC_WIDTH) && defined(GLZ_USE_WASM_SIMD128)
#include <wasm_simd128.h>
#endif

#include "glaze/util/inline.hpp"
