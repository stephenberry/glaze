// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/ptr.hpp"
#include "glaze/binary/read.hpp"
#include "glaze/binary/write.hpp"

namespace glz
{
   template <class T, class B>
   bool read_as_binary(T&& root_value, const sv json_ptr, B&& buffer) {
      return read_as<opts{.format = binary}>(std::forward<T>(root_value), json_ptr, buffer);
   }
   
   template <class T, class B>
   bool write_as_binary(T&& root_value, const sv json_ptr, B& buffer)
   {
      return write_as<opts{.format = binary}>(std::forward<T>(root_value), json_ptr, buffer);
   }
}
