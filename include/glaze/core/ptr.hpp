// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/write.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/json/json_ptr.hpp"

namespace glz
{
   // Given a JSON pointer path, reads from the buffer into the object
   template <opts Opts, class T, class B>
   bool read_as(T&& root_value, const sv json_ptr, B&& buffer) {
      return detail::seek_impl(
        [&](auto&& val) {
          read<Opts>(val, buffer);
        },
        std::forward<T>(root_value), json_ptr
    );
   }
   
   // Given a JSON pointer path, writes into a buffer the specified value
   template <opts Opts, class T, class B>
   bool write_as(T&& root_value, const sv json_ptr, B& buffer)
   {
      return detail::seek_impl([&](auto&& val) { write<Opts>(val, buffer); },
                       std::forward<T>(root_value), json_ptr);
   }
}
