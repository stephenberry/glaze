// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <glaze/core/opts.hpp>

using std::uint8_t;
using std::uint32_t;

namespace glz::yaml
{

   enum struct opts_internal : uint32_t {
      none = 0,
      flow_context = 1 << 0, // Currently in flow context (no indentation rules)
   };

   struct yaml_opts
   {
      uint32_t format = YAML;
      bool error_on_unknown_keys{true};
      bool error_on_missing_keys{false}; // Require all non-nullable keys to be present
      bool skip_null_members{true}; // Skip writing null members
      uint8_t indent_width{2}; // Spaces per indent level for writing
      bool flow_style{false}; // Use flow style (compact) for output

      uint32_t internal{}; // default to 0
   };

   consteval bool check_flow_context(auto&& o) { return o.internal & uint32_t(opts_internal::flow_context); }

   template <auto Opts>
   constexpr auto flow_context_on()
   {
      auto ret = Opts;
      ret.internal |= uint32_t(opts_internal::flow_context);
      return ret;
   }

   template <auto Opts>
   constexpr auto flow_context_off()
   {
      auto ret = Opts;
      ret.internal &= ~uint32_t(opts_internal::flow_context);
      return ret;
   }

   // Takes Opts as a template parameter (like csv_delimiter) so the value can be validated: too
   // small a width writes nested block content at or before its parent's column, which is not the
   // same document and does not read back.
   template <auto Opts>
   consteval uint8_t check_indent_width()
   {
      if constexpr (requires { Opts.indent_width; }) {
         static_assert(Opts.indent_width >= 2,
                       "YAML indent_width must be at least 2: a sequence dash takes one column and "
                       "needs at least one space before same-line content");
         return Opts.indent_width;
      }
      else {
         return 2;
      }
   }

   consteval bool check_flow_style(auto&& o)
   {
      if constexpr (requires { o.flow_style; }) {
         return o.flow_style;
      }
      else {
         return false;
      }
   }

} // namespace glz::yaml

