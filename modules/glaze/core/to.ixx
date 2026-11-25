// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/core/to.hpp"
// glz:header std=<cstdint>
// glz:header std=<utility>
export module glaze.core.to;

import std;

import glaze.core.filesystem_path;

import glaze.core.common;
import glaze.core.context;
import glaze.core.opts;
import glaze.concepts.container_concepts;
import glaze.util.type_traits;

// Common behavior for `to` specializations, typically applies for all formats

using std::uint32_t;

namespace glz
{
   template <uint32_t Format>
   struct to<Format, hidden>
   {
      template <auto Opts>
      static void op(auto&& value, auto&&...) noexcept
      {
         static_assert(false_v<decltype(value)>, "hidden type should not be written");
      }
   };

   template <uint32_t Format>
   struct to<Format, skip>
   {
      template <auto Opts>
      static void op(auto&& value, auto&&...) noexcept
      {
         static_assert(false_v<decltype(value)>, "skip type should not be written");
      }
   };

   template <uint32_t Format, filesystem_path T>
   struct to<Format, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         if constexpr (detail::wide_native_path<T>) {
            // native() is UTF-16 on Windows; string() would go through the ANSI code page
            std::string utf8{};
            if (!detail::utf16_to_utf8(value.native(), utf8)) {
               ctx.error = error_code::invalid_utf8;
               return;
            }
            to<Format, std::string>::template op<Opts>(utf8, ctx, std::forward<Args>(args)...);
         }
         else {
            to<Format, decltype(value.string())>::template op<Opts>(value.string(), ctx, std::forward<Args>(args)...);
         }
      }
   };
}
