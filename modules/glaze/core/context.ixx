// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/core/context.hpp"
// glz:header std=<array>
// glz:header std=<concepts>
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<iterator>
// glz:header std=<string>
// glz:header std=<string_view>
// glz:header std=<system_error>
export module glaze.core.context;

import std;
import glaze.core.meta_fwd;

#include "glaze/util/inline.hpp"

using std::size_t;
using std::uint32_t;

namespace glz
{
   export inline constexpr size_t max_recursive_depth_limit = 256;

   export enum struct error_code : uint32_t {
      // REPE compliant error codes
      none, //
      version_mismatch,
      invalid_header,
      invalid_query,
      invalid_body,
      parse_error,
      method_not_found,
      timeout,
      send_error,
      connection_failure,
      // Other errors
      // Internal to a non-null-terminated read: "the buffer ran out here", which is a completed
      // read or a truncated one depending on the nesting depth. settle_end_reached decides which
      // and replaces it with none or unexpected_end, so every entry point that finalizes its
      // context -- glz::read, read_streaming, repe::read_params -- has stopped returning it.
      //
      // Two paths still hand it back. json_stream_reader raises it deliberately, to signal end of
      // stream. read_jmespath and get_view_json return ctx.error without finalizing at all; they
      // navigate by matching braces rather than by entering depth, so ctx.depth stays zero
      // throughout and settle_end_reached cannot be dropped in as-is -- it would settle their
      // truncated reads to none, which is worse than leaking. Those need their own truncation
      // accounting before they can join.
      end_reached,
      partial_read_complete, // A non-error code for short circuiting partial reads
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
      constraint_violated, //
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
      nonexistent_json_ptr, //
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
      includer_error, //
      // Feature support
      feature_not_supported, //
      // JSON Pointer errors (RFC 6901)
      invalid_json_pointer, // Malformed JSON pointer syntax (e.g., "~" at end, "~2")
      // JSON Patch errors (RFC 6902)
      patch_test_failed, // Test operation value mismatch (unique to RFC 6902 test op)
      // Buffer errors
      buffer_overflow, // Write would exceed fixed buffer capacity
      invalid_length, // Length exceeds allowed limit (buffer size or user-configured max)
      // Encoding errors
      invalid_utf8, // Malformed UTF-8 in a string; checked on read unless validate_utf8 is disabled
      invalid_control_character, // A string carries a control character with no two-character JSON
      // escape, and escape_control_characters is off so it cannot be
      // written. Raised by the binary-to-JSON converters, which refuse
      // to emit a byte they would have to corrupt or hide.
      // Streaming errors
      streaming_unsupported, // Document outruns the buffer window and this format's reader cannot refill
      // Expansion errors
      // A YAML read produced more text than its budgets allow. Both budgets bound the same thing
      // -- text a small document can multiply into an unbounded amount -- and both call for the
      // same response, so they share a code; custom_error_message names which one ran away.
      exceeded_max_expansion
   };

   // Unified error context for all read/write operations
   // Provides error information and byte count processed
   export struct error_ctx final
   {
      size_t count{}; // Bytes processed (read or written)
      error_code ec{}; // Error code (none on success)
      std::string_view custom_error_message{}; // Human-readable error context

      // Returns true when there IS an error (matches std::error_code semantics)
      operator bool() const noexcept { return ec != error_code::none; }

      bool operator==(const error_code e) const noexcept { return ec == e; }
   };

   // Runtime context for configuration
   // We do not template the context on iterators so that it can be easily shared across buffer implementations
   // Not final: streaming_context inherits from this to add streaming-specific state
   export struct context
   {
      error_code error{};
      std::string_view custom_error_message;
      // INTERNAL USE:
      size_t speculation_budget{}; // Bytes of speculative (variant alternative) re-parsing left;
      // seeded per read from the input size, 0 means unlimited. See charge_speculation below.
      uint32_t depth{}; // Nesting depth of structures (objects/arrays)
      // Used for indentation when writing and for stack overflow protection when reading
      std::string current_file; // top level file path
      // NOTE: The default constructor is valid for std::string_view, so we use this rather than {}
      // because debuggers like jumping to std::string_view initialization calls
      std::string scratch{};
   };

   // Concept for any context type (base or streaming)
   export template <class T>
   concept is_context = requires(T& ctx) {
      { ctx.error } -> std::same_as<error_code&>;
      { ctx.depth } -> std::same_as<uint32_t&>;
   };

