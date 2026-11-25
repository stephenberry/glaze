// Glaze Library
// For the license information refer to glaze.ixx

#pragma once

#ifndef __has_cpp_attribute
#define GLZ_LIFETIMEBOUND
#elif __has_cpp_attribute(msvc::lifetimebound)
#define GLZ_LIFETIMEBOUND [[msvc::lifetimebound]]
#elif __has_cpp_attribute(clang::lifetimebound)
#define GLZ_LIFETIMEBOUND [[clang::lifetimebound]]
#else
#define GLZ_LIFETIMEBOUND
#endif

#ifndef __has_cpp_attribute
#define GLZ_NO_UNIQUE_ADDRESS
#elif __has_cpp_attribute(no_unique_address)
#define GLZ_NO_UNIQUE_ADDRESS [[no_unique_address]]
#elif __has_cpp_attribute(msvc::no_unique_address) || ((defined _MSC_VER) && (!defined __clang__))
// Note __has_cpp_attribute(msvc::no_unique_address) itself doesn't work as
// of 19.30.30709, even though the attribute itself is supported. See
// https://github.com/llvm/llvm-project/issues/49358#issuecomment-981041089
#define GLZ_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
// no_unique_address is not available.
#define GLZ_NO_UNIQUE_ADDRESS
#endif
