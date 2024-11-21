// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <type_traits>

#include "glaze/util/type_traits.hpp"

namespace glz
{
   // format
   inline constexpr uint32_t INVALID = 0;
   inline constexpr uint32_t BEVE = 1;
   inline constexpr uint32_t JSON = 10;
   inline constexpr uint32_t JSON_PTR = 20;
   inline constexpr uint32_t NDJSON = 100; // new line delimited JSON
   inline constexpr uint32_t MUSTACHE = 500;
   inline constexpr uint32_t CSV = 10000;
   inline constexpr uint32_t ERLANG = 20000;

   // layout csv
   inline constexpr uint8_t rowwise = 0;
   inline constexpr uint8_t colwise = 1;

   // layout erlang term
   inline constexpr uint8_t map = 0;
   inline constexpr uint8_t proplist = 1;

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

   using bits_class = std::uint64_t;

   // width in bit for option fields
   constexpr std::uint8_t fw_indentation_char = 8;
   constexpr std::uint8_t fw_indentation_width = 3;
   constexpr std::uint8_t fw_float_max_write_precision = 5;

   enum class option : std::uint8_t {
      null_terminated = 0, // Whether the input buffer is null terminated
      comments, // Support reading in JSONC style comments
      error_on_unknown_keys, // Error when an unknown key is encountered
      skip_null_members, // Skip writing out params in an object if the value is null
      use_hash_comparison, // Will replace some string equality checks with hash checks
      prettify, // Write out prettified JSON
      minified, // Require minified input for JSON, which results in faster read performance
      new_lines_in_arrays, // Whether prettified arrays should have new lines for each element
      shrink_to_fit, // Shrinks dynamic containers to new size to save memory
      write_type_info, // Write type info for meta objects in variants
      error_on_missing_keys, // Require all non nullable keys to be present in the object. Use skip_null_members = false
                             // to require nullable members
      error_on_const_read, // Error if attempt is made to read into a const value, by default the value is skipped
                           // without error
      validate_skipped, // If full validation should be performed on skipped values
      validate_trailing_whitespace, // If, after parsing a value, we want to validate the trailing whitespace
      bools_as_numbers, // Read and write booleans with 1's and 0's
      escaped_unicode_key_conversion, // JSON does not require escaped unicode keys to match with unescaped UTF-8
                                      // This enables automatic escaped unicode unescaping and matching for keys in
                                      // glz::object, but it comes at a performance cost.
      quoted_num, // Treat numbers as quoted or array-like types as having quoted numbers
      number, // Read numbers as strings and write these string as numbers
      raw, // Write out string like values without quotes
      raw_string, // Do not decode/encode escaped characters for strings (improves read/write performance)
      structs_as_arrays, // Handle structs (reading/writing) without keys, which applies
      allow_conversions, // Whether conversions between convertible types are allowed in binary, e.g. double -> float
      partial_read, // Reads into only existing fields and elements and then exits without parsing the rest of the input
      partial_read_nested, // Advance the partially read struct to the end of the struct
      concatenate, // Concatenates ranges of std::pair into single objects when writing
      hide_non_invocable, // Hides non-invocable members from the cli_menu (may be applied elsewhere in the future)
      indentation_char,
      indentation_width = indentation_char + fw_indentation_char,
      float_max_write_precision =
         indentation_width + fw_indentation_width, // The maximum precision type used for writing floats, higher
                                                   // precision floats will be cast down to this precision
      layout = float_max_write_precision + fw_float_max_write_precision, // CSV row wise output/input

      last_option_terminator
   };

   static_assert(std::uint8_t(option::last_option_terminator) <= sizeof(bits_class) * 8);

   template <typename U>
      requires(std::is_integral_v<U> && std::is_unsigned_v<U>)
   struct options
   {
      // TODO make everything consteval for gcc14+
      constexpr options() = default;
      constexpr options(const options& o) = default;
      constexpr options(const U& b) : bits{b} {};

      template <std::uint8_t width = 1, typename V>
         requires(width <= sizeof(V) * 8)
      constexpr options& set(option b, V v)
      {
         if constexpr (std::is_same_v<V, bool>) {
            if (v) {
               bits |= U(v) << static_cast<std::underlying_type_t<option>>(b);
            }
            else {
               bits = bits & ~(U(1) << static_cast<std::underlying_type_t<option>>(b));
            }
         }
         else {
            static constexpr U mask = ((U(1) << (width + 1)) - 1);
            auto val = (U(static_cast<std::make_unsigned_t<V>>(v)) & mask)
                       << static_cast<std::underlying_type_t<option>>(b);
            bits = (bits & ~(mask << static_cast<std::underlying_type_t<option>>(b))) | val;
         }

         return *this;
      }

      consteval operator U() const { return bits; }

      U bits{};
   };

   constexpr auto json_options_default = options<bits_class>()
                                            .set(option::null_terminated, GLZ_NULL_TERMINATED)
                                            .set(option::error_on_unknown_keys, true)
                                            .set(option::skip_null_members, true)
                                            .set(option::use_hash_comparison, true)
                                            .set(option::new_lines_in_arrays, true)
                                            .set(option::write_type_info, true)
                                            .set(option::allow_conversions, true)
                                            .set(option::concatenate, true)
                                            .set(option::hide_non_invocable, true)
                                            .set(option::indentation_char, ' ')
                                            .set(option::indentation_width, 3)
                                            .set(option::layout, rowwise);