   // RAII guard for recursive descent: bumps ctx.depth on entry and restores it on exit,
   // erroring out before the nesting reaches max_recursive_depth_limit so adversarial deeply
   // nested input can't overflow the stack. Mirrors the per-reader guards already used by the
   // BSON and JSONB binary readers; placed here so the text-format readers can share one copy.
   export template <class Ctx>
   struct depth_guard
   {
      Ctx& ctx;
      bool entered = false;

      depth_guard(Ctx& c) noexcept : ctx(c)
      {
         if (ctx.depth >= max_recursive_depth_limit) [[unlikely]] {
            ctx.error = error_code::exceeded_max_recursive_depth;
            return;
         }
         ++ctx.depth;
         entered = true;
      }
      ~depth_guard()
      {
         if (entered) --ctx.depth;
      }
      explicit operator bool() const noexcept { return entered; }
   };

   // A variant read is speculative: an alternative is parsed to find out whether it fits, and a
   // rejected one is rewound and the next tried. Nest that -- a variant whose alternatives contain
   // variants -- and the re-parses multiply, so a failure at the bottom of an ambiguous nest costs
   // branching^depth. Measured at ~4.3x per level, which turns 189 bytes into 55 seconds.
   //
   // The bound is on total work rather than on nesting or attempts, because the honest measure is
   // how many bytes get parsed more than once. A wide document (many variants side by side, each
   // rejecting an alternative first) re-parses each element once and stays near 1x its own size,
   // while the exponential case blows through any multiple of the input immediately. Reads that
   // exhaust the budget stop speculating and report the failure they already have -- which is the
   // outcome of a cascade anyway, since it only runs when nothing fits.
   export inline constexpr size_t max_speculative_parse_factor = 8;

   // ...plus a floor, because the factor alone is the wrong shape for small inputs. Resolving a
   // genuinely ambiguous NESTED variant -- alternatives that differ only in a late member, so the
   // rejected one parses most of the subtree before failing -- costs exponentially more per level
   // even when the document is valid and the read ultimately succeeds. Glaze has always been like
   // this; the bound only decides where it stops. Without a floor, a 265 byte document that read
   // correctly in 9 ms would now fail, which is not an improvement. A megabyte of speculative bytes
   // covers that shape to 18 levels, and gives up at 20, which is where the unbounded behavior was
   // already taking a quarter of a second and doubling every two levels. The adversarial case costs
   // a constant ~8 ms at any depth instead of 55 seconds at depth 12 and forever beyond that.
   export inline constexpr size_t min_speculative_parse_bytes = 1 << 20;

   // Charge `consumed` bytes of speculative parsing. Returns false once the budget is spent, at which
   // point the caller must stop trying alternatives. A zero budget means unlimited: a context that
   // never went through glz::read (a nested or hand-rolled parse) is not policed.
   export [[nodiscard]] GLZ_ALWAYS_INLINE bool charge_speculation(is_context auto& ctx, const size_t consumed) noexcept
   {
      if constexpr (requires { ctx.speculation_budget; }) {
         if (ctx.speculation_budget == 0) {
            return true;
         }
         ctx.speculation_budget = consumed >= ctx.speculation_budget ? 1 : ctx.speculation_budget - consumed;
         return ctx.speculation_budget > 1;
      }
      else {
         return true;
      }
   }

   // True once this read has spent its speculation budget. A stopped resolution must not then be
   // reinterpreted as success by the non-null-terminated readers' "the iterator advanced, so
   // something parsed" heuristic: what advanced it was an attempt that was abandoned, not accepted.
   export [[nodiscard]] GLZ_ALWAYS_INLINE bool speculation_exhausted(is_context auto& ctx) noexcept
   {
      if constexpr (requires { ctx.speculation_budget; }) {
         return ctx.speculation_budget == 1; // the latched floor; 0 means unbudgeted
      }
      else {
         return false;
      }
   }

   // Manual counterpart of depth_guard for the JSON/NDJSON readers described above: counts one
   // nesting level and returns true when the limit is reached, in which case the caller must return
   // immediately. The level is given back with a plain `--ctx.depth` at each syntactic close, so a
   // bail-out leaves it counted, which is what keeps a truncated buffer from finalizing as success.
   // Readers that rewind and retry (variant alternatives) must restore ctx.depth with the iterator.
   export [[nodiscard]] GLZ_ALWAYS_INLINE bool enter_depth(is_context auto& ctx) noexcept
   {
      if (ctx.depth >= max_recursive_depth_limit) [[unlikely]] {
         ctx.error = error_code::exceeded_max_recursive_depth;
         return true;
      }
      ++ctx.depth;
      return false;
   }

