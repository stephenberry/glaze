// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(GLAZE_CXX_MODULE)
#define GLAZE_EXPORT export
#else
#define GLAZE_EXPORT
#endif

#include "glaze/beve/read.hpp"
#include "glaze/beve/write.hpp"
#include "glaze/core/ptr.hpp"

GLAZE_EXPORT namespace glz
{
   template <class T, class B>
   bool read_as_binary(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return read_as<opts{.format = BEVE}>(std::forward<T>(root_value), json_ptr, buffer);
   }

   template <class T, class B>
   bool write_as_binary(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return write_as<opts{.format = BEVE}>(std::forward<T>(root_value), json_ptr, buffer);
   }
}
