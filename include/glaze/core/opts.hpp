// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

#include "glaze/util/type_traits.hpp"

namespace glz
{
   // format
   constexpr uint32_t binary = 0;
   constexpr uint32_t json = 10;
   constexpr uint32_t ndjson = 100; // new line delimited JSON
   constexpr uint32_t csv = 10000;

   // layout
   constexpr uint8_t rowwise = 0;
   constexpr uint8_t colwise = 1;

   enum struct float_precision : uint8_t { full, float32 = 4, float64 = 8, float128 = 16 };

   // We use 16 padding bytes because surrogate unicode pairs require 12 bytes
   // and we want a power of 2 buffer
   constexpr uint32_t padding_bytes = 16;

   // Write padding bytes simplifies our dump calculations by making sure we have significant excess
   constexpr uint32_t write_padding_bytes = 256;

   // We use a alias to a uint8_t for booleans so that compiler errors will print "0" or "1" rather than "true" or
   // "false" This shortens compiler error printouts significantly.
   // We use a macro rather than an alias because some compilers print out alias definitions, extending length.
#define bool_t uint8_t

   struct opts
   {
      // USER CONFIGURABLE
      uint32_t format = json;
      bool_t comments = false; // Support reading in JSONC style comments
      bool_t error_on_unknown_keys = true; // Error when an unknown key is encountered
      bool_t skip_null_members = true; // Skip writing out params in an object if the value is null
      bool_t use_hash_comparison = true; // Will replace some string equality checks with hash checks
      bool_t prettify = false; // Write out prettified JSON
      bool_t minified = false; // Require minified input for JSON, which results in faster read performance
      char indentation_char = ' '; // Prettified JSON indentation char
      uint8_t indentation_width = 3; // Prettified JSON indentation size
      bool_t new_lines_in_arrays = true; // Whether prettified arrays should have new lines for each element
      bool_t shrink_to_fit = false; // Shrinks dynamic containers to new size to save memory
      bool_t write_type_info = true; // Write type info for meta objects in variants
      bool_t error_on_missing_keys = false; // Require all non nullable keys to be present in the object. Use
                                            // skip_null_members = false to require nullable members
      bool_t error_on_const_read =
         false; // Error if attempt is made to read into a const value, by default the value is skipped without error
      bool_t validate_skipped = false; // If full validation should be performed on skipped values
      bool_t validate_trailing_whitespace =
         false; // If, after parsing a value, we want to validate the trailing whitespace

      uint8_t layout = rowwise; // CSV row wise output/input

      // The maximum precision type used for writing floats, higher precision floats will be cast down to this precision
      float_precision float_max_write_precision{};

      bool_t bools_as_numbers = false; // Read and write booleans with 1's and 0's

      bool_t escaped_unicode_key_conversion =
         false; // JSON does not require escaped unicode keys to match with unescaped UTF-8
      // This enables automatic escaped unicode unescaping and matching for keys in glz::object, but it comes at a
      // performance cost.

      bool_t quoted_num = false; // treat numbers as quoted or array-like types as having quoted numbers
      bool_t number = false; // read numbers as strings and write these string as numbers
      bool_t raw = false; // write out string like values without quotes
      bool_t raw_string =
         false; // do not decode/encode escaped characters for strings (improves read/write performance)
      bool_t structs_as_arrays = false; // Handle structs (reading/writing) without keys, which applies
      bool_t allow_conversions = true; // Whether conversions between convertible types are
      // allowed in binary, e.g. double -> float

      bool_t partial_read =
         false; // Reads into only existing fields and elements and then exits without parsing the rest of the input

      // glaze_object_t concepts
      bool_t partial_read_nested = false; // Advance the partially read struct to the end of the struct
      bool_t concatenate = true; // Concatenates ranges of std::pair into single objects when writing

      bool_t hide_non_invocable =
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

#undef bool_t

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
   constexpr auto set_binary()
   {
      opts ret = Opts;
      ret.format = binary;
      return ret;
   }

   template <opts Opts>
   constexpr auto set_json()
   {
      opts ret = Opts;
      ret.format = json;
      return ret;
   }
}

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct to_binary;

      template <class T = void>
      struct from_binary;

      template <class T = void>
      struct to_json;

      template <class T = void>
      struct from_json;

      template <class T = void>
      struct to_ndjson;

      template <class T = void>
      struct from_ndjson;

      template <class T = void>
      struct to_csv;

      template <class T = void>
      struct from_csv;
   }

   template <class T>
   concept write_binary_supported = requires { detail::to_binary<std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_binary_supported = requires { detail::from_binary<std::remove_cvref_t<T>>{}; };

   template <class T>
   concept write_json_supported = requires { detail::to_json<std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_json_supported = requires { detail::from_json<std::remove_cvref_t<T>>{}; };

   template <class T>
   concept write_ndjson_supported = requires { detail::to_ndjson<std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_ndjson_supported = requires { detail::from_ndjson<std::remove_cvref_t<T>>{}; };

   template <class T>
   concept write_csv_supported = requires { detail::to_csv<std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_csv_supported = requires { detail::from_csv<std::remove_cvref_t<T>>{}; };

   template <uint32_t Format, class T>
   consteval bool write_format_supported()
   {
      if constexpr (Format == binary) {
         return write_binary_supported<T>;
      }
      else if constexpr (Format == json) {
         return write_json_supported<T>;
      }
      else if constexpr (Format == ndjson) {
         return write_ndjson_supported<T>;
      }
      else if constexpr (Format == csv) {
         return write_csv_supported<T>;
      }
      else {
         static_assert(false_v<T>, "Glaze metadata is probably needed for your type");
      }
   }

   template <uint32_t Format, class T>
   consteval bool read_format_supported()
   {
      if constexpr (Format == binary) {
         return read_binary_supported<T>;
      }
      else if constexpr (Format == json) {
         return read_json_supported<T>;
      }
      else if constexpr (Format == ndjson) {
         return read_ndjson_supported<T>;
      }
      else if constexpr (Format == csv) {
         return read_csv_supported<T>;
      }
      else {
         static_assert(false_v<T>, "Glaze metadata is probably needed for your type");
      }
   }

   template <uint32_t Format, class T>
   concept write_supported = write_format_supported<Format, T>();

   template <uint32_t Format, class T>
   concept read_supported = read_format_supported<Format, T>();
}
