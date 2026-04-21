// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/jsonb/header.hpp"

namespace glz
{
   template <>
   struct skip_value<JSONB>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(is_context auto& ctx, auto& it, auto end) noexcept
      {
         uint8_t type_code{};
         uint64_t payload_size{};
         if (!jsonb::read_header(ctx, it, end, type_code, payload_size)) {
            return;
         }
         if (static_cast<uint64_t>(end - it) < payload_size) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         it += payload_size;
      }
   };
}
