// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/zmem/header.hpp"
#include "glaze/zmem/layout.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/core/common.hpp"
#include "glaze/concepts/container_concepts.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/reflection/to_tuple.hpp"

#include <map>

namespace glz
{
   // ============================================================================
   // ZMEM Parse Entry Point
   // ============================================================================

   template <>
   struct parse<ZMEM>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end)
      {
         using V = std::remove_cvref_t<T>;
         from<ZMEM, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                          std::forward<It0>(it), std::forward<It1>(end));
      }
   };

   namespace zmem
   {
      template <class T>
      GLZ_ALWAYS_INLINE void read_fixed_raw(T& value, const char* data) noexcept
      {
         if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            value = read_value<T>(data);
         }
         else {
            std::memcpy(&value, data, sizeof(T));
         }
      }

      template <auto Opts, class ElemType>
      GLZ_ALWAYS_INLINE void read_vector_payload(std::vector<ElemType>& value,
                                                 uint64_t count,
                                                 const char* data,
                                                 const char* struct_end,
                                                 is_context auto&& ctx)
      {
         value.resize(count);
         if (count == 0) {
            return;
         }

         if constexpr (is_fixed_type_v<ElemType>) {
            if constexpr (std::is_aggregate_v<ElemType> && !is_std_array_v<ElemType>) {
               constexpr size_t stride = vector_fixed_stride<ElemType>();
               const size_t data_size = stride * count;
               if (data + data_size > struct_end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (stride == sizeof(ElemType)) {
                  std::memcpy(value.data(), data, sizeof(ElemType) * count);
               }
               else {
                  for (size_t i = 0; i < count; ++i) {
                     std::memcpy(&value[i], data + i * stride, sizeof(ElemType));
                  }
               }
            }
            else {
               const size_t data_size = sizeof(ElemType) * count;
               if (data + data_size > struct_end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               std::memcpy(value.data(), data, data_size);
            }
         }
         else {
            const size_t offset_table_size = (count + 1) * sizeof(uint64_t);
            if (data + offset_table_size > struct_end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            uint64_t stack_offsets[offset_table_stack_threshold + 1];
            std::vector<uint64_t> heap_offsets;
            uint64_t* offsets;

            if (count <= offset_table_stack_threshold) {
               offsets = stack_offsets;
            }
            else {
               heap_offsets.resize(count + 1);
               offsets = heap_offsets.data();
            }

            std::memcpy(offsets, data, offset_table_size);

            if constexpr (std::endian::native == std::endian::big) {
               for (size_t i = 0; i <= count; ++i) {
                  offsets[i] = std::byteswap(offsets[i]);
               }
            }

            const char* data_start = data + offset_table_size;
            for (size_t i = 0; i < count; ++i) {
               const char* elem_it = data_start + offsets[i];
               const char* elem_end = data_start + offsets[i + 1];
               if (elem_end > struct_end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (is_std_string_v<ElemType>) {
                  using CharT = typename ElemType::value_type;
                  const size_t length_bytes = static_cast<size_t>(elem_end - elem_it);
                  value[i].resize(length_bytes / sizeof(CharT));
                  if (length_bytes > 0) {
                     std::memcpy(value[i].data(), elem_it, length_bytes);
                  }
               }
               else {
                  auto elem_ptr = elem_it;
                  from<ZMEM, ElemType>::template op<Opts>(value[i], ctx, elem_ptr, struct_end);
                  if (bool(ctx.error)) return;
               }
            }
         }
      }

      template <auto Opts, class Map>
      GLZ_ALWAYS_INLINE void read_map_payload(Map& value,
                                              const char* entries_start,
                                              uint64_t count,
                                              const char* inline_base,
                                              const char* struct_end,
                                              is_context auto&& ctx)
      {
         using K = typename Map::key_type;
         using V = typename Map::mapped_type;

         value.clear();
         if (count == 0) {
            return;
         }

         constexpr size_t entry_stride = map_entry_stride<K, V>();
         constexpr size_t value_offset = map_value_offset_in_entry<K, V>();

         const char* entries_end = entries_start + entry_stride * count;
         if (entries_end > struct_end) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if constexpr (is_fixed_type_v<V>) {
            for (size_t i = 0; i < count; ++i) {
               const char* entry_ptr = entries_start + i * entry_stride;
               K key{};
               read_fixed_raw(key, entry_ptr);
               const char* value_ptr = entry_ptr + value_offset;
               V val{};
               read_fixed_raw(val, value_ptr);
               value.emplace(std::move(key), std::move(val));
            }
         }
         else {
            struct entry_ref {
               K key{};
               uint64_t offset{};
               uint64_t aux{};
            };

            std::vector<entry_ref> refs;
            refs.reserve(static_cast<size_t>(count));

            for (size_t i = 0; i < count; ++i) {
               const char* entry_ptr = entries_start + i * entry_stride;
               entry_ref ref{};
               read_fixed_raw(ref.key, entry_ptr);
               ref.offset = read_value<uint64_t>(entry_ptr + value_offset);

               if constexpr (is_std_vector_v<V> || is_std_string_v<V>) {
                  ref.aux = read_value<uint64_t>(entry_ptr + value_offset + 8);
               }

               refs.emplace_back(std::move(ref));
            }

            for (auto& ref : refs) {
               if (inline_base + ref.offset > struct_end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (is_std_vector_v<V>) {
                  V vals;
                  read_vector_payload<Opts>(vals, ref.aux, inline_base + ref.offset, struct_end, ctx);
                  if (bool(ctx.error)) return;
                  value.emplace(std::move(ref.key), std::move(vals));
               }
               else if constexpr (is_std_string_v<V>) {
                  V s;
                  const size_t length = static_cast<size_t>(ref.aux);
                  if (inline_base + ref.offset + length > struct_end) {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  s.resize(length / sizeof(typename V::value_type));
                  if (length > 0) {
                     std::memcpy(s.data(), inline_base + ref.offset, length);
                  }
                  value.emplace(std::move(ref.key), std::move(s));
               }
               else {
                  V val{};
                  auto it = inline_base + ref.offset;
                  from<ZMEM, V>::template op<Opts>(val, ctx, it, struct_end);
                  if (bool(ctx.error)) return;
                  value.emplace(std::move(ref.key), std::move(val));
               }
            }
         }
      }
   } // namespace zmem

   // ============================================================================
   // Read Specializations
   // ============================================================================

   // Primitives (bool, integers, floats, enums)
   template <class T>
      requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
   struct from<ZMEM, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         if (static_cast<size_t>(end - it) < sizeof(T)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         std::memcpy(&value, &(*it), sizeof(T));
         zmem::byteswap_le(value);
         it += sizeof(T);
      }
   };

   // Fixed-size C arrays
   template <class T, std::size_t N>
   struct from<ZMEM, T[N]> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T (&value)[N], is_context auto&& ctx, auto&& it, auto&& end)
      {
         if constexpr (zmem::is_fixed_type_v<T>) {
            const size_t size = sizeof(T) * N;
            if (static_cast<size_t>(end - it) < size) {
               ctx.error = error_code::unexpected_end;
               return;
            }
            std::memcpy(value, &(*it), size);
            it += size;
         }
         else {
            for (std::size_t i = 0; i < N; ++i) {
               from<ZMEM, T>::template op<Opts>(value[i], ctx, it, end);
               if (bool(ctx.error)) return;
            }
         }
      }
   };

   // std::array
   template <class T, std::size_t N>
   struct from<ZMEM, std::array<T, N>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::array<T, N>& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         if constexpr (zmem::is_fixed_type_v<T>) {
            const size_t size = sizeof(T) * N;
            if (static_cast<size_t>(end - it) < size) {
               ctx.error = error_code::unexpected_end;
               return;
            }
            std::memcpy(value.data(), &(*it), size);
            it += size;
         }
         else {
            for (std::size_t i = 0; i < N; ++i) {
               from<ZMEM, T>::template op<Opts>(value[i], ctx, it, end);
               if (bool(ctx.error)) return;
            }
         }
      }
   };

   // ZMEM optional
   template <class T>
   struct from<ZMEM, zmem::optional<T>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(zmem::optional<T>& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         constexpr size_t opt_size = sizeof(zmem::optional<T>);
         if (static_cast<size_t>(end - it) < opt_size) {
            ctx.error = error_code::unexpected_end;
            return;
         }
         std::memcpy(&value, &(*it), opt_size);
         // Note: We need to byteswap the value if present
         if (value.has_value()) {
            zmem::byteswap_le(value.value);
         }
         it += opt_size;
      }
   };

   // std::optional <- zmem::optional conversion on read
   template <class T>
   struct from<ZMEM, std::optional<T>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::optional<T>& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         zmem::optional<T> zmem_opt;
         from<ZMEM, zmem::optional<T>>::template op<Opts>(zmem_opt, ctx, it, end);
         if (bool(ctx.error)) return;

         if (zmem_opt.has_value()) {
            value = zmem_opt.value;
         }
         else {
            value = std::nullopt;
         }
      }
   };

   // std::vector - fixed element types
   template <class T, class Alloc>
      requires zmem::is_fixed_type_v<T>
   struct from<ZMEM, std::vector<T, Alloc>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::vector<T, Alloc>& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         // Array message format: count + elements
         if (static_cast<size_t>(end - it) < sizeof(uint64_t)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         uint64_t count;
         std::memcpy(&count, &(*it), sizeof(uint64_t));
         zmem::byteswap_le(count);
         it += sizeof(uint64_t);

         value.resize(count);
         if (count > 0) {
            if constexpr (std::is_aggregate_v<T> && !zmem::is_std_array_v<T>) {
               // Fixed struct elements have padding to alignment (min 8 bytes)
               constexpr size_t wire_size = zmem::vector_fixed_stride<T>();
               const size_t data_size = wire_size * count;
               if (static_cast<size_t>(end - it) < data_size) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (wire_size == sizeof(T)) {
                  // No padding, can memcpy all at once
                  std::memcpy(value.data(), &(*it), sizeof(T) * count);
               }
               else {
                  // Has padding, copy each element
                  for (size_t i = 0; i < count; ++i) {
                     std::memcpy(&value[i], &(*it) + i * wire_size, sizeof(T));
                  }
               }
               it += data_size;
            }
            else {
               // Primitives and arrays: no padding
               const size_t data_size = sizeof(T) * count;
               if (static_cast<size_t>(end - it) < data_size) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               std::memcpy(value.data(), &(*it), data_size);
               it += data_size;
            }
         }
      }
   };

   // std::vector - variable element types
   template <class T, class Alloc>
      requires(!zmem::is_fixed_type_v<T>)
   struct from<ZMEM, std::vector<T, Alloc>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::vector<T, Alloc>& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         if (static_cast<size_t>(end - it) < sizeof(uint64_t)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         uint64_t count;
         std::memcpy(&count, &(*it), sizeof(uint64_t));
         zmem::byteswap_le(count);
         it += sizeof(uint64_t);

         if (count == 0) {
            value.clear();
            return;
         }

         // Read offset table
         const size_t offset_table_size = (count + 1) * sizeof(uint64_t);
         if (static_cast<size_t>(end - it) < offset_table_size) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         // Use stack allocation for small arrays, heap for large
         // Stack: up to 64 elements (512 bytes), avoids heap allocation in hot path
         uint64_t stack_offsets[zmem::offset_table_stack_threshold + 1];
         std::vector<uint64_t> heap_offsets;
         uint64_t* offsets;

         if (count <= zmem::offset_table_stack_threshold) {
            offsets = stack_offsets;
         } else {
            heap_offsets.resize(count + 1);
            offsets = heap_offsets.data();
         }

         // Bulk copy offset table then byteswap
         std::memcpy(offsets, &(*it), offset_table_size);
         it += offset_table_size;

         // Byteswap all offsets (no-op on little-endian systems)
         if constexpr (std::endian::native == std::endian::big) {
            for (size_t i = 0; i <= count; ++i) {
               offsets[i] = std::byteswap(offsets[i]);
            }
         }

         // Now read each element
         auto data_start = it;
         value.resize(count);

         for (size_t i = 0; i < count; ++i) {
            auto elem_it = data_start + offsets[i];
            auto elem_end = data_start + offsets[i + 1];

            if constexpr (zmem::is_std_string_v<T>) {
               using CharT = typename T::value_type;
               const size_t length_bytes = static_cast<size_t>(elem_end - elem_it);
               value[i].resize(length_bytes / sizeof(CharT));
               if (length_bytes > 0) {
                  std::memcpy(value[i].data(), &(*elem_it), length_bytes);
               }
            }
            else {
               from<ZMEM, T>::template op<Opts>(value[i], ctx, elem_it, elem_end);
               if (bool(ctx.error)) return;
            }
         }

         // Advance iterator past all data
         it = data_start + offsets[count];
      }
   };

   // std::string
   template <class CharT, class Traits, class Alloc>
   struct from<ZMEM, std::basic_string<CharT, Traits, Alloc>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::basic_string<CharT, Traits, Alloc>& value,
                                       is_context auto&& ctx, auto&& it, auto&& end)
      {
         if (static_cast<size_t>(end - it) < sizeof(uint64_t)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         uint64_t length;
         std::memcpy(&length, &(*it), sizeof(uint64_t));
         zmem::byteswap_le(length);
         it += sizeof(uint64_t);

         if (static_cast<size_t>(end - it) < length) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         value.resize(length / sizeof(CharT));
         if (length > 0) {
            std::memcpy(value.data(), &(*it), length);
            it += length;
         }
      }
   };

   // std::pair (for map entries)
   template <class K, class V>
   struct from<ZMEM, std::pair<K, V>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::pair<K, V>& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         from<ZMEM, K>::template op<Opts>(value.first, ctx, it, end);
         if (bool(ctx.error)) return;
         from<ZMEM, V>::template op<Opts>(value.second, ctx, it, end);
      }
   };

   // std::map
   template <class K, class V, class Cmp, class Alloc>
   struct from<ZMEM, std::map<K, V, Cmp, Alloc>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::map<K, V, Cmp, Alloc>& value,
                                       is_context auto&& ctx, auto&& it, auto&& end)
      {
         value.clear();

         if (static_cast<size_t>(end - it) < sizeof(uint64_t)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const char* map_start = &(*it);
         const char* map_end = map_start + static_cast<size_t>(end - it);

         uint64_t count;
         std::memcpy(&count, &(*it), sizeof(uint64_t));
         zmem::byteswap_le(count);
         it += sizeof(uint64_t);

         if (count == 0) {
            return;
         }

         const size_t map_alignment = zmem::map_data_alignment<K, V>();
         const size_t padding = zmem::padding_for_alignment(static_cast<size_t>(&(*it) - map_start), map_alignment);
         if (static_cast<size_t>(end - it) < padding) {
            ctx.error = error_code::unexpected_end;
            return;
         }
         it += padding;

         const char* entries_start = &(*it);
         zmem::read_map_payload<Opts>(value, entries_start, count, map_start, map_end, ctx);
         if (bool(ctx.error)) return;

         it = end;
      }
   };

   // std::unordered_map
   template <class K, class V, class Hash, class Eq, class Alloc>
   struct from<ZMEM, std::unordered_map<K, V, Hash, Eq, Alloc>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(std::unordered_map<K, V, Hash, Eq, Alloc>& value,
                                       is_context auto&& ctx, auto&& it, auto&& end)
      {
         value.clear();

         if (static_cast<size_t>(end - it) < sizeof(uint64_t)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const char* map_start = &(*it);
         const char* map_end = map_start + static_cast<size_t>(end - it);

         uint64_t count;
         std::memcpy(&count, &(*it), sizeof(uint64_t));
         zmem::byteswap_le(count);
         it += sizeof(uint64_t);

         if (count == 0) {
            return;
         }

         const size_t map_alignment = zmem::map_data_alignment<K, V>();
         const size_t padding = zmem::padding_for_alignment(static_cast<size_t>(&(*it) - map_start), map_alignment);
         if (static_cast<size_t>(end - it) < padding) {
            ctx.error = error_code::unexpected_end;
            return;
         }
         it += padding;

         const char* entries_start = &(*it);
         zmem::read_map_payload<Opts>(value, entries_start, count, map_start, map_end, ctx);
         if (bool(ctx.error)) return;

         it = end;
      }
   };

   // ============================================================================
   // Fixed Struct Deserialization
   // ============================================================================

   template <class T>
      requires(std::is_aggregate_v<T> && zmem::is_fixed_type_v<T> && !zmem::is_std_array_v<T>)
   struct from<ZMEM, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         // Fixed struct: direct memcpy + skip padding to alignment (min 8 bytes)
         constexpr size_t alignment = alignof(T) > 8 ? alignof(T) : 8;
         constexpr size_t wire_size = zmem::padded_size(sizeof(T), alignment);
         if (static_cast<size_t>(end - it) < wire_size) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         std::memcpy(&value, &(*it), sizeof(T));
         it += wire_size;  // Skip padding bytes too
      }
   };

   // ============================================================================
   // Variable Struct Deserialization
   // ============================================================================

   template <class T>
      requires(!zmem::is_fixed_type_v<T> && !zmem::is_std_array_v<T>
               && (glz::reflectable<T> || glz::glaze_object_t<T>))
   struct from<ZMEM, T> final
   {
      static constexpr auto N = reflect<T>::size;

      // Member type helper (works for both reflectable and glaze_object_t)
      template <size_t I>
      using member_type_at = field_t<T, I>;

      // Member access helper (works for both reflectable and glaze_object_t)
      template <size_t I>
      GLZ_ALWAYS_INLINE static decltype(auto) access_member(auto&& value) {
         if constexpr (reflectable<T>) {
            return get_member(value, get<I>(to_tie(value)));
         }
         else {
            return get_member(value, get<I>(reflect<T>::values));
         }
      }

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         if (static_cast<size_t>(end - it) < sizeof(uint64_t)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         uint64_t total_size;
         std::memcpy(&total_size, &(*it), sizeof(uint64_t));
         zmem::byteswap_le(total_size);
         it += sizeof(uint64_t);

         if (static_cast<size_t>(end - it) < total_size) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const char* struct_start = &(*it);
         const char* struct_end = struct_start + total_size;
         const char* inline_base = struct_start + zmem::inline_layout<T>::InlineBasePadding;

         if (inline_base + zmem::inline_layout<T>::InlineSectionSize > struct_end) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) return;

            using MemberType = member_type_at<I>;
            const char* field_ptr = inline_base + zmem::inline_layout<T>::Offsets[I];

            decltype(auto) member = access_member<I>(value);

            if constexpr (zmem::is_fixed_type_v<MemberType>) {
               std::memcpy(&member, field_ptr, sizeof(MemberType));
            }
            else if constexpr (zmem::is_std_vector_v<MemberType>) {
               zmem::vector_ref ref{};
               std::memcpy(&ref, field_ptr, sizeof(zmem::vector_ref));
               zmem::byteswap_le(ref.offset);
               zmem::byteswap_le(ref.count);

               if (ref.count == 0) {
                  member.clear();
                  return;
               }

               zmem::read_vector_payload<Opts>(member, ref.count, inline_base + ref.offset, struct_end, ctx);
            }
            else if constexpr (zmem::is_std_string_v<MemberType>) {
               zmem::string_ref ref{};
               std::memcpy(&ref, field_ptr, sizeof(zmem::string_ref));
               zmem::byteswap_le(ref.offset);
               zmem::byteswap_le(ref.length);

               member.resize(ref.length / sizeof(typename MemberType::value_type));
               if (ref.length > 0) {
                  if (inline_base + ref.offset + ref.length > struct_end) {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  std::memcpy(member.data(), inline_base + ref.offset, ref.length);
               }
            }
            else if constexpr (zmem::is_std_map_like_v<MemberType>) {
               zmem::map_ref ref{};
               std::memcpy(&ref, field_ptr, sizeof(zmem::map_ref));
               zmem::byteswap_le(ref.offset);
               zmem::byteswap_le(ref.count);

               zmem::read_map_payload<Opts>(member, inline_base + ref.offset, ref.count, inline_base, struct_end, ctx);
            }
            else {
               uint64_t offset;
               std::memcpy(&offset, field_ptr, sizeof(uint64_t));
               zmem::byteswap_le(offset);

               auto nested_it = inline_base + offset;
               from<ZMEM, MemberType>::template op<Opts>(member, ctx, nested_it, struct_end);
            }
         });

         it = struct_end;
      }
   };

   // ============================================================================
   // Public API Functions
   // ============================================================================

   // Read ZMEM from buffer
   template <auto Opts = opts{.format = ZMEM}, class T, contiguous Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE error_ctx read_zmem(T&& value, Buffer&& buffer)
   {
      return read<set_zmem<Opts>()>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   // Read ZMEM from buffer and return value
   template <class T, auto Opts = opts{.format = ZMEM}, contiguous Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE expected<T, error_ctx> read_zmem(Buffer&& buffer)
   {
      return read<T, set_zmem<Opts>()>(std::forward<Buffer>(buffer));
   }

   // Read ZMEM from file
   template <auto Opts = opts{.format = ZMEM}, class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE error_ctx read_file_zmem(T&& value, const std::string_view file_name,
                                                            auto&& buffer)
   {
      const auto ec = file_to_buffer(buffer, file_name);
      if (bool(ec)) [[unlikely]] {
         return {ec};
      }

      return read<set_zmem<Opts>()>(std::forward<T>(value), buffer);
   }

} // namespace glz
