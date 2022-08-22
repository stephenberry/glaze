// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/write.hpp"

#include <span>

namespace glaze::detail
{
   inline void dump(const char c, std::string& b) noexcept {
      b.push_back(c);
   }
   
   template <char c>
   inline void dump(std::string& b) noexcept {
      b.push_back(c);
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

   template <string_literal str>
   inline void dump(std::string& b) noexcept {
      b.append(str.value, str.size);
   }

   inline void dump(const std::string_view str, std::output_iterator<char> auto&& it) noexcept {
      std::copy(str.data(), str.data() + str.size(), it);
   }

   inline void dump(const std::string_view str, std::string& b) noexcept {
      b.append(str.data(), str.size());
   }
   
   template <std::byte c>
   inline void dump(std::vector<std::byte>& b) noexcept {
      b.emplace_back(c);
   }
   
   inline void dump(std::byte c,std::vector<std::byte>& b) noexcept {
      b.emplace_back(c);
   }
   
   inline void dump(const std::span<const std::byte> bytes, std::vector<std::byte>& b) noexcept {
      const auto n = bytes.size();
      const auto b_start = b.size();
      b.resize(b.size() + n);
      std::memcpy(b.data() + b_start, bytes.data(), n);
   }
}
