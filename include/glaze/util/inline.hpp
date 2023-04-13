// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(__clang__) || defined(__GNUC__)
#ifndef GLZ_USE_ALWAYS_INLINE
#define GLZ_USE_ALWAYS_INLINE
#endif
#endif

#if defined(GLZ_USE_ALWAYS_INLINE) && defined(NDEBUG)
#ifndef GLZ_ALWAYS_INLINE
#if defined(__clang__)
#define GLZ_ALWAYS_INLINE inline __attribute__((always_inline)) __attribute__((flatten))
#else
#define GLZ_ALWAYS_INLINE inline __attribute__((always_inline))
#endif
#endif
#endif

#ifndef GLZ_ALWAYS_INLINE
#define GLZ_ALWAYS_INLINE inline
#endif

#if defined(__clang__) && defined(NDEBUG)
#ifndef GLZ_FLATTEN
#define GLZ_FLATTEN inline __attribute__((flatten))
#endif

#ifndef GLZ_FLATTEN_NO_INLINE
#define GLZ_FLATTEN_NO_INLINE __attribute__((noinline)) __attribute__((flatten))
#endif
#endif

#ifndef GLZ_FLATTEN
#define GLZ_FLATTEN inline
#endif

#ifndef GLZ_FLATTEN_NO_INLINE
#define GLZ_FLATTEN_NO_INLINE
#endif
