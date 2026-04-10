// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.exceptions;

#if __cpp_exceptions

// These files provide convenience functions that throw C++ exceptions, which can make code cleaner for users

export import glaze.exceptions.binary_exceptions;
export import glaze.exceptions.cbor_exceptions;
export import glaze.exceptions.core_exceptions;
export import glaze.exceptions.csv_exceptions;
export import glaze.exceptions.json_exceptions;
export import glaze.exceptions.json_schema_exceptions;
export import glaze.exceptions.msgpack_exceptions;

import std;

import glaze.concepts.container_concepts;
import glaze.core.opts;
import glaze.core.read;
import glaze.core.reflect;
import glaze.core.write;

using std::size_t;

namespace glz::ex
{
   export template <auto Opts>
   void read(auto& value, auto&& buffer)
   {
      auto ec = glz::read<Opts>(value, buffer);
      if (ec) {
         if constexpr (Opts.format == JSON) {
            throw std::runtime_error("read error: " + glz::format_error(ec, buffer));
         }
         else {
            throw std::runtime_error("read error");
         }
      }
   }

   export template <auto Opts, class T, output_buffer Buffer>
   void write(T&& value, Buffer& buffer) noexcept
   {
      glz::write<Opts>(std::forward<T>(value), buffer);
   }

   export template <auto Opts, class T, raw_buffer Buffer>
   size_t write(T&& value, Buffer&& buffer) noexcept
   {
      return glz::write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}

#endif
