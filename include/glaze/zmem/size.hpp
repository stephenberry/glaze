// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/zmem/header.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/core/common.hpp"
#include "glaze/reflection/to_tuple.hpp"

#include <map>

namespace glz
{
   // ============================================================================
   // ZMEM Size Computation
   // ============================================================================
   //
   // Fast size computation for ZMEM serialization. This allows pre-allocating
   // the exact buffer size needed, eliminating all resize checks during write.
   //
   // Usage:
   //   auto size = glz::size_zmem(value);
   //   std::string buffer(size, '\0');
   //   glz::write_zmem<glz::opts{.format = ZMEM}>(value, buffer);
   //

   namespace zmem
   {
      // ========================================================================
      // Size computation for primitives
      // ========================================================================

      template <class T>
         requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
      GLZ_ALWAYS_INLINE constexpr size_t compute_size(const T&) noexcept
      {
         return sizeof(T);
      }

      // ========================================================================
      // Size computation for fixed-size arrays
      // ========================================================================

      template <class T, std::size_t N>
         requires is_fixed_type_v<T>
      GLZ_ALWAYS_INLINE constexpr size_t compute_size(const T (&)[N]) noexcept
      {
         return sizeof(T) * N;
      }

      template <class T, std::size_t N>
         requires is_fixed_type_v<T>
      GLZ_ALWAYS_INLINE constexpr size_t compute_size(const std::array<T, N>&) noexcept
      {
         return sizeof(T) * N;
      }

      // ========================================================================
      // Size computation for zmem::optional
      // ========================================================================

      template <class T>
      GLZ_ALWAYS_INLINE constexpr size_t compute_size(const optional<T>&) noexcept
      {
         return sizeof(optional<T>);
      }

      // ========================================================================
      // Size computation for std::optional
      // ========================================================================

      template <class T>
      GLZ_ALWAYS_INLINE constexpr size_t compute_size(const std::optional<T>&) noexcept
      {
         return sizeof(optional<T>);  // Converted to zmem::optional on write
      }

      // ========================================================================
      // Size computation for vectors
      // ========================================================================

      // Fixed element type: count (8) + elements
      template <class T, class Alloc>
         requires is_fixed_type_v<T>
      GLZ_ALWAYS_INLINE size_t compute_size(const std::vector<T, Alloc>& v) noexcept
      {
         return 8 + v.size() * sizeof(T);
      }

      // Forward declarations for mutually recursive size computation
      template <class T>
         requires(!is_fixed_type_v<T> && !is_std_array_v<T> && (glz::reflectable<T> || glz::glaze_object_t<T>))
      size_t compute_size(const T& value) noexcept;

      // Variable element type: count (8) + offset table + elements
      template <class T, class Alloc>
         requires(!is_fixed_type_v<T>)
      size_t compute_size(const std::vector<T, Alloc>& v) noexcept
      {
         const size_t count = v.size();
         size_t total = 8;  // count

         if (count > 0) {
            total += (count + 1) * 8;  // offset table
            for (const auto& elem : v) {
               total += compute_size(elem);
            }
         }

         return total;
      }

      // ========================================================================
      // Size computation for strings
      // ========================================================================

      template <class CharT, class Traits, class Alloc>
      GLZ_ALWAYS_INLINE size_t compute_size(const std::basic_string<CharT, Traits, Alloc>& s) noexcept
      {
         return 8 + s.size() * sizeof(CharT);  // length + data
      }

      template <class CharT, class Traits>
      GLZ_ALWAYS_INLINE size_t compute_size(const std::basic_string_view<CharT, Traits>& s) noexcept
      {
         return 8 + s.size() * sizeof(CharT);  // length + data
      }

      // ========================================================================
      // Size computation for spans
      // ========================================================================

      template <class T, std::size_t Extent>
         requires is_fixed_type_v<T>
      GLZ_ALWAYS_INLINE size_t compute_size(const std::span<T, Extent>& s) noexcept
      {
         if constexpr (Extent == std::dynamic_extent) {
            return 8 + s.size() * sizeof(T);  // count + elements
         }
         else {
            return s.size() * sizeof(T);  // elements only (fixed size)
         }
      }

