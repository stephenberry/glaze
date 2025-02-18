// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../Export.hpp"
#ifdef CPP_MODULES
export module glaze.glaze_exceptions;
export import glaze.exceptions.binary_exceptions;
export import glaze.exceptions.csv_exceptions;
export import glaze.exceptions.json_exceptions;
#else
#include "glaze/exceptions/binary_exceptions.cppm"
#include "glaze/exceptions/csv_exceptions.cppm"
#include "glaze/exceptions/json_exceptions.cppm"
#endif



#if __cpp_exceptions

// These files provide convenience functions that throw C++ exceptions, which can make code cleaner for users


namespace glz::ex
{
   template <opts Opts>
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

   template <opts Opts, class T, output_buffer Buffer>
   void write(T&& value, Buffer& buffer) noexcept
   {
      glz::write<Opts>(std::forward<T>(value), buffer);
   }

   template <opts Opts, class T, raw_buffer Buffer>
   size_t write(T&& value, Buffer&& buffer) noexcept
   {
      return glz::write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}

#endif
