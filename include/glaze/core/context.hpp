// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <iterator>
#include <string_view>

#include "glaze/util/validate.hpp"

namespace glz
{
   enum class error_code : uint32_t {
      none,
      no_read_input,
      data_must_be_null_terminated,
      parse_number_failure,
      expected_brace,
      expected_bracket,
      expected_quote,
      exceeded_static_array_size,
      unexpected_end,
      expected_end_comment,
      syntax_error,
      key_not_found,
      unexpected_enum,
      attempt_const_read,
      attempt_member_func_read,
      attempt_read_hidden,
      invalid_nullable_read,
      invalid_variant_object,
      invalid_variant_array,
      invalid_variant_string,
      no_matching_variant_type,
      expected_true_or_false,
      unknown_key,
      invalid_flag_input,
      invalid_escape,
      u_requires_hex_digits,
      file_extension_not_supported,
      could_not_determine_extension,
      seek_failure,
      unicode_escape_conversion_failure,
      file_open_failure,
      file_include_error,
      dump_int_error,
      get_nonexistent_json_ptr,
      get_wrong_type,
      cannot_be_referenced,
      invalid_get,
      invalid_get_fn,
      invalid_call,
      invalid_partial_key,
      name_mismatch,
      array_element_not_found,
      elements_not_convertible_to_design,
      unknown_distribution,
      invalid_distribution_elements,
      missing_key
   };

   struct parse_error final
   {
      error_code ec{};
      size_t location{};

      operator bool() const { return ec != error_code::none; }

      bool operator==(const error_code e) const { return ec == e; }
   };

   struct write_error final
   {
      error_code ec{};

      operator bool() const { return ec != error_code::none; }

      bool operator==(const error_code e) const { return ec == e; }
   };

   // Runtime context for configuration
   // We do not template the context on iterators so that it can be easily shared across buffer implementations
   struct context final
   {
      // INTERNAL USE
      uint32_t indentation_level{};
      std::string current_file; // top level file path
      error_code error{};
   };

   template <class T>
   concept is_context = std::same_as<std::decay_t<T>, context>;
}
