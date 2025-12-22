// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/util/inline.hpp"

namespace glz
{
   // Primary template for buffer traits
   // Handles resizable buffers (std::string, std::vector<char>, etc.)
   template <class Buffer>
   struct buffer_traits
   {
      static constexpr bool is_resizable = resizable<std::remove_cvref_t<Buffer>>;
      static constexpr bool has_bounded_capacity = !is_resizable && has_size<std::remove_cvref_t<Buffer>>;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(const Buffer& b) noexcept
      {
         if constexpr (is_resizable) {
            return std::numeric_limits<size_t>::max(); // Effectively unlimited
         }
         else if constexpr (has_size<std::remove_cvref_t<Buffer>>) {
            return b.size();
         }
         else {
            return std::numeric_limits<size_t>::max();
         }
      }

      // Attempt to ensure buffer can hold `needed` bytes
      // Returns true if successful, false if buffer cannot accommodate
      GLZ_ALWAYS_INLINE static bool ensure_capacity(Buffer& b, size_t needed) noexcept(not is_resizable)
      {
         if constexpr (is_resizable) {
            if (needed > b.size()) {
               b.resize(2 * needed);
            }
            return true;
         }
         else {
            return capacity(b) >= needed;
         }
      }

      // Finalize buffer to actual written size
      GLZ_ALWAYS_INLINE static void finalize(Buffer& b, size_t written) noexcept(not is_resizable)
      {
         if constexpr (is_resizable) {
            b.resize(written);
         }
         // For fixed buffers: no-op, count is returned in result
      }
   };

   // Specialization for raw char pointers
   // Raw pointers have unknown capacity and must trust the caller
   template <>
   struct buffer_traits<char*>
   {
      static constexpr bool is_resizable = false;
      static constexpr bool has_bounded_capacity = false;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(char*) noexcept
      {
         return std::numeric_limits<size_t>::max();
      }

      GLZ_ALWAYS_INLINE static constexpr bool ensure_capacity(char*, size_t) noexcept
      {
         return true; // Trust caller
      }

      GLZ_ALWAYS_INLINE static constexpr void finalize(char*, size_t) noexcept {}
   };

   // Specialization for std::span
   template <class T, size_t Extent>
   struct buffer_traits<std::span<T, Extent>>
   {
      static constexpr bool is_resizable = false;
      static constexpr bool has_bounded_capacity = true;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(const std::span<T, Extent>& b) noexcept { return b.size(); }

      GLZ_ALWAYS_INLINE static constexpr bool ensure_capacity(const std::span<T, Extent>& b, size_t needed) noexcept
      {
         return needed <= b.size();
      }

      GLZ_ALWAYS_INLINE static constexpr void finalize(std::span<T, Extent>&, size_t) noexcept {}
   };

   // Specialization for std::array
   template <class T, size_t N>
   struct buffer_traits<std::array<T, N>>
   {
      static constexpr bool is_resizable = false;
      static constexpr bool has_bounded_capacity = true;
      static constexpr size_t static_capacity = N;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(const std::array<T, N>&) noexcept { return N; }

      GLZ_ALWAYS_INLINE static constexpr bool ensure_capacity(std::array<T, N>&, size_t needed) noexcept
      {
         return needed <= N;
      }

      GLZ_ALWAYS_INLINE static constexpr void finalize(std::array<T, N>&, size_t) noexcept {}
   };
}
