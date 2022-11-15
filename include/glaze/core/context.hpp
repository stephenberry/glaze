// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

namespace glz
{
   // Runtime context for configuration
   // We do not template the context on iterators so that it can be easily shared across buffer implementations
   struct context final
   {
      size_t ix{}; // index for buffer writing
      
      bool prettify = false; // write out prettified JSON
   };
}
