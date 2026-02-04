// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/zmem/header.hpp"
#include "glaze/zmem/layout.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/core/common.hpp"
#include "glaze/concepts/container_concepts.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/reflection/to_tuple.hpp"

#include <algorithm>
#include <fstream>
#include <map>

namespace glz
{
   // ============================================================================
   // Write Unchecked Helper
   // ============================================================================
   //
   // Check if the write_unchecked flag is set in opts.
   // When set, buffer resize checks are skipped (buffer is pre-allocated).
   //

   template <auto Opts>
   constexpr bool is_write_unchecked() noexcept
   {
      if constexpr (requires { Opts.internal; }) {
         return (Opts.internal & static_cast<uint32_t>(opts_internal::write_unchecked)) != 0;
      }
      else {
         return false;
      }
   }

   // ============================================================================
   // ZMEM Serialize Entry Point
   // ============================================================================

   template <>
   struct serialize<ZMEM>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::remove_cvref_t<T>;
         to<ZMEM, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                        std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   namespace zmem
   {
      // ============================================================================
      // Compile-Time Member Type Extraction
      // ============================================================================

      // Extract member type from member pointer type (e.g., int Foo::* -> int)
      template <class T>
      struct member_ptr_type;

      template <class M, class C>
      struct member_ptr_type<M C::*> {
         using type = std::remove_cvref_t<M>;
      };

      template <class T>
      using member_ptr_type_t = typename member_ptr_type<std::remove_cvref_t<T>>::type;

      // ============================================================================
      // Compile-Time Variable Member Detection
      // ============================================================================

      // Check if a member pointer type points to a variable member (vector or string)
      template <class MemberPtr>
      struct is_variable_member_ptr : std::false_type {};

      template <class M, class C>
         requires (is_std_vector_v<std::remove_cvref_t<M>> || is_std_string_v<std::remove_cvref_t<M>>)
      struct is_variable_member_ptr<M C::*> : std::true_type {};

      template <class T>
      inline constexpr bool is_variable_member_ptr_v = is_variable_member_ptr<std::remove_cvref_t<T>>::value;


      template <bool Unchecked, class T, class B>
      GLZ_ALWAYS_INLINE void write_fixed_raw(const T& value, B& b, size_t& ix) noexcept
      {
         if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            write_value<Unchecked>(value, b, ix);
         }
         else {
            write_bytes<Unchecked>(&value, sizeof(T), b, ix);
         }
      }

      template <auto Opts, class Vec, class B>
      GLZ_ALWAYS_INLINE void write_vector_data(const Vec& value, is_context auto&& ctx, B& b, size_t& ix)
      {
         constexpr bool unchecked = glz::is_write_unchecked<Opts>();
         using ElemType = typename Vec::value_type;
         const uint64_t count = value.size();
         if (count == 0) return;

         if constexpr (zmem::is_fixed_type_v<ElemType>) {
            if constexpr (std::is_aggregate_v<ElemType> && !zmem::is_std_array_v<ElemType>) {
               constexpr size_t stride = zmem::vector_fixed_stride<ElemType>();
               if constexpr (stride == sizeof(ElemType)) {
                  write_bytes<unchecked>(value.data(), sizeof(ElemType) * count, b, ix);
               }
               else {
                  for (const auto& elem : value) {
                     to<ZMEM, ElemType>::template op<Opts>(elem, ctx, b, ix);
                  }
               }
            }
            else {
               write_bytes<unchecked>(value.data(), sizeof(ElemType) * count, b, ix);
            }
         }
         else {
            const size_t offset_table_start = ix;
            const size_t offset_table_size = (count + 1) * sizeof(uint64_t);

            if constexpr (!unchecked && resizable<std::remove_cvref_t<B>>) {
               if (ix + offset_table_size > b.size()) {
                  b.resize((std::max)(b.size() * 2, ix + offset_table_size));
               }
            }
            std::memset(b.data() + ix, 0, offset_table_size);
            ix += offset_table_size;

            const size_t data_section_start = ix;

            uint64_t stack_offsets[zmem::offset_table_stack_threshold + 1];
            std::vector<uint64_t> heap_offsets;
            uint64_t* offsets;

            if (count <= zmem::offset_table_stack_threshold) {
               offsets = stack_offsets;
            }
            else {
               heap_offsets.resize(count + 1);
               offsets = heap_offsets.data();
            }

            for (size_t i = 0; i < count; ++i) {
               offsets[i] = ix - data_section_start;
               if constexpr (zmem::is_std_string_v<ElemType>) {
                  using CharT = typename ElemType::value_type;
                  const uint64_t length = value[i].size() * sizeof(CharT);
                  if (length > 0) {
                     write_bytes<unchecked>(value[i].data(), length, b, ix);
                  }
               }
               else {
                  to<ZMEM, ElemType>::template op<Opts>(value[i], ctx, b, ix);
               }
            }
            offsets[count] = ix - data_section_start;

            if constexpr (std::endian::native == std::endian::big) {
               for (size_t i = 0; i <= count; ++i) {
                  offsets[i] = std::byteswap(offsets[i]);
               }
            }
            std::memcpy(b.data() + offset_table_start, offsets, offset_table_size);
         }
      }

