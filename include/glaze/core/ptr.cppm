// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#ifdef CPP_MODULES
export module glaze.core.ptr;
import glaze.core.opts;
import glaze.core.read;
import glaze.core.write;
import glaze.json.json_ptr;
import glaze.util.for_each;
#else
#include "glaze/core/opts.cppm"
#include "glaze/core/read.cppm"
#include "glaze/core/write.cppm"
#include "glaze/json/json_ptr.cppm"
#include "glaze/util/for_each.cppm"
#endif




namespace glz
{
   // Given a JSON pointer path, reads from the buffer into the object
   template <opts Opts, class T, class B>
   [[nodiscard]] error_ctx read_as(T&& root_value, const sv json_ptr, B&& buffer)
   {
      error_ctx pe{};
      bool b =
         detail::seek_impl([&](auto&& val) { pe = read<Opts>(val, buffer); }, std::forward<T>(root_value), json_ptr);
      if (b) {
         return pe;
      }
      pe.ec = error_code::seek_failure;
      return pe;
   }

   // Given a JSON pointer path, writes into a buffer the specified value
   template <opts Opts, class T, class B>
   [[nodiscard]] bool write_as(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return detail::seek_impl(
         [&](auto&& val) {
            if constexpr (raw_buffer<B>) {
               std::ignore = write<Opts>(
                  val, buffer); // We assume the user has sufficient null characters at the end of the buffer
            }
            else {
               write<Opts>(val, buffer);
            }
         },
         std::forward<T>(root_value), json_ptr);
   }
}
