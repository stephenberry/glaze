// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/convert.hpp"

namespace glz::detail
{
   template <class T, class V = std::remove_cvref_t<T>>
   concept byte_sized = sizeof(T) == 1 && (std::same_as<V, char> || std::same_as<V, std::byte>);

   template <class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE auto data_ptr(T& buffer) noexcept
   {
      if constexpr (has_data<T>) {
         if constexpr (std::same_as<std::decay_t<typename T::value_type>, std::byte>) {
            return reinterpret_cast<char*>(buffer.data());
         }
         else {
            return buffer.data();
         }
      }
      else {
         return buffer;
      }
   }

   template <uint32_t N, class B>
   GLZ_ALWAYS_INLINE void maybe_pad(B& b, size_t& ix) noexcept
   {
      if constexpr (vector_like<B>) {
         if (const auto k = ix + N; k > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, k));
         }
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void maybe_pad(const size_t n, B& b, size_t& ix) noexcept
   {
      if constexpr (vector_like<B>) {
         if (const auto k = ix + n; k > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, k));
         }
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

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(const byte_sized auto c, B& b, size_t& ix) noexcept
   {
      if constexpr (Checked && vector_like<B>) {
         if (ix == b.size()) [[unlikely]] {
            b.resize(b.size() == 0 ? 128 : b.size() * 2);
         }
      }
      assign_maybe_cast(c, b, ix);
      ++ix;
   }

   template <auto c, bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(B& b, size_t& ix) noexcept
   {
      if constexpr (Checked && vector_like<B>) {
         if (ix == b.size()) [[unlikely]] {
            b.resize(b.size() == 0 ? 128 : b.size() * 2);
         }
      }
      assign_maybe_cast<c>(b, ix);
      ++ix;
   }

   template <string_literal str, bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(B& b, size_t& ix) noexcept
   {
      static constexpr auto s = str.sv();
      static constexpr auto n = s.size();

      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }
         }
      }
      std::memcpy(&b[ix], s.data(), n);
      ix += n;
   }

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(const sv str, B& b, size_t& ix) noexcept
   {
      const auto n = str.size();
      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }
         }
      }
      std::memcpy(&b[ix], str.data(), n);
      ix += n;
   }

   template <auto c, class B>
   GLZ_ALWAYS_INLINE void dumpn(size_t n, B& b, size_t& ix) noexcept
   {
      if constexpr (vector_like<B>) {
         if (ix + n > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, ix + n));
         }
      }
      std::memset(&b[ix], c, n);
      ix += n;
   }

   template <auto c, class B>
   GLZ_ALWAYS_INLINE void dumpn_unchecked(size_t n, B& b, size_t& ix) noexcept
   {
      std::memset(&b[ix], c, n);
      ix += n;
   }

   template <char IndentChar, class B>
   GLZ_ALWAYS_INLINE void dump_newline_indent(size_t n, B& b, size_t& ix) noexcept
   {
      if constexpr (vector_like<B>) {
         if (const auto k = ix + n + write_padding_bytes; k > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, k));
         }
      }

      assign_maybe_cast<'\n'>(b, ix);
      ++ix;
      std::memset(&b[ix], IndentChar, n);
      ix += n;
   }

   template <const sv& str, bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump(B& b, size_t& ix) noexcept
   {
      static constexpr auto s = str;
      static constexpr auto n = s.size();

      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }
         }
      }
      std::memcpy(&b[ix], s.data(), n);
      ix += n;
   }

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump_not_empty(const sv str, B& b, size_t& ix) noexcept
   {
      const auto n = str.size();
      if constexpr (vector_like<B>) {
         if constexpr (Checked) {
            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }
         }
      }
      std::memcpy(&b[ix], str.data(), n);
      ix += n;
   }

   template <bool Checked = true, class B>
   GLZ_ALWAYS_INLINE void dump_maybe_empty(const sv str, B& b, size_t& ix) noexcept
   {
      const auto n = str.size();
      if (n) {
         if constexpr (vector_like<B>) {
            if constexpr (Checked) {
               if (ix + n > b.size()) [[unlikely]] {
                  b.resize((std::max)(b.size() * 2, ix + n));
               }
            }
         }
         std::memcpy(&b[ix], str.data(), n);
         ix += n;
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(const vector_like auto& bytes, B& b, size_t& ix) noexcept
   {
      const auto n = bytes.size();
      if constexpr (vector_like<B>) {
         if (ix + n > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, ix + n));
         }
      }
      std::memcpy(&b[ix], bytes.data(), n);
      ix += n;
   }

   template <size_t N, class B>
   GLZ_ALWAYS_INLINE void dump(const std::array<uint8_t, N>& bytes, B& b, size_t& ix) noexcept
   {
      if constexpr (vector_like<B>) {
         if (ix + N > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, ix + N));
         }
      }
      std::memcpy(&b[ix], bytes.data(), N);
      ix += N;
   }
}
