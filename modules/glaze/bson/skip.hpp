// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

#include "glaze/bson/header.hpp"
#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/inline.hpp"

// Skip a single BSON value given its element type byte. Used by the struct
// and map readers to drop over unknown keys, and by the deprecated-type
// fallbacks (undefined / dbpointer / symbol / code_w_scope) that we can read
// but choose not to materialise.

namespace glz::bson_detail
{
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool skip_bytes(is_context auto& ctx, It& it, const End& end, size_t n) noexcept
   {
      if (static_cast<size_t>(end - it) < n) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return false;
      }
      it += n;
      return true;
   }

   // Read a little-endian int32 without advancing (used for length prefixes
   // when the consumer needs the length before deciding what to do with the
   // body).
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool peek_le_int32(is_context auto& ctx, It it, const End& end, int32_t& out) noexcept
   {
      if ((end - it) < 4) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return false;
      }
      int32_t v{};
      std::memcpy(&v, it, 4);
      if constexpr (std::endian::native == std::endian::big) {
         v = static_cast<int32_t>(std::byteswap(static_cast<uint32_t>(v)));
      }
      out = v;
      return true;
   }

   template <class It, class End>
   GLZ_ALWAYS_INLINE bool read_le_int32(is_context auto& ctx, It& it, const End& end, int32_t& out) noexcept
   {
      if (!peek_le_int32(ctx, it, end, out)) [[unlikely]] {
         return false;
      }
      it += 4;
      return true;
   }

   // Advance `it` past a null-terminated cstring. On success, it points just
   // past the 0x00 terminator.
   template <class It, class End>
   GLZ_ALWAYS_INLINE bool skip_cstring(is_context auto& ctx, It& it, const End& end) noexcept
   {
      while (it < end) {
         if (*it == 0) {
            ++it;
            return true;
         }
         ++it;
      }
      ctx.error = error_code::unexpected_end;
      return false;
   }
}

namespace glz
{
   // BSON skip: the type byte is supplied by the caller (the document/array
   // reader) because BSON values do not carry a self-describing header — the
   // element tag lives outside the value.
   template <>
   struct skip_value<BSON>
   {
      template <auto Opts, class It, class End>
      static void op(uint8_t type_byte, is_context auto& ctx, It& it, const End& end) noexcept
      {
         using namespace bson;
         switch (type_byte) {
         case type::null:
         case type::undefined:
         case type::min_key:
         case type::max_key:
            return;

         case type::boolean:
            (void)bson_detail::skip_bytes(ctx, it, end, 1);
            return;

         case type::int32:
            (void)bson_detail::skip_bytes(ctx, it, end, 4);
            return;

         case type::double_:
         case type::datetime:
         case type::timestamp:
         case type::int64:
            (void)bson_detail::skip_bytes(ctx, it, end, 8);
            return;

         case type::object_id:
            (void)bson_detail::skip_bytes(ctx, it, end, 12);
            return;

         case type::decimal128:
            (void)bson_detail::skip_bytes(ctx, it, end, 16);
            return;

         case type::string:
         case type::javascript:
         case type::symbol: {
            int32_t len{};
            if (!bson_detail::read_le_int32(ctx, it, end, len)) return;
            if (len < 1) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            (void)bson_detail::skip_bytes(ctx, it, end, static_cast<size_t>(len));
            return;
         }

         case type::document:
         case type::array: {
            // The length prefix *includes* itself. Skip the rest, plus
            // the trailing null byte that the prefix already accounts for.
            int32_t len{};
            if (!bson_detail::peek_le_int32(ctx, it, end, len)) return;
            if (len < 5) [[unlikely]] {
               // Minimum: 4-byte length + 0x00 terminator.
               ctx.error = error_code::syntax_error;
               return;
            }
            (void)bson_detail::skip_bytes(ctx, it, end, static_cast<size_t>(len));
            return;
         }

         case type::binary: {
            int32_t len{};
            if (!bson_detail::read_le_int32(ctx, it, end, len)) return;
            if (len < 0) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            // One-byte subtype follows, then `len` payload bytes.
            (void)bson_detail::skip_bytes(ctx, it, end, 1 + static_cast<size_t>(len));
            return;
         }

         case type::regex:
            if (!bson_detail::skip_cstring(ctx, it, end)) return;
            if (!bson_detail::skip_cstring(ctx, it, end)) return;
            return;

         case type::db_pointer: {
            // String (length-prefixed UTF-8 + null) + 12-byte ObjectId.
            int32_t len{};
            if (!bson_detail::read_le_int32(ctx, it, end, len)) return;
            if (len < 1) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            if (!bson_detail::skip_bytes(ctx, it, end, static_cast<size_t>(len) + 12)) return;
            return;
         }

         case type::code_w_scope: {
            // Total int32 length includes itself, the string, and the scope
            // document. Skip the whole thing in one go.
            int32_t len{};
            if (!bson_detail::peek_le_int32(ctx, it, end, len)) return;
            if (len < 4) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            (void)bson_detail::skip_bytes(ctx, it, end, static_cast<size_t>(len));
            return;
         }

         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   };
}
