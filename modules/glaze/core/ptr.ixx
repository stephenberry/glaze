// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.core.ptr;

import std;

import glaze.core.opts;
import glaze.core.context;
import glaze.core.buffer_traits;
import glaze.core.read;
import glaze.core.write;
import glaze.core.seek;

import glaze.json.json_ptr;
import glaze.concepts.container_concepts;

import glaze.util.for_each;
import glaze.util.string_literal;

namespace glz
{
   // Given a JSON pointer path, reads from the buffer into the object
   export template <auto Opts, class T, class B>
   [[nodiscard]] error_ctx read_as(T&& root_value, const sv json_ptr, B&& buffer)
   {
      error_ctx pe{};
      bool b = seek([&](auto&& val) { pe = read<Opts>(val, buffer); }, std::forward<T>(root_value), json_ptr);
      if (b) {
         return pe;
      }
      pe.ec = error_code::seek_failure;
      return pe;
   }

   // Given a JSON pointer path, writes into a buffer the specified value
   export template <auto Opts, class T, class B>
   [[nodiscard]] bool write_as(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return seek(
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
