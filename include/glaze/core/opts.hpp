// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"

namespace glz
{
   struct opts
   {
      // user configurable options
      uint32_t format = json;
      bool comments = false; // write out comments
      bool error_on_unknown_keys = true;
      
      // meant for internal use
      bool whitespace_handled = false;
      bool opening_handled = false; // the opening character has been handled
   };
   
   template <opts Opts>
   constexpr auto opening_handled()
   {
      opts ret = Opts;
      ret.opening_handled = true;
      return ret;
   };
   
   template <opts Opts>
   constexpr auto ws_handled()
   {
      opts ret = Opts;
      ret.whitespace_handled = true;
      return ret;
   };
   
   template <opts Opts>
   constexpr auto ws_and_opening_handled()
   {
      opts ret = Opts;
      ret.whitespace_handled = true;
      ret.opening_handled = true;
      return ret;
   };
}
