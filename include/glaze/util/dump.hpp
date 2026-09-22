// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/core/buffer_traits.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/convert.hpp"

namespace glz
{
   template <class T, class V = std::remove_cvref_t<T>>
   concept byte_sized = sizeof(T) == 1 && (std::same_as<V, char> || std::same_as<V, std::byte>);

   template <uint32_t N, class B>
   GLZ_ALWAYS_INLINE void maybe_pad(B& b, size_t ix) noexcept(not vector_like<B>)
   {
      if constexpr (vector_like<B>) {
         if (const auto k = ix + N; k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void maybe_pad(const size_t n, B& b, size_t ix) noexcept(not vector_like<B>)
   {
      if constexpr (vector_like<B>) {
         if (const auto k = ix + n; k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }
   }

   // Fills n bytes with c, without the call into memset.
   //
   // Every caller is writing indentation, which is a handful of bytes at the depths real documents
   // reach -- and a call whose length the callee cannot see costs more than the stores it makes.
   // Overlapping stores cover any short length in one or two instructions and never touch a byte
   // outside [dst, dst + n), so this asks nothing of callers that memset did not. Indentation deep
   // enough to be worth vectorizing goes back to memset.
   GLZ_ALWAYS_INLINE void fill_bytes(auto* dst, const char c, const size_t n) noexcept
   {
      auto* p = reinterpret_cast<char*>(dst);
      if (n >= 64) [[unlikely]] {
         std::memset(p, c, n);
         return;
      }
      const uint64_t v = 0x0101010101010101ull * uint8_t(c);
      if (n >= 8) {
         size_t i = 0;
         for (; i + 8 <= n; i += 8) {
            std::memcpy(p + i, &v, 8);
         }
         if (i < n) {
            std::memcpy(p + n - 8, &v, 8); // overlaps what is already written, stays within n
         }
      }
      else if (n >= 4) {
         std::memcpy(p, &v, 4);
         std::memcpy(p + n - 4, &v, 4);
      }
      else if (n) {
         std::memcpy(p, &v, 1);
         std::memcpy(p + (n >> 1), &v, 1);
         std::memcpy(p + n - 1, &v, 1);
      }
   }

   template <auto c>
   GLZ_ALWAYS_INLINE void assign_maybe_cast(auto& b, size_t& ix) noexcept
   {
      using V = std::decay_t<decltype(b[0])>;
      using C = std::decay_t<decltype(c)>;
      if constexpr (std::same_as<V, C>) {
         b[ix] = c;
      }
      else {
         b[ix] = static_cast<V>(c);
      }
   }

   GLZ_ALWAYS_INLINE void assign_maybe_cast(const byte_sized auto c, auto& b, size_t& ix) noexcept
   {
      using V = std::decay_t<decltype(b[0])>;
      using C = std::decay_t<decltype(c)>;
      if constexpr (std::same_as<V, C>) {
         b[ix] = c;
      }
      else {
         b[ix] = static_cast<V>(c);
      }
   }

   // Buffers whose data() maps to a nonzero logical position (streaming buffers that flush a prefix
   // and slide their window) report that offset here, so that data_at() can translate a logical
   // index into physical storage the way operator[] already does.
   template <class T>
   concept has_data_offset = requires(const T& t) { t.data_offset(); };

   // The address of the write position, formed without subscripting so that a full buffer
   // (ix == size()) yields a one-past-the-end pointer instead of an out of range access. A memset or
   // memcpy of zero bytes through that pointer is well defined, so a length of zero needs no branch.
   template <class B>
   GLZ_ALWAYS_INLINE auto data_at(B& b, const size_t ix) noexcept
   {
      if constexpr (std::is_pointer_v<std::remove_cvref_t<B>>) {
         return b + ix;
      }
      else {
         static_assert(has_data<std::remove_cvref_t<B>>,
                       "an output buffer must be contiguous: dump writes through memset and memcpy");
         if constexpr (has_data_offset<std::remove_cvref_t<B>>) {
            assert(ix >= b.data_offset() && "Index before flush offset");
            return b.data() + (ix - b.data_offset());
         }
         else {
            return b.data() + ix;
         }
      }
   }

   // Low-level buffer write primitives (dump functions)
   // ================================================
   // These functions write directly to the buffer WITHOUT bounds checking for bounded buffers.
   //
   // Contract:
   // - For resizable buffers (std::string, std::vector): Auto-resizes when Checked=true (default)
   // - For bounded buffers (std::array, std::span): Caller MUST call ensure_space() first
   // - For raw pointers: Caller assumes responsibility for sufficient space
   //
   // The Checked template parameter ONLY enables auto-resize for resizable buffers.
   // It does NOT enable bounds checking for bounded buffers - that would require ctx for error reporting
   // and would duplicate the ensure_space() logic. The padding model (ensure_space checks for
   // ix + n + write_padding_bytes) guarantees sufficient space for subsequent dump calls.

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(const byte_sized auto c, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if constexpr (Checked && vector_like<B>) {
         if (ix == b.size()) [[unlikely]] {
            grow_buffer(b, b.size() == 0 ? 64 : ix + 1);
         }
      }
      assign_maybe_cast(c, b, ix);
      ++ix;
   }

   template <auto c, bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if constexpr (Checked && vector_like<B>) {
         if (ix == b.size()) [[unlikely]] {
            grow_buffer(b, b.size() == 0 ? 64 : ix + 1);
         }
      }
      assign_maybe_cast<c>(b, ix);
      ++ix;
   }

   template <string_literal str, bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      static constexpr auto s = str.sv();
      static constexpr auto n = s.size();

      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            const auto k = ix + n;
            if (k > b.size()) [[unlikely]] {
               grow_buffer(b, k);
            }
         }
      }
      std::memcpy(&b[ix], s.data(), n);
      ix += n;
   }

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(const sv str, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      const auto n = str.size();
      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            const auto k = ix + n;
            if (ix + n > b.size()) [[unlikely]] {
               grow_buffer(b, k);
            }
         }
      }
      std::memcpy(&b[ix], str.data(), n);
      ix += n;
   }

   template <auto c, class B>
   [[deprecated("use dumpn(c, n, b, ix) instead of dumpn<c>(n, b, ix) to reduce template instantiations")]]
   GLZ_ALWAYS_INLINE void dumpn(size_t n, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if constexpr (vector_like<B>) {
         const auto k = ix + n;
         if (k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }
      fill_bytes(data_at(b, ix), c, n);
      ix += n;
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dumpn(const byte_sized auto c, size_t n, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if constexpr (vector_like<B>) {
         const auto k = ix + n;
         if (k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }
      fill_bytes(data_at(b, ix), c, n);
      ix += n;
   }

   template <auto c, class B>
   [[deprecated(
      "use dumpn_unchecked(c, n, b, ix) instead of dumpn_unchecked<c>(n, b, ix) to reduce template instantiations")]]
   GLZ_ALWAYS_INLINE void dumpn_unchecked(size_t n, B& b, size_t& ix) noexcept
   {
      fill_bytes(data_at(b, ix), c, n);
      ix += n;
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dumpn_unchecked(const byte_sized auto c, size_t n, B& b, size_t& ix) noexcept
   {
      fill_bytes(data_at(b, ix), c, n);
      ix += n;
   }

   template <char IndentChar, class B>
   [[deprecated(
      "use dump_newline_indent(c, n, b, ix) instead of dump_newline_indent<c>(n, b, ix) to reduce template "
      "instantiations")]]
   GLZ_ALWAYS_INLINE void dump_newline_indent(size_t n, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if constexpr (vector_like<B>) {
         if (const auto k = ix + n + write_padding_bytes; k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }

      assign_maybe_cast<'\n'>(b, ix);
      ++ix;
      fill_bytes(data_at(b, ix), IndentChar, n);
      ix += n;
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump_newline_indent(const byte_sized auto c, size_t n, B& b,
                                              size_t& ix) noexcept(not vector_like<B>)
   {
      if constexpr (vector_like<B>) {
         if (const auto k = ix + n + write_padding_bytes; k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }

      assign_maybe_cast('\n', b, ix);
      ++ix;
      fill_bytes(data_at(b, ix), c, n);
      ix += n;
   }

   template <const sv& str, bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(B& b, size_t& ix) noexcept(not vector_like<B> && not Checked)
   {
      static constexpr auto s = str;
      static constexpr auto n = s.size();

      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            const auto k = ix + n;
            if (k > b.size()) [[unlikely]] {
               grow_buffer(b, k);
            }
         }
      }
      std::memcpy(&b[ix], s.data(), n);
      ix += n;
   }

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump_not_empty(const sv str, B& b, size_t& ix) noexcept(not vector_like<B> && not Checked)
   {
      const auto n = str.size();
      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            const auto k = ix + n;
            if (k > b.size()) [[unlikely]] {
               grow_buffer(b, k);
            }
         }
      }
      std::memcpy(&b[ix], str.data(), n);
      ix += n;
   }

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump_maybe_empty(const sv str, B& b, size_t& ix) noexcept(not vector_like<B> && not Checked)
   {
      const auto n = str.size();
      if (n) {
         if constexpr (vector_like<B>) {
            if constexpr (Checked) {
               const auto k = ix + n;
               if (k > b.size()) [[unlikely]] {
                  grow_buffer(b, k);
               }
            }
         }
         std::memcpy(&b[ix], str.data(), n);
         ix += n;
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(const vector_like auto& bytes, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      const auto n = bytes.size();
      if constexpr (vector_like<B>) {
         const auto k = ix + n;
         if (k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }
      std::memcpy(&b[ix], bytes.data(), n);
      ix += n;
   }

   template <size_t N, class B>
   GLZ_ALWAYS_INLINE void dump(const std::array<uint8_t, N>& bytes, B& b, size_t& ix) noexcept(not vector_like<B>)
   {
      if constexpr (vector_like<B>) {
         const auto k = ix + N;
         if (k > b.size()) [[unlikely]] {
            grow_buffer(b, k);
         }
      }
      std::memcpy(&b[ix], bytes.data(), N);
      ix += N;
   }
}
