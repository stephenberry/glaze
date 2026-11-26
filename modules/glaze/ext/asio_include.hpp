// Glaze Library
// For the license information refer to glaze.ixx
#pragma once

// Two distinct, intentionally similar macros govern the Asio backend:
//
//   GLZ_USE_BOOST_ASIO   - Input: forces Boost.Asio even when standalone Asio
//                          is also available.
//   GLZ_USING_BOOST_ASIO - Output: reports that Boost.Asio was selected.
//
// CMake's glaze::asio target defines GLZ_USE_BOOST_ASIO when appropriate so
// every Glaze networking component selects the same backend.

#if defined(_WIN32) && !defined(_WIN32_WINNT) && !defined(_WIN32_WINDOWS)
// Asio otherwise warns and assumes Windows 7. Set that default explicitly only
// when the build has not already selected a Windows target version.
#define _WIN32_WINNT 0x0601
#endif

#if defined(_WIN32) && defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#error glaze/ext/asio_include.hpp cannot be included after Windows.h has included Winsock.h. \
       Define WIN32_LEAN_AND_MEAN before including Windows.h to prevent Windows.h from including Winsock.h.
#endif

// Standalone Asio does not infer ASIO_NO_TYPEID from -fno-rtti. Select its
// supported no-typeid path automatically when the compiler has RTTI disabled.
#if !defined(__cpp_rtti) && !defined(__GXX_RTTI) && !defined(_CPPRTTI)
#ifndef ASIO_NO_TYPEID
#define ASIO_NO_TYPEID
#endif
#ifndef BOOST_ASIO_NO_TYPEID
#define BOOST_ASIO_NO_TYPEID
#endif
#endif

#if __has_include(<asio.hpp>) && !defined(GLZ_USE_BOOST_ASIO)
#include <asio.hpp>
#ifdef GLZ_ENABLE_SSL
#include <asio/ssl.hpp>
#endif
#elif __has_include(<boost/asio.hpp>)
#ifndef GLZ_USING_BOOST_ASIO
#define GLZ_USING_BOOST_ASIO
#endif
#include <boost/asio.hpp>
#ifdef GLZ_ENABLE_SSL
#include <boost/asio/ssl.hpp>
#endif
#else
static_assert(false, "standalone or boost asio must be available to use Glaze networking");
#endif

namespace glz
{
#if defined(GLZ_USING_BOOST_ASIO)
   namespace asio
   {
      using namespace boost::asio;
      using error_code = boost::system::error_code;
      using system_error = boost::system::system_error;
   }
#endif
}
