// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   inline write_error beve_to_json(const std::string& beve, std::string& out) noexcept {
      size_t ix{}; // write index
      
      auto* it = beve.data();
      auto* end = it + beve.size();
      
      while (it < end)
      {
         const auto tag = uint8_t(*it);
         const auto type = tag & 0b0000'1111;
         switch (type)
         {
            case tag::null: {
               detail::dump<"null">(out, ix);
               break;
            }
            case tag::boolean: {
               if (tag >> 4) {
                  detail::dump<"true">(out, ix);
               }
               else {
                  detail::dump<"false">(out, ix);
               }
               break;
            }
            case tag::number: {
               
               break;
            }
            default: {
               return {error_code::syntax_error};
            }
         }
         ++it;
      }
      return {};
   }
}
