// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.exceptions.json_schema_exceptions;

#if __cpp_exceptions

import std;

import glaze.core.opts;
import glaze.core.reflect;

import glaze.exceptions.core_exceptions;

import glaze.json.schema;

namespace glz::ex
{
   export template <class T, auto Opts = opts{}, class Buffer>
   void write_json_schema(Buffer&& buffer)
   {
      const auto ec = glz::write_json_schema<T, Opts>(buffer);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }

   export template <class T, auto Opts = opts{}>
   [[nodiscard]] std::string write_json_schema()
   {
      std::string buffer{};
      glz::ex::write_json_schema<T, Opts>(buffer);
      return buffer;
   }
}

#endif
