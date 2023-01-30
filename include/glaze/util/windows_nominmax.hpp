// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifdef _WIN32
#ifdef NOMINMAX
#include <windows.h>
#else
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#endif
#endif
