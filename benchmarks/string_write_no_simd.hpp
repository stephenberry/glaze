#pragma once

#include <string>

#if defined(_WIN32)
#if defined(string_write_no_simd_EXPORTS)
#define NO_SIMD_API __declspec(dllexport)
#else
#define NO_SIMD_API __declspec(dllimport)
#endif
#else
#define NO_SIMD_API
#endif

namespace no_simd
{
   NO_SIMD_API void write_json_string(const std::string& input, std::string& output);
}
