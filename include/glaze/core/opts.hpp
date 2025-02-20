// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

#include "glaze/util/type_traits.hpp"

namespace glz
{
   // format
   // Built in formats must be less than 65536
   // User defined formats can be 65536 to 4294967296
   inline constexpr uint32_t INVALID = 0;
   inline constexpr uint32_t BEVE = 1;
   inline constexpr uint32_t JSON = 10;
   inline constexpr uint32_t JSON_PTR = 20;
   inline constexpr uint32_t NDJSON = 100; // new line delimited JSON
   inline constexpr uint32_t TOML = 400;
   inline constexpr uint32_t STENCIL = 500;
   inline constexpr uint32_t CSV = 10000;

   // layout
   inline constexpr uint8_t rowwise = 0;
   inline constexpr uint8_t colwise = 1;

   enum struct float_precision : uint8_t { //
      full, //
      float32 = 4, //
      float64 = 8, //
      float128 = 16 //
   };

   // We use 16 padding bytes because surrogate unicode pairs require 12 bytes
   // and we want a power of 2 buffer
   inline constexpr uint32_t padding_bytes = 16;

   // Write padding bytes simplifies our dump calculations by making sure we have significant excess
   inline constexpr size_t write_padding_bytes = 256;

   // This macro exists so that Glaze tests can change the default behavior
   // to easily run tests as if strings were not null terminated
#ifndef GLZ_NULL_TERMINATED
#define GLZ_NULL_TERMINATED true
#endif

   struct opts
   {
      // USER CONFIGURABLE
      uint32_t format = JSON;
      bool null_terminated = GLZ_NULL_TERMINATED; // Whether the input buffer is null terminated
      bool comments = false; // Support reading in JSONC style comments
      bool error_on_unknown_keys = true; // Error when an unknown key is encountered
      bool skip_null_members = true; // Skip writing out params in an object if the value is null
      bool use_hash_comparison = true; // Will replace some string equality checks with hash checks
      bool prettify = false; // Write out prettified JSON
      bool minified = false; // Require minified input for JSON, which results in faster read performance
      char indentation_char = ' '; // Prettified JSON indentation char
      uint8_t indentation_width = 3; // Prettified JSON indentation size
      bool new_lines_in_arrays = true; // Whether prettified arrays should have new lines for each element
      bool append_arrays = false; // When reading into an array the data will be appended if the type supports it
      bool shrink_to_fit = false; // Shrinks dynamic containers to new size to save memory
      bool write_type_info = true; // Write type info for meta objects in variants
      bool error_on_missing_keys = false; // Require all non nullable keys to be present in the object. Use
                                            // skip_null_members = false to require nullable members
      bool error_on_const_read =
         false; // Error if attempt is made to read into a const value, by default the value is skipped without error
      bool validate_skipped = false; // If full validation should be performed on skipped values
      bool validate_trailing_whitespace =
         false; // If, after parsing a value, we want to validate the trailing whitespace

      uint8_t layout = rowwise; // CSV row wise output/input

      // The maximum precision type used for writing floats, higher precision floats will be cast down to this precision
      float_precision float_max_write_precision{};

      bool bools_as_numbers = false; // Read and write booleans with 1's and 0's

      bool quoted_num = false; // treat numbers as quoted or array-like types as having quoted numbers
      bool number = false; // treats all types like std::string as numbers: read/write these quoted numbers
      bool raw = false; // write out string like values without quotes
      bool raw_string =
         false; // do not decode/encode escaped characters for strings (improves read/write performance)
      bool structs_as_arrays = false; // Handle structs (reading/writing) without keys, which applies
      bool allow_conversions = true; // Whether conversions between convertible types are
      // allowed in binary, e.g. double -> float

      bool partial_read =
         false; // Reads into the deepest structural object and then exits without parsing the rest of the input

      // glaze_object_t concepts
      bool concatenate = true; // Concatenates ranges of std::pair into single objects when writing

      bool hide_non_invocable =
         true; // Hides non-invocable members from the cli_menu (may be applied elsewhere in the future)

      enum struct internal : uint32_t {
         none = 0,
         opening_handled = 1 << 0, // the opening character has been handled
         closing_handled = 1 << 1, // the closing character has been handled
         ws_handled = 1 << 2, // whitespace has already been parsed
         no_header = 1 << 3, // whether or not a binary header is needed
         disable_write_unknown =
            1 << 4, // whether to turn off writing unknown fields for a glz::meta specialized for unknown writing
         is_padded = 1 << 5, // whether or not the read buffer is padded
         disable_padding = 1 << 6, // to explicitly disable padding for contexts like includers
         write_unchecked = 1 << 7 // the write buffer has sufficient space and does not need to be checked
      };
      // Sufficient space is only applicable to writing certain types and based on the write_padding_bytes

      // INTERNAL USE
      uint32_t internal{}; // default should be 0

      [[nodiscard]] constexpr bool operator==(const opts&) const noexcept = default;
   };

   consteval bool has_opening_handled(opts o) { return o.internal & uint32_t(opts::internal::opening_handled); }

   consteval bool has_closing_handled(opts o) { return o.internal & uint32_t(opts::internal::closing_handled); }

   consteval bool has_ws_handled(opts o) { return o.internal & uint32_t(opts::internal::ws_handled); }