   struct opts
   {
      // USER CONFIGURABLE
      uint32_t format = JSON;
      bits_class bits = json_options_default;

      char indentation_char = ' '; // Prettified JSON indentation char
      uint8_t indentation_width = 3; // Prettified JSON indentation size

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
   };

   template <std::uint8_t width, typename R>
      requires((width > 0) && (width <= sizeof(R) * 8) && (not std::is_reference_v<R>))
   consteval R get(const opts& o, option b)
   {
      using Ret = std::remove_cv_t<R>;

      using UR = std::make_unsigned_t<Ret>; //  TODO make it bigger
      static constexpr UR mask = (UR(1) << (width + 1)) - 1;

      Ret ret = (o.bits >> uint8_t(b)) & mask;
      return ret;
   }

   template <>
   consteval bool get<1, bool>(const opts& o, option b)
   {
      return o.bits & (uint64_t(1) << uint8_t(b));
   }

   consteval bool has(const opts& o, option b) { return get<1, bool>(o, b); }

   consteval bool has_opening_handled(const opts& o) { return o.internal & uint32_t(opts::internal::opening_handled); }

   consteval bool has_closing_handled(const opts& o) { return o.internal & uint32_t(opts::internal::closing_handled); }

   consteval bool has_ws_handled(const opts& o) { return o.internal & uint32_t(opts::internal::ws_handled); }

   consteval bool has_no_header(const opts& o) { return o.internal & uint32_t(opts::internal::no_header); }

   consteval bool has_disable_write_unknown(const opts& o)
   {
      return o.internal & uint32_t(opts::internal::disable_write_unknown);
   }

   consteval bool has_is_padded(const opts& o) { return o.internal & uint32_t(opts::internal::is_padded); }

   consteval bool has_disable_padding(const opts& o) { return o.internal & uint32_t(opts::internal::disable_padding); }

   consteval bool has_write_unchecked(const opts& o) { return o.internal & uint32_t(opts::internal::write_unchecked); }

   consteval bool has_format(const opts& o, std::uint32_t format) { return o.format == format; }

   consteval bool has_layout(const opts& o, std::uint8_t layout)
   {
      return get<1, std::uint8_t>(o, option::layout) == layout;
   }

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

   template <opts Opts, auto bit, std::uint8_t width, auto value>
   constexpr auto set_opt2()
   {
      opts ret = Opts;

      using T = decltype(value);
      if constexpr (std::is_enum_v<T>) {
         using U = std::underlying_type_t<T>;
         ret.bits = options<bits_class>(Opts.bits).template set<width, U>(bit, static_cast<U>(value));
      }
      else {
         ret.bits = options<bits_class>(Opts.bits).template set<width>(bit, value);
      }

      return ret;
   }

   template <opts Opts, auto bit>
   constexpr auto opt_on()
   {
      opts ret = Opts;
      ret.bits |= decltype(ret.bits)(1 << uint8_t(bit));
      return ret;
   }

   template <opts Opts, auto bit>
   inline constexpr auto opt_true = opt_on<Opts, bit>();

   template <opts Opts, auto bit>
   constexpr auto opt_off()
   {
      opts ret = Opts;
      ret.bits = ret.bits & ~(decltype(ret.bits)(1) << uint8_t(bit));
      return ret;
   }

   template <opts Opts, auto bit>
   inline constexpr auto opt_false = opt_off<Opts, bit>();

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
}

namespace glz
{
   namespace detail
   {
      template <uint32_t Format = INVALID, class T = void>
      struct to;

      template <uint32_t Format = INVALID, class T = void>
      struct to_partial;

      template <uint32_t Format = INVALID, class T = void>
      struct from;

      template <uint32_t Format = INVALID>
      struct skip_value;
   }

   template <class T>
   concept write_beve_supported = requires { detail::to<BEVE, std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_beve_supported = requires { detail::from<BEVE, std::remove_cvref_t<T>>{}; };

   template <class T>
   concept write_json_supported = requires { detail::to<JSON, std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_json_supported = requires { detail::from<JSON, std::remove_cvref_t<T>>{}; };

   template <class T>
   concept write_ndjson_supported = requires { detail::to<NDJSON, std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_ndjson_supported = requires { detail::from<NDJSON, std::remove_cvref_t<T>>{}; };

   template <class T>
   concept write_csv_supported = requires { detail::to<CSV, std::remove_cvref_t<T>>{}; };

   template <class T>
   concept read_csv_supported = requires { detail::from<CSV, std::remove_cvref_t<T>>{}; };

   // specialize this struct for every format for writing to
   template <uint32_t Format, class T>
   struct write_format_supported;

   template <uint32_t Format, class T>
   concept write_supported = write_format_supported<Format, T>::value;

   // specialize this struct for every format for reading from
   template <uint32_t Format, class T>
   struct read_format_supported;

   template <uint32_t Format, class T>
   concept read_supported = read_format_supported<Format, T>::value;
}
