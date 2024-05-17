// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

#include "glaze/concepts/container_concepts.hpp"

namespace glz::detail
{
   template <class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE auto data_ptr(T& buffer) noexcept
   {
      if constexpr (resizable<T>) {
         return buffer.data();
      }
      else {
         return buffer;
      }
   }

   template <uint32_t N>
   GLZ_ALWAYS_INLINE void maybe_pad(vector_like auto& b, auto& ix) noexcept
   {
      if (const auto k = ix + N; k > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, k));
      }
   }

   GLZ_ALWAYS_INLINE void maybe_pad(const size_t n, vector_like auto& b, auto& ix) noexcept
   {
      if (const auto k = ix + n; k > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, k));
      }
   }

   template <uint32_t N>
   GLZ_ALWAYS_INLINE void maybe_pad(char*&, auto&) noexcept
   {}

   GLZ_ALWAYS_INLINE void maybe_pad(const size_t, char*&, auto&) noexcept {}

   GLZ_ALWAYS_INLINE void dump(const char c, vector_like auto& b, auto& ix) noexcept
   {
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() == 0 ? 128 : b.size() * 2);
      }

      b[ix] = c;
      ++ix;
   }

   GLZ_ALWAYS_INLINE void dump(const char c, char*& b) noexcept
   {
      *b = c;
      ++b;
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dump(vector_like auto& b, auto& ix) noexcept
   {
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() == 0 ? 128 : b.size() * 2);
      }

      using V = std::decay_t<decltype(b[0])>;
      if constexpr (std::same_as<V, char>) {
         b[ix] = c;
      }
      else {
         b[ix] = static_cast<V>(c);
      }
      ++ix;
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dump(auto* b, auto& ix) noexcept
   {
      b[ix] = c;
      ++ix;
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dump(char*& b) noexcept
   {
      *b = c;
      ++b;
   }

   template <string_literal str>
   GLZ_ALWAYS_INLINE void dump(char*& b) noexcept
   {
      static constexpr auto s = str.sv();
      static constexpr auto n = s.size();
      std::memcpy(b, s.data(), n);
      b += n;
   }

   template <string_literal str>
   GLZ_ALWAYS_INLINE void dump(vector_like auto& b, auto& ix) noexcept
   {
      static constexpr auto s = str.sv();
      static constexpr auto n = s.size();

      if (ix + n > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + n));
      }

      std::memcpy(b.data() + ix, s.data(), n);
      ix += n;
   }

   template <string_literal str>
   GLZ_ALWAYS_INLINE void dump(auto* b, auto& ix) noexcept
   {
      static constexpr auto s = str.sv();
      static constexpr auto n = s.size();

      std::memcpy(b + ix, s.data(), n);
      ix += n;
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dump_unchecked(vector_like auto& b, auto& ix) noexcept
   {
      using V = std::decay_t<decltype(b[0])>;
      if constexpr (std::same_as<V, char>) {
         b[ix] = c;
      }
      else {
         b[ix] = static_cast<V>(c);
      }
      ++ix;
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dump_unchecked(auto* b, auto& ix) noexcept
   {
      b[ix] = c;
      ++ix;
   }

   GLZ_ALWAYS_INLINE void dump_unchecked(const char c, auto* b, auto& ix) noexcept
   {
      b[ix] = c;
      ++ix;
   }

   GLZ_ALWAYS_INLINE void dump_unchecked(const char c, vector_like auto& b, auto& ix) noexcept
   {
      b[ix] = c;
      ++ix;
   }

   GLZ_ALWAYS_INLINE void dump_unchecked(const sv str, vector_like auto& b, auto& ix) noexcept
   {
      const auto n = str.size();
      std::memcpy(b.data() + ix, str.data(), n);
      ix += n;
   }

   template <string_literal str>
   GLZ_ALWAYS_INLINE void dump_unchecked(vector_like auto& b, auto& ix) noexcept
   {
      static constexpr auto s = str.sv();
      static constexpr auto n = s.size();

      std::memcpy(b.data() + ix, s.data(), n);
      ix += n;
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dumpn(size_t n, char*& b) noexcept
   {
      std::memset(b, c, n);
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dumpn_unchecked(size_t n, char*& b) noexcept
   {
      std::memset(b, c, n);
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dumpn(size_t n, vector_like auto& b, auto& ix) noexcept
   {
      if (ix + n > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + n));
      }

      std::memset(b.data() + ix, c, n);
      ix += n;
   }

   template <char c>
   GLZ_ALWAYS_INLINE void dumpn_unchecked(size_t n, vector_like auto& b, auto& ix) noexcept
   {
      std::memset(b.data() + ix, c, n);
      ix += n;
   }

   template <char IndentChar>
   GLZ_ALWAYS_INLINE void dump_newline_indent(size_t n, vector_like auto& b, auto& ix) noexcept
   {
      if (const auto k = ix + n + write_padding_bytes; k > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, k));
      }

      b[ix] = '\n';
      ++ix;
      std::memset(b.data() + ix, IndentChar, n);
      ix += n;
   }

   template <const sv& str>
   GLZ_ALWAYS_INLINE void dump(vector_like auto& b, auto& ix) noexcept
   {
      static constexpr auto s = str;
      static constexpr auto n = s.size();

      if (ix + n > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + n));
      }

      std::memcpy(b.data() + ix, s.data(), n);
      ix += n;
   }

   template <const sv& str>
   GLZ_ALWAYS_INLINE void dump_unchecked(vector_like auto& b, auto& ix) noexcept
   {
      static constexpr auto s = str;
      static constexpr auto n = s.size();

      std::memcpy(b.data() + ix, s.data(), n);
      ix += n;
   }

   template <const sv& str>
   GLZ_ALWAYS_INLINE void dump(auto* b, auto& ix) noexcept
   {
      static constexpr auto s = str;
      static constexpr auto n = s.size();

      std::memcpy(b + ix, s.data(), n);
      ix += n;
   }

   template <const sv& str>
   GLZ_ALWAYS_INLINE void dump_unchecked(auto* b, auto& ix) noexcept
   {
      static constexpr auto s = str;
      static constexpr auto n = s.size();

      std::memcpy(b + ix, s.data(), n);
      ix += n;
   }

   GLZ_ALWAYS_INLINE void dump_not_empty(const sv str, vector_like auto& b, auto& ix) noexcept
   {
      const auto n = str.size();
      if (ix + n > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + n));
      }

      std::memcpy(b.data() + ix, str.data(), n);
      ix += n;
   }

   GLZ_ALWAYS_INLINE void dump_not_empty(const sv str, auto* b, auto& ix) noexcept
   {
      const auto n = str.size();

      std::memcpy(b + ix, str.data(), n);
      ix += n;
   }

   GLZ_ALWAYS_INLINE void dump_maybe_empty(const sv str, vector_like auto& b, auto& ix) noexcept
   {
      const auto n = str.size();
      if (n) {
         if (ix + n > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, ix + n));
         }

         std::memcpy(b.data() + ix, str.data(), n);
         ix += n;
      }
   }

   GLZ_ALWAYS_INLINE void dump_maybe_empty(const sv str, auto* b, auto& ix) noexcept
   {
      const auto n = str.size();
      if (n) {
         std::memcpy(b + ix, str.data(), n);
         ix += n;
      }
   }

   GLZ_ALWAYS_INLINE void dump(const sv str, char*& b) noexcept
   {
      for (auto& c : str) {
         *b = c;
         ++b;
      }
   }

   template <std::byte c, class B>
   GLZ_ALWAYS_INLINE void dump(B&& b) noexcept
   {
      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b.emplace_back(c);
      }
      else {
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b.push_back(std::bit_cast<value_t>(c));
      }
   }

   template <std::byte c, class B>
   GLZ_ALWAYS_INLINE void dump(B&& b, auto& ix) noexcept
   {
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() == 0 ? 128 : b.size() * 2);
      }

      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b[ix] = c;
      }
      else {
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b[ix] = std::bit_cast<value_t>(c);
      }
      ++ix;
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(std::byte c, B&& b) noexcept
   {
      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b.emplace_back(c);
      }
      else {
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b.push_back(std::bit_cast<value_t>(c));
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(std::byte c, auto&& b, auto& ix) noexcept
   {
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() == 0 ? 128 : b.size() * 2);
      }

      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b[ix] = c;
      }
      else {
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b[ix] = std::bit_cast<value_t>(c);
      }
      ++ix;
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(const std::span<const std::byte> bytes, B&& b) noexcept
   {
      const auto n = bytes.size();
      const auto b_start = b.size();
      b.resize(b.size() + n);
      std::memcpy(b.data() + b_start, bytes.data(), n);
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(const std::span<const std::byte> bytes, B&& b, auto& ix) noexcept
   {
      const auto n = bytes.size();
      if (ix + n > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + n));
      }

      std::memcpy(b.data() + ix, bytes.data(), n);
      ix += n;
   }

   template <size_t N, class B>
   GLZ_ALWAYS_INLINE void dump(const std::array<uint8_t, N>& bytes, B&& b, auto& ix) noexcept
   {
      if (ix + N > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + N));
      }

      std::memcpy(b.data() + ix, bytes.data(), N);
      ix += N;
   }
}
