// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstring>

namespace glz
{
   constexpr uint64_t to_uint64(const char* bytes, size_t n) noexcept
   {
      if (std::is_constant_evaluated()) {
         uint64_t res{};
         assert(n <= 8);
         for (size_t i = 0; i < n; ++i) {
            res |= static_cast<uint64_t>(bytes[i]) << (8 * i);
         }
         return res;
      }
      else {
         uint64_t res; // no need to default initialize, excess will be discarded
         std::memcpy(&res, bytes, n);
         return res;
      }
   }
   
   template <size_t N>
   constexpr uint64_t to_uint64_n(const char* bytes) noexcept
   {
      static_assert(N <= 8);
      if (std::is_constant_evaluated()) {
         uint64_t res{};
         for (size_t i = 0; i < N; ++i) {
            res |= static_cast<uint64_t>(bytes[i]) << (8 * i);
         }
         return res;
      }
      else {
         uint64_t res; // no need to default initialize, excess will be discarded
         std::memcpy(&res, bytes, N);
         return res;
      }
   }
   
   /*constexpr uint64_t to_uint64(const char* bytes, size_t n = 8) noexcept
   {
      // In cases where n < 8 we do overread in the runtime context for perf and special care should be taken:
      //  * Alignment or page boundary checks should probably be done before calling this function
      //  * Garbage bits should be handled by caller. Ignore them shift them its up to you.
      // https://stackoverflow.com/questions/37800739/is-it-safe-to-read-past-the-end-of-a-buffer-within-the-same-page-on-x86-and-x64
      if (std::is_constant_evaluated()) {
         uint64_t res{};
         assert(n <= 8);
         for (size_t i = 0; i < n; ++i) {
            res |= static_cast<uint64_t>(bytes[i]) << (8 * i);
         }
         return res;
      }
      else {
         // Note: memcpy is way faster with compiletime known length
         uint64_t res; // no need to default initialize, excess will be discarded
         std::memcpy(&res, bytes, 8);
         return res;
      }
   }*/

    /*
   // Note: This relies on undefined behavior but should generally be ok
   // https://stackoverflow.com/questions/37800739/is-it-safe-to-read-past-the-end-of-a-buffer-within-the-same-page-on-x86-and-x64
   // Gets better perf then memcmp on small strings like keys but worse perf on longer strings (>40 or so)
   constexpr bool string_cmp(auto &&s0, auto &&s1) noexcept
   {
      const auto n = s0.size();
      if (s1.size() != n) {
         return false;
      }

      if (n < 8) {
         // TODO add option to skip page checks when we know they are not needed like the compile time known keys or
         // stringviews in the middle of the buffer
         if (!std::is_constant_evaluated() && (((reinterpret_cast<std::uintptr_t>(s0.data()) & 4095) > 4088) ||
                                               ((reinterpret_cast<std::uintptr_t>(s1.data()) & 4095) > 4088)))
            [[unlikely]] {
            // Buffer over-read may cross page boundary
            // There are faster things we could do here but this branch is unlikely
            return std::memcmp(s0.data(), s1.data(), n) == 0;
         }
         else {
            const auto shift = 64 - 8 * n;
            return (to_uint64(s0.data(), n) << shift) == (to_uint64(s1.data(), n) << shift);
         }
      }

      const char *b0 = s0.data();
      const char *b1 = s1.data();
      const char *end7 = s0.data() + n - 7;

      for (; b0 < end7; b0 += 8, b1 += 8) {
         if (to_uint64(b0) != to_uint64(b1)) {
            return false;
         }
      }

      const uint64_t nm8 = n - 8;
      return (to_uint64(s0.data() + nm8) == to_uint64(s1.data() + nm8));
   }*/
   
   constexpr bool string_cmp(auto &&s0, auto &&s1) noexcept
   {
      if (std::is_constant_evaluated()) {
         return s0 == s1;
      }
      else {
         const auto n = s0.size();
         if (s1.size() != n) {
            return false;
         }
         
         return std::memcmp(s0.data(), s1.data(), n) == 0;
      }
   }
   
   template <size_t N>
   constexpr bool string_cmp_n(auto &&s0, auto &&s1) noexcept
   {
      if (std::is_constant_evaluated()) {
         return s0 == s1;
      }
      else {
         if (s0.size() != N) {
            return false;
         }
         
         return std::memcmp(s0.data(), s1.data(), N) == 0;
      }
   }
   
   struct string_cmp_equal_to final
   {
      template <class T0, class T1>
      constexpr bool operator()(T0&& lhs, T1&& rhs) const noexcept {
         return string_cmp(std::forward<T0>(lhs), std::forward<T1>(rhs));
      }
   };
   
   template <const sv& S, bool CheckSize = true>
   inline constexpr bool cx_string_cmp(const sv key) noexcept {
      constexpr auto s = S; // Needed for MSVC to avoid an internal compiler error
      constexpr auto n = s.size();
      if (std::is_constant_evaluated()) {
         return key == s;
      }
      else {
         if constexpr (CheckSize) {
            return (key.size() == n) && (std::memcmp(key.data(), s.data(), n) == 0);
         }
         else {
            return std::memcmp(key.data(), s.data(), n) == 0;
         }
      }
   }
}
