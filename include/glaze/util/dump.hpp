// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/write.hpp"

#include <span>
//#include <bit>

namespace glz::detail
{
   inline void dump(const char c, push_backable auto& b) noexcept {
      b.push_back(c);
   }
   
   inline void dump(const char c, vector_like auto& b, auto&& ix) noexcept {
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() * 2);
      }
      
      b[ix] = c;
      ++ix;
   }
   
   inline void dump(const char c, char*& b) noexcept {
      *b = c;
      ++b;
   }
   
   template <char c>
   inline void dump(push_backable auto& b) noexcept {
      b.push_back(c);
   }
   
   template <char c>
   inline void dump(vector_like auto& b, auto&& ix) noexcept {
      if (ix == b.size()) [[unlikely]] {
         b.resize(b.size() * 2);
      }
      
      b[ix] = c;
      ++ix;
   }
   
   template <char c>
   inline void dump_unchecked(vector_like auto& b, auto&& ix) noexcept {
      b[ix] = c;
      ++ix;
   }
   
   template <char c>
   inline void dump(char*& b) noexcept {
      *b = c;
      ++b;
   }

   template <char c>
   inline void dump(std::output_iterator<char> auto&& it) noexcept
   {
      *it = c;
      ++it;
   }

   template <string_literal str>
   inline void dump(std::output_iterator<char> auto&& it) noexcept {
      std::copy(str.value, str.value + str.size, it);
   }
   
   template <const sv& str>
   inline void dump(std::output_iterator<char> auto&& it) noexcept {
      std::copy_n(str.data(), str.size(), it);
   }
   
   template <string_literal str>
   inline void dump(char*& b) noexcept {
      static constexpr auto s = str.sv();
      for (auto& c : s) {
         *b = c;
         ++b;
      }
   }
   
   template <string_literal str>
   inline void dump(vector_like auto& b, auto&& ix) noexcept {
      static constexpr auto s = str.sv();
      static constexpr auto n = s.size();
      
      if (ix + n > b.size()) [[unlikely]] {
         b.resize(std::max(b.size() * 2, ix + n));
      }
      
      std::memcpy(b.data() + ix, s.data(), n);
      ix += n;
   }
   
   template <const sv& str>
   inline void dump(vector_like auto& b, auto&& ix) noexcept {
      static constexpr auto s = str;
      static constexpr auto n = s.size();
      
      if (ix + n > b.size()) [[unlikely]] {
         b.resize(std::max(b.size() * 2, ix + n));
      }
      
      std::memcpy(b.data() + ix, s.data(), n);
      ix += n;
   }

   inline void dump(const sv str, std::output_iterator<char> auto&& it) noexcept {
      std::copy(str.data(), str.data() + str.size(), it);
   }
   
   inline void dump(const sv str, vector_like auto& b, auto&& ix) noexcept {
      const auto n = str.size();
      if (ix + n > b.size()) [[unlikely]] {
         b.resize(std::max(b.size() * 2, ix + n));
      }
      
      std::memcpy(b.data() + ix, str.data(), n);
      ix += n;
   }
   
   inline void dump(const sv str, char*& b) noexcept {
      for (auto& c : str) {
         *b = c;
         ++b;
      }
   }
   
   template <std::byte c, class B>
   inline void dump(B&& b) noexcept
   {
      // TODO use std::bit_cast when apple clang supports it
      using value_t = nano::ranges::range_value_t<std::decay_t<B>>;
      if constexpr (std::same_as<value_t, std::byte>) {
         b.emplace_back(c);
      }
      else {
         static constexpr std::byte chr = c;
         static_assert(sizeof(value_t) == sizeof(std::byte));
         b.push_back(*reinterpret_cast<value_t*>(const_cast<std::byte*>(&chr)));
         //b.push_back(std::bit_cast<value_t>(c));
      }
   }

   template <class B>
   inline void dump(std::byte c, auto&& b) noexcept
   {
      // TODO use std::bit_cast when apple clang supports it
      using value_t = nano::ranges::range_value_t<std::decay_t<B>>;
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
   inline void dump(const std::span<const std::byte> bytes, B&& b) noexcept
   {
      const auto n = bytes.size();
      const auto b_start = b.size();
      b.resize(b.size() + n);
      std::memcpy(b.data() + b_start, bytes.data(), n);
   }
}
