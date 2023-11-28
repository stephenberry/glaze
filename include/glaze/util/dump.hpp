// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <span>

#include "glaze/core/write.hpp"
// #include <bit>

namespace glz::detail
{
   GLZ_ALWAYS_INLINE void dump(const char c, vector_like auto& b, auto& ix) noexcept
   {
      assert(ix <= b.size());
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
      assert(ix <= b.size());
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

   GLZ_ALWAYS_INLINE void dump_unchecked(const sv str, vector_like auto& b, auto& ix) noexcept
   {
      const auto n = str.size();
      std::memcpy(b.data() + ix, str.data(), n);
      ix += n;
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

   template <char c>
   GLZ_ALWAYS_INLINE void dumpn(size_t n, char*& b) noexcept
   {
      std::fill_n(b, n, c);
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
   GLZ_ALWAYS_INLINE void dumpn(size_t n, vector_like auto& b, auto& ix) noexcept
   {
      if (ix + n > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + n));
      }

      std::fill_n(b.data() + ix, n, c);
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
   GLZ_ALWAYS_INLINE void dump(auto* b, auto& ix) noexcept
   {
      static constexpr auto s = str;
      static constexpr auto n = s.size();

      std::memcpy(b + ix, s.data(), n);
      ix += n;
   }

   GLZ_ALWAYS_INLINE void dump(const sv str, vector_like auto& b, auto& ix) noexcept
   {
      const auto n = str.size();
      if (ix + n > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + n));
      }

      std::memcpy(b.data() + ix, str.data(), n);
      ix += n;
   }

   GLZ_ALWAYS_INLINE void dump(const sv str, auto* b, auto& ix) noexcept
   {
      const auto n = str.size();

      std::memcpy(b + ix, str.data(), n);
      ix += n;
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
      // TODO use std::bit_cast when apple clang supports it
      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b.emplace_back(c);
      }
      else {
         static constexpr std::byte chr = c;
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b.push_back(*reinterpret_cast<value_t*>(const_cast<std::byte*>(&chr)));
         // b.push_back(std::bit_cast<value_t>(c));
      }
   }

   template <std::byte c, class B>
   GLZ_ALWAYS_INLINE void dump(B&& b, auto& ix) noexcept
   {
      assert(ix <= b.size());
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() == 0 ? 128 : b.size() * 2);
      }

      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b[ix] = c;
      }
      else {
         static constexpr std::byte chr = c;
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b[ix] = *reinterpret_cast<value_t*>(const_cast<std::byte*>(&chr));
         // TODO use std::bit_cast when apple clang supports it
         // b[ix] = std::bit_cast<value_t>(c);
      }
      ++ix;
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(std::byte c, B&& b) noexcept
   {
      // TODO use std::bit_cast when apple clang supports it
      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b.emplace_back(c);
      }
      else {
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b.push_back(*reinterpret_cast<value_t*>(const_cast<std::byte*>(&c)));
         // b.push_back(std::bit_cast<value_t>(c));
      }
   }

   template <class B>
   GLZ_ALWAYS_INLINE void dump(std::byte c, auto&& b, auto& ix) noexcept
   {
      assert(ix <= b.size());
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() == 0 ? 128 : b.size() * 2);
      }

      using value_t = range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b[ix] = c;
      }
      else {
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b[ix] = *reinterpret_cast<value_t*>(const_cast<std::byte*>(&c));
         // TODO use std::bit_cast when apple clang supports it
         // b[ix] = std::bit_cast<value_t>(c);
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
      assert(ix <= b.size());
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
      assert(ix <= b.size());
      if (ix + N > b.size()) [[unlikely]] {
         b.resize((std::max)(b.size() * 2, ix + N));
      }

      std::memcpy(b.data() + ix, bytes.data(), N);
      ix += N;
   }

   template <glaze_flags_t T>
   consteval auto byte_length() noexcept
   {
      constexpr auto N = std::tuple_size_v<meta_t<T>>;

      if constexpr (N % 8 == 0) {
         return N / 8;
      }
      else {
         return (N / 8) + 1;
      }
   }
}