      template <class Map>
      GLZ_ALWAYS_INLINE auto make_sorted_entries(const Map& value)
      {
         using K = typename Map::key_type;
         using V = typename Map::mapped_type;

         std::vector<std::pair<K, V>> entries;
         entries.reserve(value.size());
         for (const auto& [k, v] : value) {
            entries.emplace_back(k, v);
         }

         if constexpr (zmem::is_std_unordered_map_v<Map>) {
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
               return a.first < b.first;
            });
         }

         return entries;
      }

      template <auto Opts, class Entries, class B>
      GLZ_ALWAYS_INLINE void write_map_payload_aligned(const Entries& entries,
                                                       size_t inline_base,
                                                       is_context auto&& ctx,
                                                       B& b,
                                                       size_t& ix)
      {
         constexpr bool unchecked = glz::is_write_unchecked<Opts>();
         using Entry = std::remove_cvref_t<typename Entries::value_type>;
         using K = std::remove_cvref_t<typename Entry::first_type>;
         using V = std::remove_cvref_t<typename Entry::second_type>;

         const uint64_t count = entries.size();
         if (count == 0) return;

         constexpr size_t entry_stride = zmem::map_entry_stride<K, V>();
         constexpr size_t value_offset = zmem::map_value_offset_in_entry<K, V>();

         std::array<size_t, zmem::offset_table_stack_threshold> stack_positions{};
         std::vector<size_t> heap_positions;
         size_t* offset_positions = nullptr;

         if constexpr (!zmem::is_fixed_type_v<V>) {
            if (count <= zmem::offset_table_stack_threshold) {
               offset_positions = stack_positions.data();
            }
            else {
               heap_positions.resize(count);
               offset_positions = heap_positions.data();
            }
         }

         size_t entry_index = 0;
         for (const auto& [k, v] : entries) {
            const size_t entry_start = ix;
            write_fixed_raw<unchecked>(k, b, ix);

            const size_t pad_to_value = value_offset - sizeof(K);
            if (pad_to_value > 0) {
               write_padding<unchecked>(pad_to_value, b, ix);
            }

            if constexpr (zmem::is_fixed_type_v<V>) {
               write_fixed_raw<unchecked>(v, b, ix);
               const size_t tail_pad = entry_stride - (value_offset + sizeof(V));
               if (tail_pad > 0) {
                  write_padding<unchecked>(tail_pad, b, ix);
               }
            }
            else if constexpr (zmem::is_std_vector_v<V>) {
               write_value<unchecked, uint64_t>(0, b, ix); // offset placeholder
               write_value<unchecked>(static_cast<uint64_t>(v.size()), b, ix);
               offset_positions[entry_index] = entry_start + value_offset;
               const size_t tail_pad = entry_stride - (value_offset + 16);
               if (tail_pad > 0) {
                  write_padding<unchecked>(tail_pad, b, ix);
               }
            }
            else if constexpr (zmem::is_std_string_v<V>) {
               using CharT = typename V::value_type;
               const uint64_t length = v.size() * sizeof(CharT);
               write_value<unchecked, uint64_t>(0, b, ix); // offset placeholder
               write_value<unchecked>(length, b, ix);
               offset_positions[entry_index] = entry_start + value_offset;
               const size_t tail_pad = entry_stride - (value_offset + 16);
               if (tail_pad > 0) {
                  write_padding<unchecked>(tail_pad, b, ix);
               }
            }
            else {
               write_value<unchecked, uint64_t>(0, b, ix); // offset placeholder
               offset_positions[entry_index] = entry_start + value_offset;
               const size_t tail_pad = entry_stride - (value_offset + 8);
               if (tail_pad > 0) {
                  write_padding<unchecked>(tail_pad, b, ix);
               }
            }

            ++entry_index;
         }

         if constexpr (!zmem::is_fixed_type_v<V>) {
            size_t value_index = 0;
            for (const auto& [k, v] : entries) {
               size_t value_alignment = size_t{8};
               if constexpr (zmem::is_std_vector_v<V>) {
                  using ElemType = typename V::value_type;
                  value_alignment = zmem::vector_data_alignment<ElemType>();
               }

               const size_t padding = zmem::padding_for_alignment(ix - inline_base, value_alignment);
               if (padding > 0) {
                  write_padding<unchecked>(padding, b, ix);
               }

               const uint64_t offset = ix - inline_base;
               const uint64_t offset_le = std::endian::native == std::endian::little
                                             ? offset
                                             : zmem::to_little_endian(offset);
               std::memcpy(b.data() + offset_positions[value_index], &offset_le, sizeof(uint64_t));

               if constexpr (zmem::is_std_vector_v<V>) {
                  write_vector_data<Opts>(v, ctx, b, ix);
               }
               else if constexpr (zmem::is_std_string_v<V>) {
                  using CharT = typename V::value_type;
                  const uint64_t length = v.size() * sizeof(CharT);
                  if (length > 0) {
                     write_bytes<unchecked>(v.data(), length, b, ix);
                  }
               }
               else {
                  to<ZMEM, V>::template op<Opts>(v, ctx, b, ix);
               }

               ++value_index;
            }
         }
      }

   } // namespace zmem

   // ============================================================================
   // Write Specializations
   // ============================================================================

   // Primitives (bool, integers, floats, enums)
   template <class T>
      requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
   struct to<ZMEM, T> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&&, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         zmem::write_value<unchecked>(value, b, ix);
      }
   };

   // Fixed-size C arrays (including char arrays for fixed strings)
   template <class T, std::size_t N>
   struct to<ZMEM, T[N]> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const T (&value)[N], is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         if constexpr (zmem::is_fixed_type_v<T>) {
            // For fixed element types, write contiguously
            zmem::write_bytes<unchecked>(value, sizeof(T) * N, b, ix);
         }
         else {
            // For variable element types, serialize each element
            for (std::size_t i = 0; i < N; ++i) {
               to<ZMEM, T>::template op<Opts>(value[i], ctx, b, ix);
            }
         }
      }
   };

   // std::array
   template <class T, std::size_t N>
   struct to<ZMEM, std::array<T, N>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::array<T, N>& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         if constexpr (zmem::is_fixed_type_v<T>) {
            zmem::write_bytes<unchecked>(value.data(), sizeof(T) * N, b, ix);
         }
         else {
            for (std::size_t i = 0; i < N; ++i) {
               to<ZMEM, T>::template op<Opts>(value[i], ctx, b, ix);
            }
         }
      }
   };

   // ZMEM optional
   template <class T>
   struct to<ZMEM, zmem::optional<T>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const zmem::optional<T>& value, [[maybe_unused]] is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         // Write the entire optional struct directly (it has guaranteed layout)
         zmem::write_bytes<unchecked>(&value, sizeof(zmem::optional<T>), b, ix);
      }
   };

   // std::optional -> zmem::optional conversion on write
   template <class T>
   struct to<ZMEM, std::optional<T>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::optional<T>& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         zmem::optional<T> zmem_opt;
         if (value.has_value()) {
            zmem_opt = *value;
         }
         to<ZMEM, zmem::optional<T>>::template op<Opts>(zmem_opt, ctx, b, ix);
      }
   };

   // std::vector - fixed element types (contiguous)
   template <class T, class Alloc>
      requires zmem::is_fixed_type_v<T>
   struct to<ZMEM, std::vector<T, Alloc>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::vector<T, Alloc>& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         // For top-level vector serialization (Array message format):
         // Write count (8 bytes) + elements
         const uint64_t count = value.size();
         zmem::write_value<unchecked>(count, b, ix);

         if (count > 0) {
            if constexpr (std::is_aggregate_v<T> && !zmem::is_std_array_v<T>) {
               constexpr size_t stride = zmem::vector_fixed_stride<T>();
               if constexpr (stride == sizeof(T)) {
                  zmem::write_bytes<unchecked>(value.data(), sizeof(T) * count, b, ix);
               }
               else {
                  for (const auto& elem : value) {
                     to<ZMEM, T>::template op<Opts>(elem, ctx, b, ix);
                  }
               }
            }
            else {
               zmem::write_bytes<unchecked>(value.data(), sizeof(T) * count, b, ix);
            }
         }
      }
   };

   // std::vector - variable element types (offset table + self-contained elements)
   template <class T, class Alloc>
      requires(!zmem::is_fixed_type_v<T>)
   struct to<ZMEM, std::vector<T, Alloc>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::vector<T, Alloc>& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         const uint64_t count = value.size();
         zmem::write_value<unchecked>(count, b, ix);

         if (count == 0) {
            return;
         }

         zmem::write_vector_data<Opts>(value, ctx, b, ix);
      }
   };

   // std::string (variable-length)
   template <class CharT, class Traits, class Alloc>
   struct to<ZMEM, std::basic_string<CharT, Traits, Alloc>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::basic_string<CharT, Traits, Alloc>& value,
                                       is_context auto&&, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         // For top-level string serialization:
         // Write length (8 bytes) + raw bytes (NOT null-terminated)
         const uint64_t length = value.size() * sizeof(CharT);
         zmem::write_value<unchecked>(length, b, ix);

         if (length > 0) {
            zmem::write_bytes<unchecked>(value.data(), length, b, ix);
         }
      }
   };

   // std::string_view
   template <class CharT, class Traits>
   struct to<ZMEM, std::basic_string_view<CharT, Traits>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::basic_string_view<CharT, Traits>& value,
                                       is_context auto&&, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         const uint64_t length = value.size() * sizeof(CharT);
         zmem::write_value<unchecked>(length, b, ix);

         if (length > 0) {
            zmem::write_bytes<unchecked>(value.data(), length, b, ix);
         }
      }
   };

   // std::span (for read-only views)
   template <class T, std::size_t Extent>
      requires zmem::is_fixed_type_v<T>
   struct to<ZMEM, std::span<T, Extent>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::span<T, Extent>& value, is_context auto&&, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         if constexpr (Extent == std::dynamic_extent) {
            // Dynamic span: write count + elements
            const uint64_t count = value.size();
            zmem::write_value<unchecked>(count, b, ix);
         }
         // Fixed extent spans write elements directly (count known at compile time)

         if (!value.empty()) {
            zmem::write_bytes<unchecked>(value.data(), sizeof(T) * value.size(), b, ix);
         }
      }
   };

   // std::pair (for map entries)
   template <class K, class V>
   struct to<ZMEM, std::pair<K, V>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::pair<K, V>& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         // Align key
         const size_t key_padding = zmem::padding_for_alignment(ix, alignof(K));
         zmem::write_padding<unchecked>(key_padding, b, ix);
         to<ZMEM, K>::template op<Opts>(value.first, ctx, b, ix);

         // Align value
         const size_t val_padding = zmem::padding_for_alignment(ix, alignof(V));
         zmem::write_padding<unchecked>(val_padding, b, ix);
         to<ZMEM, V>::template op<Opts>(value.second, ctx, b, ix);
      }
   };

   // std::map - ZMEM requires sorted keys (which std::map provides)
   template <class K, class V, class Cmp, class Alloc>
   struct to<ZMEM, std::map<K, V, Cmp, Alloc>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::map<K, V, Cmp, Alloc>& value,
                                       is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         const uint64_t count = value.size();
         const size_t map_start = ix;
         zmem::write_value<unchecked>(count, b, ix);

         if (count == 0) {
            return;
         }

         const size_t map_alignment = zmem::map_data_alignment<K, V>();
         const size_t padding = zmem::padding_for_alignment(ix - map_start, map_alignment);
         if (padding > 0) {
            zmem::write_padding<unchecked>(padding, b, ix);
         }

         const auto entries = zmem::make_sorted_entries(value);
         zmem::write_map_payload_aligned<Opts>(entries, map_start, ctx, b, ix);
      }
   };

   // std::unordered_map - keys are sorted before serialization
   template <class K, class V, class Hash, class Eq, class Alloc>
   struct to<ZMEM, std::unordered_map<K, V, Hash, Eq, Alloc>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::unordered_map<K, V, Hash, Eq, Alloc>& value,
                                       is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         const uint64_t count = value.size();
         const size_t map_start = ix;
         zmem::write_value<unchecked>(count, b, ix);

         if (count == 0) {
            return;
         }

         const size_t map_alignment = zmem::map_data_alignment<K, V>();
         const size_t padding = zmem::padding_for_alignment(ix - map_start, map_alignment);
         if (padding > 0) {
            zmem::write_padding<unchecked>(padding, b, ix);
         }

         const auto entries = zmem::make_sorted_entries(value);
         zmem::write_map_payload_aligned<Opts>(entries, map_start, ctx, b, ix);
      }
   };

   // ============================================================================
   // Fixed Struct Serialization (Trivially Copyable)
   // ============================================================================

   // For fixed aggregate types (trivially copyable structs)
   template <class T>
      requires(std::is_aggregate_v<T> && zmem::is_fixed_type_v<T> && !zmem::is_std_array_v<T>)
   struct to<ZMEM, T> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&&, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         // Fixed struct: direct memcpy
         zmem::write_bytes<unchecked>(&value, sizeof(T), b, ix);
         // Pad to alignment (minimum 8 bytes) for safe zero-copy access
         constexpr size_t alignment = alignof(T) > 8 ? alignof(T) : 8;
         constexpr size_t wire_size = zmem::padded_size(sizeof(T), alignment);
         constexpr size_t padding = wire_size - sizeof(T);
         if constexpr (padding > 0) {
            zmem::write_padding<unchecked>(padding, b, ix);
         }
      }
   };

   // ============================================================================
   // Variable Struct Serialization (Has Vectors/Strings)
   // ============================================================================

   // Variable structs need reflection to handle inline and variable sections.
   // Serialization uses the compile-time inline layout (alignment + offsets) and
   // writes variable payloads with inline-base-relative offsets.

   template <class T>
      requires(!zmem::is_fixed_type_v<T> && !zmem::is_std_array_v<T>
               && (glz::reflectable<T> || glz::glaze_object_t<T>))
   struct to<ZMEM, T> final
   {
      static constexpr auto N = reflect<T>::size;
      using Layout = zmem::inline_layout<T>;

      template <size_t I>
      using member_type_at = field_t<T, I>;

      template <size_t I>
      GLZ_ALWAYS_INLINE static decltype(auto) access_member(auto&& value)
      {
         if constexpr (reflectable<T>) {
            return get_member(value, get<I>(to_tie(value)));
         }
         else {
            return get_member(value, get<I>(reflect<T>::values));
         }
      }

      template <size_t MemberIndex, auto Opts, class B>
      GLZ_ALWAYS_INLINE static void write_variable_member_data(
         const T& value,
         size_t ref_pos,
         size_t inline_base,
         is_context auto&& ctx,
         B& b,
         size_t& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         decltype(auto) member = access_member<MemberIndex>(value);
         using MemberType = member_type_at<MemberIndex>;

         size_t alignment = size_t{8};
         if constexpr (zmem::is_std_vector_v<MemberType>) {
            using ElemType = typename MemberType::value_type;
            alignment = zmem::vector_data_alignment<ElemType>();
         }
         else if constexpr (zmem::is_std_map_like_v<MemberType>) {
            using K = typename MemberType::key_type;
            using V = typename MemberType::mapped_type;
            alignment = zmem::map_data_alignment<K, V>();
         }

         const size_t padding = zmem::padding_for_alignment(ix - inline_base, alignment);
         if (padding > 0) {
            zmem::write_padding<unchecked>(padding, b, ix);
         }

         const uint64_t offset = ix - inline_base;

         if constexpr (zmem::is_std_vector_v<MemberType>) {
            const uint64_t count = member.size();
            if constexpr (std::endian::native == std::endian::little) {
               uint64_t ref_data[2] = {offset, count};
               std::memcpy(b.data() + ref_pos, ref_data, sizeof(ref_data));
            }
            else {
               uint64_t ref_data[2] = {zmem::to_little_endian(offset), zmem::to_little_endian(count)};
               std::memcpy(b.data() + ref_pos, ref_data, sizeof(ref_data));
            }

            if (count > 0) {
               zmem::write_vector_data<Opts>(member, ctx, b, ix);
            }
         }
         else if constexpr (zmem::is_std_string_v<MemberType>) {
            using CharT = typename MemberType::value_type;
            const uint64_t length = member.size() * sizeof(CharT);
            if constexpr (std::endian::native == std::endian::little) {
               uint64_t ref_data[2] = {offset, length};
               std::memcpy(b.data() + ref_pos, ref_data, sizeof(ref_data));
            }
            else {
               uint64_t ref_data[2] = {zmem::to_little_endian(offset), zmem::to_little_endian(length)};
               std::memcpy(b.data() + ref_pos, ref_data, sizeof(ref_data));
            }

            if (length > 0) {
               zmem::write_bytes<unchecked>(member.data(), length, b, ix);
            }
         }
         else if constexpr (zmem::is_std_map_like_v<MemberType>) {
            const auto entries = zmem::make_sorted_entries(member);
            const uint64_t count = entries.size();
            if constexpr (std::endian::native == std::endian::little) {
               uint64_t ref_data[2] = {offset, count};
               std::memcpy(b.data() + ref_pos, ref_data, sizeof(ref_data));
            }
            else {
               uint64_t ref_data[2] = {zmem::to_little_endian(offset), zmem::to_little_endian(count)};
               std::memcpy(b.data() + ref_pos, ref_data, sizeof(ref_data));
            }

            if (count > 0) {
               zmem::write_map_payload_aligned<Opts>(entries, inline_base, ctx, b, ix);
            }
         }
         else {
            const uint64_t offset_le = std::endian::native == std::endian::little
                                          ? offset
                                          : zmem::to_little_endian(offset);
            std::memcpy(b.data() + ref_pos, &offset_le, sizeof(uint64_t));
            to<ZMEM, MemberType>::template op<Opts>(member, ctx, b, ix);
         }
      }

      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&& ctx, B& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();

         constexpr size_t header_plus_inline = 8 + Layout::InlineBasePadding + Layout::InlineSectionSize;
         if constexpr (!unchecked && resizable<std::remove_cvref_t<B>>) {
            const size_t required = ix + header_plus_inline;
            if (required > b.size()) {
               b.resize((std::max)(b.size() * 2, required));
            }
         }

         const size_t size_pos = ix;
         std::memset(b.data() + ix, 0, 8);
         ix += 8;

         if constexpr (Layout::InlineBasePadding > 0) {
            zmem::write_padding<unchecked>(Layout::InlineBasePadding, b, ix);
         }

         const size_t inline_base = ix;
         std::array<size_t, N> ref_positions{};

         for_each<N>([&]<size_t I>() {
            using MemberType = member_type_at<I>;
            constexpr size_t field_alignment = Layout::template field_alignment<I>();
            const size_t pad = zmem::padding_for_alignment(ix - inline_base, field_alignment);
            if (pad > 0) {
               zmem::write_padding<unchecked>(pad, b, ix);
            }

            if constexpr (zmem::is_fixed_type_v<MemberType>) {
               decltype(auto) member = access_member<I>(value);
               std::memcpy(b.data() + ix, &member, sizeof(MemberType));
               ix += sizeof(MemberType);
            }
            else if constexpr (zmem::is_std_vector_v<MemberType> || zmem::is_std_string_v<MemberType>
                               || zmem::is_std_map_like_v<MemberType>) {
               ref_positions[I] = ix;
               std::memset(b.data() + ix, 0, 16);
               ix += 16;
            }
            else {
               ref_positions[I] = ix;
               std::memset(b.data() + ix, 0, 8);
               ix += 8;
            }
         });

         for_each<N>([&]<size_t I>() {
            using MemberType = member_type_at<I>;
            if constexpr (zmem::is_variable_member_type<MemberType>()) {
               write_variable_member_data<I, Opts>(value, ref_positions[I], inline_base, ctx, b, ix);
            }
         });

         const size_t end_padding = zmem::padding_for_alignment(ix - size_pos - 8, 8);
         if (end_padding > 0) {
            zmem::write_padding<unchecked>(end_padding, b, ix);
         }

         const uint64_t total_size = zmem::to_little_endian(static_cast<uint64_t>(ix - size_pos - 8));
         std::memcpy(b.data() + size_pos, &total_size, sizeof(uint64_t));
      }
   };

   // ============================================================================
   // Helper Functions
   // ============================================================================

   namespace detail
   {
      template <auto Opts>
      consteval auto set_zmem()
      {
         auto ret = Opts;
         ret.format = ZMEM;
         return ret;
      }

      template <auto Opts>
      consteval auto set_zmem_unchecked()
      {
         auto ret = Opts;
         ret.format = ZMEM;
         ret.internal |= static_cast<uint32_t>(opts_internal::write_unchecked);
         return ret;
      }
   }

   template <auto Opts>
   consteval auto set_zmem()
   {
      return detail::set_zmem<Opts>();
   }

   template <auto Opts>
   consteval auto set_zmem_unchecked()
   {
      return detail::set_zmem_unchecked<Opts>();
   }

   // ============================================================================
   // Public API Functions
   // ============================================================================

   // Write ZMEM to buffer
   template <auto Opts = opts{.format = ZMEM}, class T, output_buffer Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE error_ctx write_zmem(T&& value, Buffer&& buffer)
   {
      return write<set_zmem<Opts>()>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   // Write ZMEM to new string
   template <auto Opts = opts{.format = ZMEM}, class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE expected<std::string, error_ctx> write_zmem(T&& value)
   {
      return write<set_zmem<Opts>()>(std::forward<T>(value));
   }

   // Write ZMEM to file
   template <auto Opts = opts{.format = ZMEM}, class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE error_ctx write_file_zmem(T&& value, const std::string_view file_name,
                                                              auto&& buffer)
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      const auto ec = write<set_zmem<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec;
      }

      std::ofstream file(file_name.data(), std::ios::binary);
      if (file) {
         file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
      }
      else {
         return {.ec = error_code::file_open_failure};
      }

      return {};
   }

   // ============================================================================
   // Pre-allocated Write API
   // ============================================================================
   //
   // These functions pre-compute the exact serialized size, allocate once,
   // then write with all resize checks disabled for maximum performance.
   //

   // Write ZMEM to pre-allocated buffer (no resize checks)
   // Buffer must already be sized to at least size_zmem(value) bytes
   template <auto Opts = opts{.format = ZMEM}, class T, class Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE error_ctx write_zmem_unchecked(T&& value, Buffer&& buffer, size_t& bytes_written)
   {
      bytes_written = 0;
      context ctx{};
      serialize<ZMEM>::op<set_zmem_unchecked<Opts>()>(value, ctx, buffer, bytes_written);
      return {.ec = ctx.error};
   }

   // Write ZMEM with automatic pre-allocation (compute size, allocate, write unchecked)
   // This is the fastest way to serialize when you don't have a reusable buffer
   template <auto Opts = opts{.format = ZMEM}, class T, output_buffer Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE error_ctx write_zmem_preallocated(T&& value, Buffer&& buffer)
   {
      // Step 1: Compute exact serialized size
      const size_t required_size = size_zmem(value);

      // Step 2: Pre-allocate buffer to exact size
      buffer.resize(required_size);

      // Step 3: Write with all resize checks disabled
      size_t bytes_written = 0;
      context ctx{};
      serialize<ZMEM>::op<set_zmem_unchecked<Opts>()>(value, ctx, buffer, bytes_written);

      // Trim buffer to actual bytes written (should match required_size)
      if (bytes_written != required_size) {
         buffer.resize(bytes_written);
      }

      return {.ec = ctx.error};
   }

   // Write ZMEM to new string with pre-allocation (fastest for new allocations)
   template <auto Opts = opts{.format = ZMEM}, class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE expected<std::string, error_ctx> write_zmem_preallocated(T&& value)
   {
      std::string buffer;
      const auto ec = write_zmem_preallocated<Opts>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return unexpected(ec);
      }
      return buffer;
   }

} // namespace glz
