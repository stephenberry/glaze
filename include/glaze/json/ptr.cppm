// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#ifdef CPP_MODULES
export module glaze.json.ptr;
import glaze.core.ptr;
import glaze.json.read;
import glaze.json.write;
#else
#include "glaze/core/ptr.cppm"
#include "glaze/json/read.cppm"
#include "glaze/json/write.cppm"
#endif




namespace glz
{
   template <class T, class B>
   bool read_as_json(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return read_as<opts{}>(std::forward<T>(root_value), json_ptr, buffer);
   }

   template <class T, class B>
   bool write_as_json(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return write_as<opts{}>(std::forward<T>(root_value), json_ptr, buffer);
   }
}
