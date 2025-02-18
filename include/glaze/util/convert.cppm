// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#include <cstdint>
#ifdef CPP_MODULES
export module glaze.util.convert;
#else
#endif




namespace glz
{
   consteval uint16_t to_uint16_t(const char chars[2]) { return uint16_t(chars[0]) | (uint16_t(chars[1]) << 8); }
}
