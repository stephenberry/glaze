// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/bson/read.hpp"
#include "glaze/bson/skip.hpp"
#include "glaze/bson/write.hpp"
#include "glaze/core/custom.hpp"
#include "glaze/core/write_wrappers.hpp"

namespace glz
{
   // BSON needs both halves of glz::custom specialized here rather than relying on the format
   // independent versions in core/custom.hpp:
   //
   //  - readers receive the element tag that the caller already consumed, so it has to be threaded
   //    through to whatever the setter is fed;
   //  - writers must expose `type_code`, the BSON element type byte, which is written before the
   //    key and therefore has to be known from the type alone. For a custom field that is the type
   //    the getter resolves to.
   template <class T>
      requires(is_specialization_v<T, custom_t>)
   struct from<BSON, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end)
      {
         detail::custom_read<Opts, T>(
            value, ctx,
            [&](auto& input) {
               from<BSON, std::decay_t<decltype(input)>>::template op<Opts>(input, tag, ctx, it, end);
            },
            [&] { skip_value<BSON>::template op<Opts>(tag, ctx, it, end); });
      }
   };

   template <class T>
      requires(is_specialization_v<T, custom_t>)
   struct to<BSON, T>
   {
      static constexpr uint8_t type_code = to<BSON, resolve_write_type_t<T>>::type_code;

      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         detail::custom_write<BSON, Opts, T>(value, ctx, args...);
      }
   };
}
