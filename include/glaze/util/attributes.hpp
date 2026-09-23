// Glaze Library
// For the license information refer to glaze.hpp

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

// The MSVC spelling is tested before the standard one. Under the MSVC ABI [[no_unique_address]]
// is accepted but deliberately has no layout effect, so a front end there can report it through
// __has_cpp_attribute while ignoring it. Testing the standard spelling first would select that
// inert form and silently give up the empty member optimization on the whole ABI.
#if defined(_MSC_VER) && !defined(__clang__)
// cl.exe is matched directly rather than through __has_cpp_attribute, which did not report
// msvc::no_unique_address as of 19.30.30709 even though the attribute was supported. See
// https://github.com/llvm/llvm-project/issues/49358#issuecomment-981041089
#define GLZ_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif !defined(__has_cpp_attribute)
#define GLZ_NO_UNIQUE_ADDRESS
#elif __has_cpp_attribute(msvc::no_unique_address)
// clang-cl, and any other front end targeting the MSVC ABI
#define GLZ_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
#define GLZ_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
// no_unique_address is not available.
#define GLZ_NO_UNIQUE_ADDRESS
#endif
