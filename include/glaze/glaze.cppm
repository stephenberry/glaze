module;

#define GLAZE_CXX_MODULE 1

// Third-party headers
#include "glaze/api/xxh64.hpp"

#ifdef GLAZE_API_ON_WINDOWS
#ifdef NOMINMAX
#include <windows.h>
#else
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#endif
#define SHARED_LIBRARY_EXTENSION ".dll"
#define SHARED_LIBRARY_PREFIX ""
#elif __APPLE__
#include <dlfcn.h>
#define SHARED_LIBRARY_EXTENSION ".dylib"
#define SHARED_LIBRARY_PREFIX "lib"
#elif __has_include(<dlfcn.h>)
#include <dlfcn.h>
#define SHARED_LIBRARY_EXTENSION ".so"
#define SHARED_LIBRARY_PREFIX "lib"
#endif

export module glaze;

export import std;

#include "glaze/glaze.hpp"
