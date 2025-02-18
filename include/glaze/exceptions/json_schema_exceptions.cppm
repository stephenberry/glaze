// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#ifdef CPP_MODULES
export module glaze.exceptions.json_schema_exceptions;
import glaze.exceptions.core_exceptions;
import glaze.json.schema;
#else
#include "glaze/exceptions/core_exceptions.cppm"
#include "glaze/json/schema.cppm"
#endif



#if __cpp_exceptions


namespace glz::ex
{
   template <class T, opts Opts = opts{}, class Buffer>
   void write_json_schema(Buffer&& buffer)
   {
      const auto ec = glz::write_json_schema<T, Opts>(buffer);
      if (bool(ec)) [[unlikely]] {
         throw std::runtime_error(format_error(ec, buffer));
      }
   }

   template <class T, opts Opts = opts{}>
   [[nodiscard]] std::string write_json_schema()
   {
      std::string buffer{};
      glz::ex::write_json_schema<T, Opts>(buffer);
      return buffer;
   }
}

#endif