   consteval bool has_no_header(opts o) { return o.internal & uint32_t(opts::internal::no_header); }

   consteval bool has_disable_write_unknown(opts o)
   {
      return o.internal & uint32_t(opts::internal::disable_write_unknown);
   }

   consteval bool has_is_padded(opts o) { return o.internal & uint32_t(opts::internal::is_padded); }

   consteval bool has_disable_padding(opts o) { return o.internal & uint32_t(opts::internal::disable_padding); }

   consteval bool has_write_unchecked(opts o) { return o.internal & uint32_t(opts::internal::write_unchecked); }

   template <opts Opts>
   constexpr auto opening_handled()
   {
      opts ret = Opts;
      ret.internal |= uint32_t(opts::internal::opening_handled);
      return ret;
   }

   template <opts Opts>
   constexpr auto opening_and_closing_handled()
   {
      opts ret = Opts;
      ret.internal |= (uint32_t(opts::internal::opening_handled) | uint32_t(opts::internal::closing_handled));
      return ret;
   }

   template <opts Opts>
   constexpr auto opening_handled_off()
   {
      opts ret = Opts;
      ret.internal &= ~uint32_t(opts::internal::opening_handled);
      return ret;
   }

   template <opts Opts>
   constexpr auto opening_and_closing_handled_off()
   {
      opts ret = Opts;
      ret.internal &= ~(uint32_t(opts::internal::opening_handled) | uint32_t(opts::internal::closing_handled));
      return ret;
   }

   template <opts Opts>
   constexpr auto ws_handled()
   {
      opts ret = Opts;
      ret.internal |= uint32_t(opts::internal::ws_handled);
      return ret;
   }

   template <opts Opts>
   constexpr auto ws_handled_off()
   {
      opts ret = Opts;
      ret.internal &= ~uint32_t(opts::internal::ws_handled);
      return ret;
   }

   template <opts Opts>
   constexpr auto no_header_on()
   {
      opts ret = Opts;
      ret.internal |= uint32_t(opts::internal::no_header);
      return ret;
   }

   template <opts Opts>
   constexpr auto no_header_off()
   {
      opts ret = Opts;
      ret.internal &= ~uint32_t(opts::internal::no_header);
      return ret;
   }

   template <opts Opts>
   constexpr auto is_padded_on()
   {
      opts ret = Opts;
      ret.internal |= uint32_t(opts::internal::is_padded);
      return ret;
   }

   template <opts Opts>
   constexpr auto is_padded_off()
   {
      opts ret = Opts;
      ret.internal &= ~uint32_t(opts::internal::is_padded);
      return ret;
   }

   template <opts Opts>
   constexpr auto disable_padding_on()
   {
      opts ret = Opts;
      ret.internal |= uint32_t(opts::internal::disable_padding);
      return ret;
   }

   template <opts Opts>
   constexpr auto disable_padding_off()
   {
      opts ret = Opts;
      ret.internal &= ~uint32_t(opts::internal::disable_padding);
      return ret;
   }

   template <opts Opts>
   constexpr auto write_unchecked_on()
   {
      opts ret = Opts;
      ret.internal |= uint32_t(opts::internal::write_unchecked);
      return ret;
   }

   template <opts Opts>
   constexpr auto write_unchecked_off()
   {
      opts ret = Opts;
      ret.internal &= ~uint32_t(opts::internal::write_unchecked);
      return ret;
   }

   template <opts Opts, auto member_ptr>
   constexpr auto set_opt(auto&& value)
   {
      opts ret = Opts;
      ret.*member_ptr = value;
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
   constexpr auto disable_write_unknown_off()
   {
      opts ret = Opts;
      ret.internal &= ~uint32_t(opts::internal::disable_write_unknown);
      return ret;
   }

   template <opts Opts>
   constexpr auto disable_write_unknown_on()
   {
      opts ret = Opts;
      ret.internal |= uint32_t(opts::internal::disable_write_unknown);
      return ret;
   }

   template <opts Opts>
   constexpr auto set_beve()
   {
      opts ret = Opts;
      ret.format = BEVE;
      return ret;
   }

   template <opts Opts>
   constexpr auto set_json()
   {
      opts ret = Opts;
      ret.format = JSON;
      return ret;
   }

   template <opts Opts>
   constexpr auto set_toml()
   {
      opts ret = Opts;
      ret.format = TOML;
      return ret;
   }
}

namespace glz
{
   template <uint32_t Format = INVALID, class T = void>
   struct to;
   
   template <uint32_t Format = INVALID, class T = void>
   struct from;
   
   template <uint32_t Format = INVALID, class T = void>
   struct to_partial;
   
   template <uint32_t Format = INVALID>
   struct skip_value;

   template <uint32_t Format, class T>
   concept write_supported = requires { to<Format, std::remove_cvref_t<T>>{}; };

   template <uint32_t Format, class T>
   concept read_supported = requires { from<Format, std::remove_cvref_t<T>>{}; };
   
   // These templates save typing by determining the core type used to select the proper to/from specialization
   // Long term I would like to remove these detail indirections.
   
   template <uint32_t Format>
   struct parse
   {};
   
   template <uint32_t Format>
   struct serialize
   {};
   
   template <uint32_t Format>
   struct serialize_partial
   {};
}
