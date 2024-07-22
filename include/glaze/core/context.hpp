// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

#include "glaze/reflection/enum_macro.hpp"

namespace glz
{
   constexpr size_t max_recursive_depth_limit = 256;

   GLZ_ENUM(error_code,
            none, //
            no_read_input, //
            data_must_be_null_terminated, //
            parse_number_failure, //
            expected_brace, //
            expected_bracket, //
            expected_quote, //
            expected_comma, //
            expected_colon, //
            exceeded_static_array_size, //
            exceeded_max_recursive_depth, //
            unexpected_end, //
            expected_end_comment, //
            syntax_error, //
            unexpected_enum, //
            attempt_const_read, //
            attempt_member_func_read, //
            attempt_read_hidden, //
            invalid_nullable_read, //
            invalid_variant_object, //
            invalid_variant_array, //
            invalid_variant_string, //
            no_matching_variant_type, //
            expected_true_or_false, //
            // Key errors
            key_not_found, //
            unknown_key, //
            missing_key, //
            // Other errors
            invalid_flag_input, //
            invalid_escape, //
            u_requires_hex_digits, //
            unicode_escape_conversion_failure, //
            dump_int_error, //
            // File errors
            file_open_failure, //
            file_close_failure, //
            file_include_error, //
            file_extension_not_supported, //
            could_not_determine_extension, //
            // JSON pointer access errors
            get_nonexistent_json_ptr, //
            get_wrong_type, //
            seek_failure, //
            // Other errors
            cannot_be_referenced, //
            invalid_get, //
            invalid_get_fn, //
            invalid_call, //
            invalid_partial_key, //
            name_mismatch, //
            array_element_not_found, //
            elements_not_convertible_to_design, //
            unknown_distribution, //
            invalid_distribution_elements, //
            hostname_failure, //
            includer_error //
   );

   struct error_ctx final
   {
      error_code ec{};
      std::string_view custom_error_message{};
      // INTERNAL USE:
      size_t location{};
      std::string_view includer_error{}; // error from a nested file includer

      operator bool() const { return ec != error_code::none; }

      bool operator==(const error_code e) const { return ec == e; }
   };

   // Runtime context for configuration
   // We do not template the context on iterators so that it can be easily shared across buffer implementations
   struct context final
   {
      error_code error{};
      std::string_view custom_error_message{};
      // INTERNAL USE:
      uint32_t indentation_level{}; // When writing this is the number of indent character to serialize
      // When reading indentation_level is used to track the depth of structures to prevent stack overflows
      // From massive depths due to untrusted inputs or attacks
      std::string current_file; // top level file path
      std::string_view includer_error{}; // error from a nested file includer
   };

   template <class T>
   concept is_context = std::same_as<std::decay_t<T>, context>;
}
