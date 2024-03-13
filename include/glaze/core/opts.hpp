// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

namespace glz
{
   enum struct format : uint32_t
   {
      binary,
      json,
      ndjson, // new line delimited JSON
      csv,
   };
   
   enum struct comments : bool
   {
      no,
      yes
   };
   
   enum struct force_conformance : bool
   {
      no,
      yes
   };
   
   enum struct structs_as_arrays : bool
   {
      no,
      yes
   };
   
   enum struct layout : uint8_t
   {
      rowwise,
      colwise
   };
   
   struct opts
   {
      // USER CONFIGURABLE
      glz::format format = glz::format::json;
      glz::comments comments{}; // Write out comments
      bool error_on_unknown_keys = true; // Error when an unknown key is encountered
      bool skip_null_members = true; // Skip writing out params in an object if the value is null
      bool use_hash_comparison = true; // Will replace some string equality checks with hash checks
      bool prettify = false; // Write out prettified JSON
      char indentation_char = ' '; // Prettified JSON indentation char
      uint8_t indentation_width = 3; // Prettified JSON indentation size
      bool shrink_to_fit = false; // Shrinks dynamic containers to new size to save memory
      bool write_type_info = true; // Write type info for meta objects in variants
      glz::force_conformance force_conformance{}; // Do not allow invalid json normally accepted such as comments, nan, inf.
      bool error_on_missing_keys = false; // Require all non nullable keys to be present in the object. Use
                                          // skip_null_members = false to require nullable members
      bool error_on_const_read =
         false; // Error if attempt is made to read into a const value, by default the value is skipped without error
      glz::layout layout = glz::layout::rowwise; // CSV row wise output/input
      bool quoted_num = false; // treat numbers as quoted or array-like types as having quoted numbers
      bool number = false; // read numbers as strings and write these string as numbers
      bool raw = false; // write out string like values without quotes
      bool raw_string = false; // do not decode/encode escaped characters for strings (improves read/write performance)
      structs_as_arrays structs_as_arrays{}; // Handle structs (reading/writing) without keys, which applies to reflectable and
                                      // glaze_object_t concepts
      bool partial_read_nested = false; // Rewind forward the partially readed struct to the end of the struct
      bool concatenate = true; // concatenates ranges of std::pair into single objects when writing

      // INTERNAL USE
      bool opening_handled = false; // the opening character has been handled
      bool closing_handled = false; // the closing character has been handled
      bool ws_handled = false; // whitespace has already been parsed
      bool no_header = false; // whether or not a binary header is needed
      bool write_unknown = true; // whether to write unkwown fields

      [[nodiscard]] constexpr bool operator==(const opts&) const noexcept = default;
      
      constexpr opts() = default;
      
      template <class... Args>
      constexpr opts(Args... args) {
         (set(args), ...);
      }
      
      constexpr void set(glz::format v) { format = v; }
      constexpr void set(glz::comments v) { comments = v; }
      constexpr void set(glz::force_conformance v) { force_conformance = v; }
      constexpr void set(glz::layout v) { layout = v; }
      constexpr void set(glz::structs_as_arrays v) { structs_as_arrays = v; }
   };

   template <opts Opts>
   constexpr auto opening_handled()
   {
      opts ret = Opts;
      ret.opening_handled = true;
      return ret;
   }

   template <opts Opts>
   constexpr auto opening_and_closing_handled()
   {
      opts ret = Opts;
      ret.opening_handled = true;
      ret.closing_handled = true;
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
   constexpr auto opening_and_closing_handled_off()
   {
      opts ret = Opts;
      ret.opening_handled = false;
      ret.closing_handled = false;
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

   template <opts Opts, auto member_ptr>
   constexpr auto opt_on()
   {
      opts ret = Opts;
      ret.*member_ptr = true;
      return ret;
   }

   template <opts Opts, auto member_ptr>
   inline constexpr auto opt_true = opt_on<Opts, member_ptr>();

   template <opts Opts, auto member_ptr>
   constexpr auto opt_off()
   {
      opts ret = Opts;
      ret.*member_ptr = false;
      return ret;
   }

   template <opts Opts, auto member_ptr>
   inline constexpr auto opt_false = opt_off<Opts, member_ptr>();

   template <opts Opts>
   constexpr auto write_unknown_off()
   {
      opts ret = Opts;
      ret.write_unknown = false;
      return ret;
   }

   template <opts Opts>
   constexpr auto write_unknown_on()
   {
      opts ret = Opts;
      ret.write_unknown = true;
      return ret;
   }

   template <opts Opts>
   constexpr auto set_binary()
   {
      opts ret = Opts;
      ret.format = format::binary;
      return ret;
   }

   template <opts Opts>
   constexpr auto set_json()
   {
      opts ret = Opts;
      ret.format = format::json;
      return ret;
   }
}
