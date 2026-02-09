// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/zmem/header.hpp"
#include "glaze/zmem/layout.hpp"
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

      // Fixed element type: count (8) + elements (each padded to 8 bytes if struct)
      template <class T, class Alloc>
         requires is_fixed_type_v<T>
      GLZ_ALWAYS_INLINE size_t compute_size(const std::vector<T, Alloc>& v) noexcept
      {
         if constexpr (std::is_aggregate_v<T> && !is_std_array_v<T>) {
            // Fixed struct elements use padded stride (min 8 bytes)
            return 8 + v.size() * vector_fixed_stride<T>();
         }
         else {
            // Primitives and arrays use natural size
            return 8 + v.size() * sizeof(T);
         }
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
         return 8 + compute_vector_payload_size(v);
      }

      template <class Vec>
      GLZ_ALWAYS_INLINE size_t compute_vector_payload_size(const Vec& v) noexcept
      {
         using ElemType = typename Vec::value_type;
         const size_t count = v.size();
         if (count == 0) {
            return 0;
         }

         if constexpr (is_fixed_type_v<ElemType>) {
            if constexpr (std::is_aggregate_v<ElemType> && !is_std_array_v<ElemType>) {
               return count * vector_fixed_stride<ElemType>();
            }
            else {
               return count * sizeof(ElemType);
            }
         }
         else {
            size_t total = (count + 1) * 8;  // offset table
            for (const auto& elem : v) {
               if constexpr (is_std_string_v<ElemType>) {
                  using CharT = typename ElemType::value_type;
                  total += elem.size() * sizeof(CharT);
               }
               else {
                  total += compute_size(elem);
               }
            }
            return total;
         }
      }

      template <class K, class V, class Map>
      GLZ_ALWAYS_INLINE size_t compute_map_payload_size(const Map& m, size_t payload_offset) noexcept
      {
         const size_t count = m.size();
         if (count == 0) {
            return 0;
         }

         constexpr size_t entry_stride = map_entry_stride<K, V>();
         size_t offset = entry_stride * count;

         if constexpr (is_fixed_type_v<V>) {
            return offset;
         }

         for (const auto& [k, v] : m) {
            size_t value_alignment = size_t{8};
            if constexpr (is_std_vector_v<V>) {
               using ElemType = typename V::value_type;
               value_alignment = vector_data_alignment<ElemType>();
            }

            const size_t pad = padding_for_alignment(payload_offset + offset, value_alignment);
            offset += pad;

            if constexpr (is_std_vector_v<V>) {
               offset += compute_vector_payload_size(v);
            }
            else if constexpr (is_std_string_v<V>) {
               using CharT = typename V::value_type;
               offset += v.size() * sizeof(CharT);
            }
            else {
               offset += compute_size(v);
            }
         }

         return offset;
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
         if (m.empty()) {
            return total;
         }

         const size_t padding = padding_for_alignment(total, map_data_alignment<K, V>());
         total += padding;
         const size_t payload_offset = total;
         total += compute_map_payload_size<K, V>(m, payload_offset);
         return total;
      }

      // Variable value type
      template <class K, class V, class Cmp, class Alloc>
         requires(!is_fixed_type_v<V>)
      size_t compute_size(const std::map<K, V, Cmp, Alloc>& m) noexcept
      {
         size_t total = 8;  // count
         if (m.empty()) {
            return total;
         }

         const size_t padding = padding_for_alignment(total, map_data_alignment<K, V>());
         total += padding;
         const size_t payload_offset = total;
         total += compute_map_payload_size<K, V>(m, payload_offset);
         return total;
      }

      // ========================================================================
      // Size computation for fixed structs (trivially copyable)
      // ========================================================================

      template <class T>
         requires(std::is_aggregate_v<T> && is_fixed_type_v<T> && !is_std_array_v<T>)
      GLZ_ALWAYS_INLINE constexpr size_t compute_size(const T&) noexcept
      {
         // Fixed struct sizes are padded to alignment (min 8 bytes) for safe zero-copy access
         constexpr size_t alignment = alignof(T) > 8 ? alignof(T) : 8;
         return padded_size(sizeof(T), alignment);
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
         constexpr auto N = reflect<T>::size;
         using Layout = inline_layout<T>;

         size_t variable_size = 0;
         for_each<N>([&]<size_t I>() {
            decltype(auto) member = access_member_for_size<T, I>(value);
            using MemberType = field_t<T, I>;

            if constexpr (is_std_vector_v<MemberType>) {
               using ElemType = typename MemberType::value_type;
               const size_t alignment = vector_data_alignment<ElemType>();
               const size_t current_offset = Layout::InlineSectionSize + variable_size;
               variable_size += padding_for_alignment(current_offset, alignment);
               variable_size += compute_vector_payload_size(member);
            }
            else if constexpr (is_std_string_v<MemberType>) {
               const size_t current_offset = Layout::InlineSectionSize + variable_size;
               variable_size += padding_for_alignment(current_offset, 8);
               using CharT = typename MemberType::value_type;
               variable_size += member.size() * sizeof(CharT);
            }
            else if constexpr (is_std_map_like_v<MemberType>) {
               using K = typename MemberType::key_type;
               using V = typename MemberType::mapped_type;
               const size_t alignment = map_data_alignment<K, V>();
               const size_t current_offset = Layout::InlineSectionSize + variable_size;
               variable_size += padding_for_alignment(current_offset, alignment);
               const size_t payload_offset = Layout::InlineSectionSize + variable_size;
               variable_size += compute_map_payload_size<K, V>(member, payload_offset);
            }
            else if constexpr (!is_fixed_type_v<MemberType>) {
               const size_t current_offset = Layout::InlineSectionSize + variable_size;
               variable_size += padding_for_alignment(current_offset, 8);
               variable_size += compute_size(member);
            }
         });

         size_t content_size = Layout::InlineBasePadding + Layout::InlineSectionSize + variable_size;
         content_size += padding_for_alignment(content_size, 8);
         return 8 + content_size;
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
