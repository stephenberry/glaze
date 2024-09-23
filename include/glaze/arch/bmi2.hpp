// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include "glaze/util/inline.hpp"

#ifdef GLZ_HAS_BMI2
#include <immintrin.h>

namespace glz
{
   GLZ_ALWAYS_INLINE uint64_t pext64(const uint64_t src, const uint64_t mask) noexcept {
       return _pext_u64(src, mask);
   }
}

#endif
