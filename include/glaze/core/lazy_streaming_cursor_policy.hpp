// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace glz
{
   // ============================================================================
   // Lazy JSON streaming cursor policy
   // ============================================================================
   //
   // Controls whether lazy_iterator::operator++ can jump past a value extent
   // recorded by read_into (or a finished container iterator), and how that
   // extent is stored on lazy_document.
   //
   // - disabled: zero storage, zero runtime cost (default)
   // - enabled: OffsetT = size_t (full span)
   // - packed / packed_with_fallback: OffsetT = uint32_t on 64-bit (compact storage);
   //   on 32-bit platforms packed policies are equivalent to enabled (size_t is already 32-bit)
   //
   // ============================================================================

   inline constexpr bool lazy_streaming_packed_is_enabled = sizeof(std::size_t) <= sizeof(std::uint32_t);

   enum struct lazy_streaming_cursor_policy : uint8_t {
      disabled = 0,
      enabled = 1,
      packed = 2,
      packed_with_fallback = 3
   };

   consteval lazy_streaming_cursor_policy get_lazy_streaming_cursor_policy(auto&& Opts)
   {
      if constexpr (requires { Opts.lazy_streaming_cursor; }) {
         using T = std::remove_cvref_t<decltype(Opts.lazy_streaming_cursor)>;
         if constexpr (std::same_as<T, lazy_streaming_cursor_policy>) {
            return Opts.lazy_streaming_cursor;
         }
         else if constexpr (std::same_as<T, bool>) {
            return Opts.lazy_streaming_cursor ? lazy_streaming_cursor_policy::packed_with_fallback
                                              : lazy_streaming_cursor_policy::disabled;
         }
      }
      return lazy_streaming_cursor_policy::disabled;
   }

   consteval bool lazy_streaming_cursor_active(auto&& Opts)
   {
      return get_lazy_streaming_cursor_policy(Opts) != lazy_streaming_cursor_policy::disabled;
   }

   // Opt-out SWAR pre-scan in the lazy skip loops (end-bounded, non-null-terminated
   // buffers only). An absent field or `true` keeps the SWAR pre-scan enabled (the
   // default). Set `lazy_swar_skip = false` on your opts to compile the plain scalar
   // skip path instead — useful for A/B ablation and to reproduce the exact
   // pre-SWAR (upstream-equivalent) skip behavior from the same headers.
   consteval bool lazy_swar_skip_active(auto&& Opts)
   {
      if constexpr (requires { Opts.lazy_swar_skip; }) {
         return Opts.lazy_swar_skip;
      }
      return true;
   }
} // namespace glz
