// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/zmem/header.hpp"
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

         const size_t data_size = sizeof(T) * count;
         if (static_cast<size_t>(end - it) < data_size) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         value.resize(count);
         if (count > 0) {
            std::memcpy(value.data(), &(*it), data_size);
            it += data_size;
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
            from<ZMEM, T>::template op<Opts>(value[i], ctx, elem_it, elem_end);
            if (bool(ctx.error)) return;
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

         if constexpr (zmem::is_fixed_type_v<V>) {
            // Fixed value map: count + entries (fixed-size, no offset table)
            if (static_cast<size_t>(end - it) < sizeof(uint64_t)) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            uint64_t count;
            std::memcpy(&count, &(*it), sizeof(uint64_t));
            zmem::byteswap_le(count);
            it += sizeof(uint64_t);

            for (size_t i = 0; i < count; ++i) {
               std::pair<K, V> entry;
               from<ZMEM, std::pair<K, V>>::template op<Opts>(entry, ctx, it, end);
               if (bool(ctx.error)) return;
               value.emplace(std::move(entry));
            }
         }
         else {
            // Variable value map format:
            // [size:8][count:8][offset_table:(count+1)*8][entries...]
            if (static_cast<size_t>(end - it) < sizeof(uint64_t) * 2) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            uint64_t size;
            std::memcpy(&size, &(*it), sizeof(uint64_t));
            zmem::byteswap_le(size);
            it += sizeof(uint64_t);

            uint64_t count;
            std::memcpy(&count, &(*it), sizeof(uint64_t));
            zmem::byteswap_le(count);
            it += sizeof(uint64_t);

            if (count == 0) {
               return;
            }

            // Skip offset table (we read entries sequentially, but the table is there for random access)
            const size_t offset_table_size = (count + 1) * sizeof(uint64_t);
            if (static_cast<size_t>(end - it) < offset_table_size) {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += offset_table_size;

            // Read entries sequentially
            // Key and value are serialized directly without alignment padding
            for (size_t i = 0; i < count; ++i) {
               K key;
               from<ZMEM, K>::template op<Opts>(key, ctx, it, end);
               if (bool(ctx.error)) return;

               V val;
               from<ZMEM, V>::template op<Opts>(val, ctx, it, end);
               if (bool(ctx.error)) return;

               value.emplace(std::move(key), std::move(val));
            }
         }
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
         // Read as vector of pairs (sorted in ZMEM format)
         std::vector<std::pair<K, V>> entries;
         from<ZMEM, std::vector<std::pair<K, V>>>::template op<Opts>(entries, ctx, it, end);
         if (bool(ctx.error)) return;

         value.clear();
         value.reserve(entries.size());
         for (auto& entry : entries) {
            value.emplace(std::move(entry));
         }
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
         // Fixed struct: direct memcpy
         if (static_cast<size_t>(end - it) < sizeof(T)) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         std::memcpy(&value, &(*it), sizeof(T));
         it += sizeof(T);
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
         // Variable struct format: [size:8][inline section][variable section]

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

         auto base = it; // Reference point for offsets (byte 8)
         auto struct_end = it + total_size;

         // Read fields using reflection
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) return;

            decltype(auto) member = access_member<I>(value);
            using MemberType = member_type_at<I>;

            if constexpr (zmem::is_fixed_type_v<MemberType>) {
               from<ZMEM, MemberType>::template op<Opts>(member, ctx, it, struct_end);
            }
            else if constexpr (zmem::is_std_vector_v<MemberType>) {
               // Read vector reference
               if (static_cast<size_t>(struct_end - it) < sizeof(zmem::vector_ref)) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               zmem::vector_ref ref;
               std::memcpy(&ref, &(*it), sizeof(zmem::vector_ref));
               zmem::byteswap_le(ref.offset);
               zmem::byteswap_le(ref.count);
               it += sizeof(zmem::vector_ref);

               // Read vector data from variable section
               using ElemType = typename MemberType::value_type;
               member.resize(ref.count);

               if (ref.count > 0) {
                  auto data_it = base + ref.offset;
                  if constexpr (zmem::is_fixed_type_v<ElemType>) {
                     const size_t data_size = sizeof(ElemType) * ref.count;
                     std::memcpy(member.data(), &(*data_it), data_size);
                  }
                  else {
                     // Variable elements
                     for (size_t i = 0; i < ref.count; ++i) {
                        from<ZMEM, ElemType>::template op<Opts>(member[i], ctx, data_it, struct_end);
                        if (bool(ctx.error)) return;
                     }
                  }
               }
            }
            else if constexpr (zmem::is_std_string_v<MemberType>) {
               // Read string reference
               if (static_cast<size_t>(struct_end - it) < sizeof(zmem::string_ref)) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               zmem::string_ref ref;
               std::memcpy(&ref, &(*it), sizeof(zmem::string_ref));
               zmem::byteswap_le(ref.offset);
               zmem::byteswap_le(ref.length);
               it += sizeof(zmem::string_ref);

               // Read string data from variable section
               member.resize(ref.length);
               if (ref.length > 0) {
                  auto data_it = base + ref.offset;
                  std::memcpy(member.data(), &(*data_it), ref.length);
               }
            }
            else {
               // Nested variable type
               from<ZMEM, MemberType>::template op<Opts>(member, ctx, it, struct_end);
            }
         });

         // Ensure we've consumed the full struct
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
