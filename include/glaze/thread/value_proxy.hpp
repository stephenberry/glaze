// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

// Serializing a value proxy is transparent: it reads and writes exactly as its element type
// does, in whatever format is asked for.
//
// `is_value_proxy` is declared in glaze/core/common.hpp so that the core concepts
// (`nullable_t` in particular) can exclude proxies from their own matches.

namespace glz
{
   template <uint32_t Format, is_value_proxy T>
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         parse<Format>::template op<Opts>(value.value(), ctx, args...);
      }

      // Formats whose readers consume the header before dispatching pass the tag ahead of the
      // context (MSGPACK always, BEVE under no_header). Hand the element's reader the same tag
      // and options this one was given, exactly as the format's dispatcher would have.
      //
      // This dispatches to `from` rather than to `parse` like the overload above, because
      // `parse<MSGPACK>`'s tag overload requires no_header while MSGPACK's own dispatcher passes
      // tags with it unset; turning it on here instead would leak into the element's nested
      // reads, which is how a proxy holding a struct would break.
      template <auto Opts, class Tag>
         requires(not is_context<std::remove_cvref_t<Tag>>)
      static void op(auto&& value, Tag&& tag, is_context auto&& ctx, auto&&... args)
      {
         using V = std::remove_cvref_t<decltype(value.value())>;
         from<Format, V>::template op<Opts>(value.value(), tag, ctx, args...);
      }
   };

   template <uint32_t Format, is_value_proxy T>
   struct to<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         serialize<Format>::template op<Opts>(value.value(), ctx, args...);
      }
   };
}