   // Runtime constraint concepts
   // These detect if a user-defined context has runtime constraint fields.
   // Users can inherit from glz::context and add these fields for runtime limits:
   //   struct my_context : glz::context {
   //      std::size_t max_string_length = 1024;
   //      std::size_t max_array_size = 100;
   //      std::size_t max_map_size = 50;
   //      bool allocate_raw_pointers = false;
   //   };
   // Use with if constexpr to ensure zero binary overhead when not used.

   export template <class Ctx>
   concept has_runtime_max_string_length = requires(Ctx& ctx) {
      { ctx.max_string_length } -> std::convertible_to<size_t>;
   };

   export template <class Ctx>
   concept has_runtime_max_array_size = requires(Ctx& ctx) {
      { ctx.max_array_size } -> std::convertible_to<size_t>;
   };

   export template <class Ctx>
   concept has_runtime_max_map_size = requires(Ctx& ctx) {
      { ctx.max_map_size } -> std::convertible_to<size_t>;
   };

   export template <class Ctx>
   concept has_runtime_allocate_raw_pointers = requires(Ctx& ctx) {
      { ctx.allocate_raw_pointers } -> std::convertible_to<bool>;
   };

   template <>
   struct meta<glz::error_code>
   {
      static constexpr std::string_view name = "glz::error_code";
      using enum glz::error_code;
      static constexpr std::array keys{"none",
                                       "version_mismatch",
                                       "invalid_header",
                                       "invalid_query",
                                       "invalid_body",
                                       "parse_error",
                                       "method_not_found",
                                       "timeout",
                                       "send_error",
                                       "connection_failure",
                                       "end_reached",
                                       "partial_read_complete",
                                       "no_read_input",
                                       "data_must_be_null_terminated",
                                       "parse_number_failure",
                                       "expected_brace",
                                       "expected_bracket",
                                       "expected_quote",
                                       "expected_comma",
                                       "expected_colon",
                                       "exceeded_static_array_size",
                                       "exceeded_max_recursive_depth",
                                       "unexpected_end",
                                       "expected_end_comment",
                                       "syntax_error",
                                       "unexpected_enum",
                                       "attempt_const_read",
                                       "attempt_member_func_read",
                                       "attempt_read_hidden",
                                       "invalid_nullable_read",
                                       "invalid_variant_object",
                                       "invalid_variant_array",
                                       "invalid_variant_string",
                                       "no_matching_variant_type",
                                       "expected_true_or_false",
                                       "constraint_violated",
                                       "key_not_found",
                                       "unknown_key",
                                       "missing_key",
                                       "invalid_flag_input",
                                       "invalid_escape",
                                       "u_requires_hex_digits",
                                       "unicode_escape_conversion_failure",
                                       "dump_int_error",
                                       "file_open_failure",
                                       "file_close_failure",
                                       "file_include_error",
                                       "file_extension_not_supported",
                                       "could_not_determine_extension",
                                       "nonexistent_json_ptr",
                                       "get_wrong_type",
                                       "seek_failure",
                                       "cannot_be_referenced",
                                       "invalid_get",
                                       "invalid_get_fn",
                                       "invalid_call",
                                       "invalid_partial_key",
                                       "name_mismatch",
                                       "array_element_not_found",
                                       "elements_not_convertible_to_design",
                                       "unknown_distribution",
                                       "invalid_distribution_elements",
                                       "hostname_failure",
                                       "includer_error",
                                       "feature_not_supported",
                                       "invalid_json_pointer",
                                       "patch_test_failed",
                                       "buffer_overflow",
                                       "invalid_length",
                                       "invalid_utf8",
                                       "invalid_control_character",
                                       "streaming_unsupported",
                                       "exceeded_max_expansion"};
      static constexpr std::array value{none, //
                                        version_mismatch, //
                                        invalid_header, //
                                        invalid_query, //
                                        invalid_body, //
                                        parse_error, //
                                        method_not_found, //
                                        timeout, //
                                        send_error, //
                                        connection_failure, //
                                        end_reached, // A non-error code for non-null terminated input buffers
                                        partial_read_complete,
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
                                        constraint_violated, //
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
                                        nonexistent_json_ptr, //
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
                                        includer_error, //
                                        feature_not_supported, //
                                        // JSON Pointer errors (RFC 6901)
                                        invalid_json_pointer, //
                                        // JSON Patch errors (RFC 6902)
                                        patch_test_failed, //
                                        // Buffer errors
                                        buffer_overflow, //
                                        invalid_length, //
                                        // Encoding errors
                                        invalid_utf8, //
                                        invalid_control_character, //
                                        // Streaming errors
                                        streaming_unsupported, //
                                        // Expansion errors
                                        exceeded_max_expansion};
   };
}
