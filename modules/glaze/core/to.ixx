// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.core.to;

import std;

import glaze.core.common;
import glaze.core.context;
import glaze.core.opts;
import glaze.concepts.container_concepts;
import glaze.util.type_traits;

// Common behavior for `to` specializations, typically applies for all formats

namespace glz
{
   template <std::uint32_t Format>
   struct to<Format, hidden>
   {
      template <auto Opts>
      static void op(auto&& value, auto&&...) noexcept
      {
         static_assert(false_v<decltype(value)>, "hidden type should not be written");
      }
   };

   template <std::uint32_t Format>
   struct to<Format, skip>
   {
      template <auto Opts>
      static void op(auto&& value, auto&&...) noexcept
      {
         static_assert(false_v<decltype(value)>, "skip type should not be written");
      }
   };

   export template <std::uint32_t Format, filesystem_path T>
   struct to<Format, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         to<Format, decltype(value.string())>::template op<Opts>(value.string(), ctx, std::forward<Args>(args)...);
      }
   };
}
