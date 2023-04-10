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
      bool comments = false; // Write out comments
      bool error_on_unknown_keys = true; // Error when an unknown key is encountered
      bool skip_null_members = true; // Skip writing out params in an object if the value is null
      bool allow_hash_check = true; // Will replace some string equality checks with hash checks
      bool prettify = false; // Write out prettified JSON
      char indentation_char = ' '; // Prettified JSON indentation char
      uint8_t indentation_width = 3; // Prettified JSON indentation size
      bool shrink_to_fit = false; // Shrinks dynamic containers to new size to save memory
      bool write_type_info = true; // Write type info for meta objects in variants
      bool use_cx_tags = true; // Whether binary output should write compile time known tags
      bool force_conformance = false; // Do not allow invalid json normally accepted such as comments, nan, inf.
      bool error_on_missing_keys = false; // Require all non nullable keys to be present in the object. Use
                                          // skip_null_members = false to require nullable members
      uint32_t layout = rowwise; // CSV row wise output/input

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
   }

   template <opts Opts>
   constexpr auto opening_handled_off()
   {
      opts ret = Opts;
      ret.opening_handled = false;
      return ret;
   }

   template <opts Opts>
   constexpr auto ws_handled()
   {
      opts ret = Opts;
      ret.ws_handled = true;
      return ret;
   }

   template <opts Opts>
   constexpr auto ws_handled_off()
   {
      opts ret = Opts;
      ret.ws_handled = false;
      return ret;
   }
}
