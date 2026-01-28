#pragma once

#include <cstdint>

#if defined(__clang__) || (defined(__GNUC__) && defined(__llvm__)) || (defined(__APPLE__) && defined(__clang__))
#define BENCH_CLANG 1
#elif defined(_MSC_VER)
#define BENCH_MSVC 1
#pragma warning(disable : 4820)
#pragma warning(disable : 4371)
#pragma warning(disable : 4710)
#pragma warning(disable : 4711)
#elif defined(__GNUC__) && !defined(__clang__)
#define BENCH_GNUCXX 1
#endif

#if defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))
#define BENCH_MAC 1
#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
#define BENCH_LINUX 1
#elif defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#define BENCH_WIN 1
#endif

#if defined(BENCH_MSVC)
#define BENCH_NO_INLINE __declspec(noinline)
#define BENCH_FLATTEN [[msvc::flatten]] inline
#define BENCH_ALWAYS_INLINE [[msvc::forceinline]] inline
#elif defined(BENCH_CLANG)
#define BENCH_NO_INLINE __attribute__((__noinline__))
#define BENCH_FLATTEN inline __attribute__((flatten))
#define BENCH_ALWAYS_INLINE inline __attribute__((always_inline))
#elif defined(BENCH_GNUCXX)
#define BENCH_NO_INLINE __attribute__((noinline))
#define BENCH_FLATTEN inline __attribute__((flatten))
#define BENCH_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define BENCH_FLATTEN inline
#define BENCH_NO_INLINE
#define BENCH_ALWAYS_INLINE inline
#endif
