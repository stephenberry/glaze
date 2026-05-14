// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/zmem/header.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/meta.hpp"

#include <array>
#include <type_traits>
#include <utility>

namespace glz::zmem
{
   namespace detail
   {
      [[nodiscard]] consteval size_t max_size(size_t a, size_t b) noexcept
      {
         return a > b ? a : b;
      }
   }

   template <class T>
   [[nodiscard]] consteval bool is_variable_member_type() noexcept
   {
      if constexpr (is_std_vector_v<T> || is_std_string_v<T> || is_std_map_like_v<T>) {
         return true;
      }
      else if constexpr (!is_fixed_type_v<T> && !is_std_array_v<T>) {
         return true; // nested variable struct
      }
      else {
         return false;
      }
   }

   template <class T>
   [[nodiscard]] consteval size_t inline_field_alignment() noexcept
   {
      if constexpr (is_fixed_type_v<T>) {
         return alignof(T);
      }
      else {
         return size_t{8};
      }
   }

   template <class T>
   [[nodiscard]] consteval size_t inline_field_size() noexcept
   {
      if constexpr (is_fixed_type_v<T>) {
         return sizeof(T);
      }
      else if constexpr (is_std_vector_v<T> || is_std_string_v<T> || is_std_map_like_v<T>) {
         return 16; // vector_ref, string_ref, map_ref
      }
      else {
         return 8; // nested variable struct offset
      }
   }

   template <class T>
   [[nodiscard]] consteval size_t vector_data_alignment() noexcept
   {
      if constexpr (is_fixed_type_v<T>) {
         return detail::max_size(size_t{8}, alignof(T));
      }
      else {
         return size_t{8};
      }
   }

   template <class T>
   [[nodiscard]] consteval size_t vector_fixed_stride() noexcept
   {
      if constexpr (std::is_aggregate_v<T> && !is_std_array_v<T>) {
         const size_t alignment = detail::max_size(size_t{8}, alignof(T));
         return align_up(sizeof(T), alignment);
      }
      else {
         return sizeof(T);
      }
   }

   template <class K, class V>
   [[nodiscard]] consteval size_t map_value_alignment() noexcept
   {
      if constexpr (is_fixed_type_v<V>) {
         return alignof(V);
      }
      else {
         return size_t{8};
      }
   }

   template <class K, class V>
   [[nodiscard]] consteval size_t map_entry_align() noexcept
   {
      if constexpr (is_fixed_type_v<V>) {
         return detail::max_size(alignof(K), alignof(V));
      }
      else {
         return detail::max_size(alignof(K), size_t{8});
      }
   }

   template <class K, class V>
   [[nodiscard]] consteval size_t map_value_offset_in_entry() noexcept
   {
      return align_up(sizeof(K), map_value_alignment<K, V>());
   }

   template <class K, class V>
   [[nodiscard]] consteval size_t map_entry_payload_size() noexcept
   {
      if constexpr (is_fixed_type_v<V>) {
         return align_up(sizeof(K), map_value_alignment<K, V>()) + sizeof(V);
      }
      else if constexpr (is_std_vector_v<V> || is_std_string_v<V>) {
         return align_up(sizeof(K), size_t{8}) + 16;
      }
      else {
         return align_up(sizeof(K), size_t{8}) + 8;
      }
   }

   template <class K, class V>
   [[nodiscard]] consteval size_t map_entry_stride() noexcept
   {
      return align_up(map_entry_payload_size<K, V>(), map_entry_align<K, V>());
   }

   template <class K, class V>
   [[nodiscard]] consteval size_t map_data_alignment() noexcept
   {
      return detail::max_size(size_t{8}, map_entry_align<K, V>());
   }

   template <class T>
   struct inline_layout
   {
      static constexpr size_t N = reflect<T>::size;

      template <size_t I>
      using member_type_at = field_t<T, I>;

      template <size_t I>
      static consteval size_t field_alignment() noexcept
      {
         return inline_field_alignment<member_type_at<I>>();
      }

      template <size_t I>
      static consteval size_t field_size() noexcept
      {
         return inline_field_size<member_type_at<I>>();
      }

      static consteval size_t compute_inline_align() noexcept
      {
         size_t alignment = size_t{8};
         [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((alignment = detail::max_size(alignment, field_alignment<Is>())), ...);
         }(std::make_index_sequence<N>{});
         return alignment;
      }

      static consteval auto compute_offsets() noexcept
      {
         std::array<size_t, N> offsets{};
         size_t offset = 0;
         [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((offset = align_up(offset, field_alignment<Is>()),
              offsets[Is] = offset,
              offset += field_size<Is>()), ...);
         }(std::make_index_sequence<N>{});
         return offsets;
      }

      static constexpr auto Offsets = compute_offsets();

      static constexpr size_t InlineSectionSize = []() consteval {
         if constexpr (N == 0) {
            return size_t{0};
         }
         else {
            return Offsets[N - 1] + field_size<N - 1>();
         }
      }();

      static constexpr size_t InlineSectionAlign = compute_inline_align();
      static constexpr size_t InlineBasePadding = padding_for_alignment(8, InlineSectionAlign);
      static constexpr size_t InlineBaseOffset = 8 + InlineBasePadding;
   };

} // namespace glz::zmem
