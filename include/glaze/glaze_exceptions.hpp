// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __cpp_exceptions

// These files provide convenience functions that throw C++ exceptions, which can make code cleaner for users

#include "glaze/exceptions/binary_exceptions.hpp"
#include "glaze/exceptions/cbor_exceptions.hpp"
#include "glaze/exceptions/csv_exceptions.hpp"
#include "glaze/exceptions/json_exceptions.hpp"
#include "glaze/exceptions/msgpack_exceptions.hpp"

namespace glz::ex
{
   template <auto Opts>
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

}

#endif
