// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.toml.common;

import glaze.core.common;

#include "glaze/util/inline.hpp"

namespace glz
{
   // Skip whitespace and comments
   export template <class It, class End>
   inline void skip_ws_and_comments(It&& it, End end) noexcept
   {
      while (it != end) {
         if (*it == ' ' || *it == '\t') {
            ++it;
         }
         else if (*it == '#') {
            while (it != end && *it != '\n' && *it != '\r') {
               ++it;
            }
         }
         else {
            break;
         }
      }
   }

   // Skip to next line
   export template <class Ctx, class It, class End>
   inline bool skip_to_next_line(Ctx&, It&& it, End end) noexcept
   {
      while (it != end && *it != '\n' && *it != '\r') {
         ++it;
      }

      if (it == end) {
         return false;
      }

      if (*it == '\r') {
         ++it;
         if (it != end && *it == '\n') {
            ++it;
         }
      }
      else if (*it == '\n') {
         ++it;
      }

      return true;
   }
} // namespace glz