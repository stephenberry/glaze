// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"

namespace glz
{
   struct opts
   {
      // USER CONFIGURABLE
      uint32_t format = json;
      bool comments = false; // write out comments
      bool error_on_unknown_keys = true; // error when an unknown key is encountered
      bool skip_null_members = true; // skip writing out params in an object if the value is null
      bool no_except = false; // turn off and on throwing exceptions
      bool allow_hash_check = false; // Will replace some string equality checks with hash checks
      bool prettify = false; // write out prettified JSON
      bool rowwise = true; // rowwise output for csv, false is column wise
      
      // INTERNAL USE
      bool opening_handled = false; // the opening character has been handled
   };
   
   template <opts Opts>
   constexpr auto opening_handled()
   {
      opts ret = Opts;
      ret.opening_handled = true;
      return ret;
   };
}