      // ========================================================================
      // Size computation for pairs
      // ========================================================================

      template <class K, class V>
         requires is_fixed_type_v<V>
      GLZ_ALWAYS_INLINE size_t compute_size(const std::pair<K, V>&) noexcept
      {
         // Fixed pair: key alignment + key + value alignment + value
         // For size computation, we use worst-case alignment
         return sizeof(K) + (alignof(V) - 1) + sizeof(V);
      }

      template <class K, class V>
         requires(!is_fixed_type_v<V>)
      size_t compute_size(const std::pair<K, V>& p) noexcept
      {
         // Variable value: key + value (no alignment padding in variable value maps)
         return compute_size(p.first) + compute_size(p.second);
      }

      // ========================================================================
      // Size computation for maps
      // ========================================================================

      // Fixed value type
      template <class K, class V, class Cmp, class Alloc>
         requires is_fixed_type_v<V>
      size_t compute_size(const std::map<K, V, Cmp, Alloc>& m) noexcept
      {
         size_t total = 8;  // count
         for (const auto& [k, v] : m) {
            // Use pair size with alignment
            total += compute_size(std::pair<K, V>{k, v});
         }
         return total;
      }

      // Variable value type
      template <class K, class V, class Cmp, class Alloc>
         requires(!is_fixed_type_v<V>)
      size_t compute_size(const std::map<K, V, Cmp, Alloc>& m) noexcept
      {
         const size_t count = m.size();
         size_t total = 8 + 8;  // size header + count

         if (count > 0) {
            total += (count + 1) * 8;  // offset table
            for (const auto& [k, v] : m) {
               total += compute_size(k) + compute_size(v);
            }
         }

         return total;
      }

      // ========================================================================
      // Size computation for fixed structs (trivially copyable)
      // ========================================================================

      template <class T>
         requires(std::is_aggregate_v<T> && is_fixed_type_v<T> && !is_std_array_v<T>)
      GLZ_ALWAYS_INLINE constexpr size_t compute_size(const T&) noexcept
      {
         return sizeof(T);
      }

      // ========================================================================
      // Size computation for variable structs
      // ========================================================================

      // Member access helper for size computation (works for both reflectable and glaze_object_t)
      template <class T, size_t I>
      GLZ_ALWAYS_INLINE decltype(auto) access_member_for_size(const T& value) noexcept {
         if constexpr (glz::reflectable<T>) {
            return get_member(value, get<I>(to_tie(value)));
         }
         else {
            return get_member(value, get<I>(reflect<T>::values));
         }
      }

      template <class T>
         requires(!is_fixed_type_v<T> && !is_std_array_v<T> && (glz::reflectable<T> || glz::glaze_object_t<T>))
      size_t compute_size(const T& value) noexcept
      {
         // Variable struct: size header (8) + inline section + variable section
         size_t total = 8;  // size header

         constexpr auto N = reflect<T>::size;

         // Compute inline section and variable section sizes
         for_each<N>([&]<size_t I>() {
            decltype(auto) member = access_member_for_size<T, I>(value);
            using MemberType = field_t<T, I>;

            if constexpr (is_fixed_type_v<MemberType>) {
               total += sizeof(MemberType);
            }
            else if constexpr (is_std_vector_v<MemberType>) {
               // Inline: 16-byte reference
               total += 16;
               // Variable section: vector data
               using ElemType = typename MemberType::value_type;
               if constexpr (is_fixed_type_v<ElemType>) {
                  total += member.size() * sizeof(ElemType);
               }
               else {
                  for (const auto& elem : member) {
                     total += compute_size(elem);
                  }
               }
            }
            else if constexpr (is_std_string_v<MemberType>) {
               // Inline: 16-byte reference
               total += 16;
               // Variable section: string data
               total += member.size();
            }
            else {
               // Nested variable type
               total += compute_size(member);
            }
         });

         return total;
      }

   }  // namespace zmem

   // ============================================================================
   // Public API: size_zmem
   // ============================================================================

   template <class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE size_t size_zmem(const T& value) noexcept
   {
      return zmem::compute_size(value);
   }

}  // namespace glz
