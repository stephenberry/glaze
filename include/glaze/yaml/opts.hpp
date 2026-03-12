// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <glaze/core/opts.hpp>

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

   consteval uint8_t check_indent_width(auto&& o)
   {
      if constexpr (requires { o.indent_width; }) {
         return o.indent_width;
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
