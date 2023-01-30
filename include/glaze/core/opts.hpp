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
      bool prettify = false;         // write out prettified JSON
      char indentation_char = ' ';   // prettified JSON indentation char
      uint8_t indentation_width = 3; // prettified JSON indentation size
      bool shrink_to_fit = false; // shrinks dynamic containers to new size to save memory
      bool write_type_info = true; // Write type info for meta objects in variants
      bool use_cx_tags = true; // whether binary output should write compile time known tags

      // INTERNAL USE
      bool opening_handled = false; // the opening character has been handled
      bool ws_handled = false; // whitespace has already been parsed
   };
   
   template <opts Opts>
   constexpr auto opening_handled()
   {
      opts ret = Opts;
      ret.opening_handled = true;
      return ret;
   };
   
   template <opts Opts>
   constexpr auto opening_handled_off()
   {
      opts ret = Opts;
      ret.opening_handled = false;
      return ret;
   };
   
   template <opts Opts>
   constexpr auto ws_handled()
   {
      opts ret = Opts;
      ret.ws_handled = true;
      return ret;
   };
   
   template <opts Opts>
   constexpr auto ws_handled_off()
   {
      opts ret = Opts;
      ret.ws_handled = false;
      return ret;
   };
}
