// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.json.ptr;

import std;

import glaze.core.ptr;
import glaze.core.opts;
import glaze.util.string_literal;

namespace glz
{
   export template <class T, class B>
   bool read_as_json(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return read_as<opts{}>(std::forward<T>(root_value), json_ptr, buffer);
   }

   export template <class T, class B>
   bool write_as_json(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return write_as<opts{}>(std::forward<T>(root_value), json_ptr, buffer);
   }
}
