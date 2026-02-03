// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/zmem/header.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/core/common.hpp"
#include "glaze/concepts/container_concepts.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/reflection/to_tuple.hpp"

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
         if constexpr (zmem::is_simple_type_v<T>) {
            // For simple element types, write contiguously
            zmem::write_bytes<unchecked>(value, sizeof(T) * N, b, ix);
         }
         else {
            // For complex element types, serialize each element
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
         if constexpr (zmem::is_simple_type_v<T>) {
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

   // std::vector - simple element types (contiguous)
   template <class T, class Alloc>
      requires zmem::is_simple_type_v<T>
   struct to<ZMEM, std::vector<T, Alloc>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::vector<T, Alloc>& value, is_context auto&&, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         // For top-level vector serialization (Array message format):
         // Write count (8 bytes) + elements
         const uint64_t count = value.size();
         zmem::write_value<unchecked>(count, b, ix);

         if (count > 0) {
            zmem::write_bytes<unchecked>(value.data(), sizeof(T) * count, b, ix);
         }
      }
   };

   // std::vector - complex element types (offset table + self-contained elements)
   template <class T, class Alloc>
      requires(!zmem::is_simple_type_v<T>)
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

         // For complex elements, we need an offset table
         // Reserve space for offset table: (count + 1) * 8 bytes
         const size_t offset_table_start = ix;
         const size_t offset_table_size = (count + 1) * sizeof(uint64_t);

         // Write placeholder offsets (will be patched later)
         // Batch the placeholder writes for efficiency
         if constexpr (!unchecked && resizable<std::remove_cvref_t<B>>) {
            if (ix + offset_table_size > b.size()) {
               b.resize((std::max)(b.size() * 2, ix + offset_table_size));
            }
         }
         std::memset(b.data() + ix, 0, offset_table_size);
         ix += offset_table_size;

         const size_t data_section_start = ix;

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

         // Serialize each element and record offsets
         for (size_t i = 0; i < count; ++i) {
            offsets[i] = ix - data_section_start;
            to<ZMEM, T>::template op<Opts>(value[i], ctx, b, ix);
         }
         offsets[count] = ix - data_section_start; // Sentinel

         // Patch offset table - batch write with endian conversion
         // On little-endian systems (most modern CPUs), skip the conversion loop entirely
         if constexpr (std::endian::native == std::endian::big) {
            for (size_t i = 0; i <= count; ++i) {
               offsets[i] = std::byteswap(offsets[i]);
            }
         }
         std::memcpy(b.data() + offset_table_start, offsets, offset_table_size);
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
      requires zmem::is_simple_type_v<T>
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
         if constexpr (zmem::is_simple_type_v<V>) {
            // Simple map: count + entries (fixed-size, no offset table needed)
            const uint64_t count = value.size();
            zmem::write_value<unchecked>(count, b, ix);

            for (const auto& [k, v] : value) {
               to<ZMEM, std::pair<const K, V>>::template op<Opts>(std::pair<const K, V>{k, v}, ctx, b, ix);
            }
         }
         else {
            // Complex map format:
            // [size:8][count:8][offset_table:(count+1)*8][entries...]
            // This enables O(log N) binary search on sorted keys

            // Reserve space for size header
            const size_t size_pos = ix;
            zmem::write_value<unchecked, uint64_t>(0, b, ix); // Placeholder

            const uint64_t count = value.size();
            zmem::write_value<unchecked>(count, b, ix);

            if (count == 0) {
               // Patch size (just the count field, 8 bytes)
               const uint64_t total_size = zmem::to_little_endian(static_cast<uint64_t>(8));
               std::memcpy(b.data() + size_pos, &total_size, sizeof(uint64_t));
               return;
            }

            // Reserve space for offset table: (count + 1) * 8 bytes
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

            // Use stack allocation for small maps, heap for large
            uint64_t stack_offsets[zmem::offset_table_stack_threshold + 1];
            std::vector<uint64_t> heap_offsets;
            uint64_t* offsets;

            if (count <= zmem::offset_table_stack_threshold) {
               offsets = stack_offsets;
            } else {
               heap_offsets.resize(count + 1);
               offsets = heap_offsets.data();
            }

            // Serialize each entry and record offsets
            // For complex maps, serialize key and value directly without alignment padding
            // (the complex value's 8-byte header provides natural alignment)
            size_t entry_idx = 0;
            for (const auto& [k, v] : value) {
               offsets[entry_idx++] = ix - data_section_start;
               // Write key directly
               to<ZMEM, K>::template op<Opts>(k, ctx, b, ix);
               // Write value directly (complex value has its own size header)
               to<ZMEM, V>::template op<Opts>(v, ctx, b, ix);
            }
            offsets[count] = ix - data_section_start; // Sentinel

            // Patch offset table - batch write with endian conversion
            if constexpr (std::endian::native == std::endian::big) {
               for (size_t i = 0; i <= count; ++i) {
                  offsets[i] = std::byteswap(offsets[i]);
               }
            }
            std::memcpy(b.data() + offset_table_start, offsets, offset_table_size);

            // Patch size
            const uint64_t total_size = zmem::to_little_endian(static_cast<uint64_t>(ix - size_pos - 8));
            std::memcpy(b.data() + size_pos, &total_size, sizeof(uint64_t));
         }
      }
   };

   // std::unordered_map - needs sorting before serialization
   template <class K, class V, class Hash, class Eq, class Alloc>
   struct to<ZMEM, std::unordered_map<K, V, Hash, Eq, Alloc>> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const std::unordered_map<K, V, Hash, Eq, Alloc>& value,
                                       is_context auto&& ctx, B&& b, auto& ix)
      {
         // Convert to sorted vector of pairs
         std::vector<std::pair<K, V>> sorted_entries(value.begin(), value.end());
         std::sort(sorted_entries.begin(), sorted_entries.end(),
                   [](const auto& a, const auto& b) { return a.first < b.first; });

         // Use vector serialization
         to<ZMEM, std::vector<std::pair<K, V>>>::template op<Opts>(sorted_entries, ctx, b, ix);
      }
   };

   // ============================================================================
   // Simple Struct Serialization (Trivially Copyable)
   // ============================================================================

   // For simple aggregate types (trivially copyable structs)
   template <class T>
      requires(std::is_aggregate_v<T> && zmem::is_simple_type_v<T> && !zmem::is_std_array_v<T>)
   struct to<ZMEM, T> final
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&&, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         // Simple struct: direct memcpy with zero overhead
         zmem::write_bytes<unchecked>(&value, sizeof(T), b, ix);
      }
   };

   // ============================================================================
   // Complex Struct Serialization (Has Vectors/Strings)
   // ============================================================================

   // Complex structs need reflection to handle inline and variable sections
   // This requires glaze metadata to be defined for the struct
   //
   // OPTIMIZATION: Single-pass serialization with compile-time layout
   // - Variable member indices are computed at compile time
   // - No runtime dispatch (O(1) instead of O(N) per variable member)
   // - No deferred writes array needed

   // Forward declaration for recursive inline size calculation
   template <class T>
   struct inline_section_size_calculator;

   template <class T>
      requires(!zmem::is_simple_type_v<T> && !zmem::is_std_array_v<T>
               && (glz::reflectable<T> || glz::glaze_object_t<T>))
   struct to<ZMEM, T> final
   {
      static constexpr auto N = reflect<T>::size;

      // ========================================================================
      // Compile-Time Member Type Extraction
      // ========================================================================

      // Use field_t which works for both reflectable and glaze_object_t types
      template <size_t I>
      using member_type_at = field_t<T, I>;

      // ========================================================================
      // Compile-Time Variable Member Index Computation
      // ========================================================================

      // Check if member at index I is a variable member (vector or string)
      template <size_t I>
      static consteval bool is_variable_member() {
         if constexpr (I >= N) {
            return false;
         } else {
            using MemberType = member_type_at<I>;
            return zmem::is_std_vector_v<MemberType> || zmem::is_std_string_v<MemberType>;
         }
      }

      // Count variable members at compile time
      static consteval size_t count_variable_members() {
         return []<size_t... Is>(std::index_sequence<Is...>) consteval {
            return ((is_variable_member<Is>() ? size_t{1} : size_t{0}) + ...);
         }(std::make_index_sequence<N>{});
      }

      static constexpr size_t NumVariable = count_variable_members();

      // Get array of variable member indices at compile time
      static consteval auto compute_variable_indices() {
         // Use size 1 minimum to avoid zero-size array
         std::array<size_t, NumVariable == 0 ? 1 : NumVariable> result{};
         if constexpr (NumVariable > 0) {
            size_t idx = 0;
            [&]<size_t... Is>(std::index_sequence<Is...>) {
               ((is_variable_member<Is>() ? void(result[idx++] = Is) : void()), ...);
            }(std::make_index_sequence<N>{});
         }
         return result;
      }

      static constexpr auto VariableIndices = compute_variable_indices();

      // ========================================================================
      // Compile-Time Inline Section Size Calculation
      // ========================================================================

      // Calculate the inline size contribution of a single member
      template <size_t I>
      static consteval size_t member_inline_size() {
         using MemberType = member_type_at<I>;

         if constexpr (zmem::is_simple_type_v<MemberType>) {
            // Simple types: direct sizeof
            return sizeof(MemberType);
         }
         else if constexpr (zmem::is_std_vector_v<MemberType> || zmem::is_std_string_v<MemberType>) {
            // Variable members: 16-byte reference (offset + count/length)
            return 16;
         }
         else if constexpr (std::is_aggregate_v<MemberType> && !zmem::is_std_array_v<MemberType>) {
            // Nested complex type: 8-byte size header + its inline section
            return 8 + inline_section_size_calculator<MemberType>::value;
         }
         else {
            // Fallback for other types
            return sizeof(MemberType);
         }
      }

      // Total inline section size (excluding the 8-byte size header of this struct)
      static consteval size_t compute_inline_section_size() {
         return []<size_t... Is>(std::index_sequence<Is...>) consteval {
            return (member_inline_size<Is>() + ... + size_t{0});
         }(std::make_index_sequence<N>{});
      }

      static constexpr size_t InlineSectionSize = compute_inline_section_size();

      // ========================================================================
      // Member Access Helper (works for both reflectable and glaze_object_t)
      // ========================================================================

      template <size_t I>
      GLZ_ALWAYS_INLINE static decltype(auto) access_member(auto&& value) {
         if constexpr (reflectable<T>) {
            return get_member(value, get<I>(to_tie(value)));
         }
         else {
            return get_member(value, get<I>(reflect<T>::values));
         }
      }

      // ========================================================================
      // Variable Member Data Writer (compile-time dispatched)
      // ========================================================================

      template <size_t MemberIndex, auto Opts, class B>
      GLZ_ALWAYS_INLINE static void write_variable_member_data(
         const T& value,
         size_t ref_pos,
         size_t inline_start,
         is_context auto&& ctx,
         B&& b,
         size_t& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();
         decltype(auto) member = access_member<MemberIndex>(value);
         using MemberType = member_type_at<MemberIndex>;

         const uint64_t offset = ix - inline_start;

         if constexpr (zmem::is_std_vector_v<MemberType>) {
            const uint64_t count = member.size();
            // Combined write: offset and count together (single memcpy)
            // Skip endian conversion on little-endian systems (most modern CPUs)
            if constexpr (std::endian::native == std::endian::little) {
               uint64_t ref_data[2] = {offset, count};
               std::memcpy(b.data() + ref_pos, ref_data, 16);
            }
            else {
               uint64_t ref_data[2] = {zmem::to_little_endian(offset), zmem::to_little_endian(count)};
               std::memcpy(b.data() + ref_pos, ref_data, 16);
            }

            if (count > 0) {
               using ElemType = typename MemberType::value_type;
               if constexpr (zmem::is_simple_type_v<ElemType>) {
                  zmem::write_bytes<unchecked>(member.data(), sizeof(ElemType) * count, b, ix);
               }
               else {
                  for (const auto& elem : member) {
                     to<ZMEM, ElemType>::template op<Opts>(elem, ctx, b, ix);
                  }
               }
            }
         }
         else if constexpr (zmem::is_std_string_v<MemberType>) {
            const uint64_t length = member.size();
            // Combined write: offset and length together (single memcpy)
            // Skip endian conversion on little-endian systems (most modern CPUs)
            if constexpr (std::endian::native == std::endian::little) {
               uint64_t ref_data[2] = {offset, length};
               std::memcpy(b.data() + ref_pos, ref_data, 16);
            }
            else {
               uint64_t ref_data[2] = {zmem::to_little_endian(offset), zmem::to_little_endian(length)};
               std::memcpy(b.data() + ref_pos, ref_data, 16);
            }

            if (length > 0) {
               zmem::write_bytes<unchecked>(member.data(), length, b, ix);
            }
         }
      }

      // ========================================================================
      // Main Serialization Entry Point
      // ========================================================================

      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         constexpr bool unchecked = is_write_unchecked<Opts>();

         // Complex struct format:
         // [size:8][inline section][variable section]

         // Pre-reserve buffer space using compile-time inline section size
         // This eliminates resize checks during inline section writes
         // (Skip if unchecked - buffer already pre-allocated)
         constexpr size_t header_plus_inline = 8 + InlineSectionSize;
         if constexpr (!unchecked && resizable<std::remove_cvref_t<B>>) {
            const size_t required = ix + header_plus_inline;
            if (required > b.size()) {
               b.resize((std::max)(b.size() * 2, required));
            }
         }

         // Write size header placeholder (no bounds check needed - pre-reserved)
         const size_t size_pos = ix;
         std::memcpy(b.data() + ix, "\0\0\0\0\0\0\0\0", 8);
         ix += 8;

         const size_t inline_start = ix;

         // Track positions of variable member references (stack allocated, fixed size)
         std::array<size_t, NumVariable == 0 ? 1 : NumVariable> ref_positions{};
         size_t var_idx = 0;

         // Phase 1: Write inline section (no resize checks - pre-reserved or unchecked)
         // Variable member placeholders are recorded for later patching
         for_each<N>([&]<size_t I>() {
            decltype(auto) member = access_member<I>(value);
            using MemberType = member_type_at<I>;

            if constexpr (zmem::is_simple_type_v<MemberType>) {
               // Simple member: write inline (direct memcpy, no bounds check)
               std::memcpy(b.data() + ix, &member, sizeof(MemberType));
               ix += sizeof(MemberType);
            }
            else if constexpr (zmem::is_std_vector_v<MemberType> || zmem::is_std_string_v<MemberType>) {
               // Variable member: record position and write placeholder (16 bytes)
               ref_positions[var_idx++] = ix;
               std::memset(b.data() + ix, 0, 16);
               ix += 16;
            }
            else {
               // Nested complex type: recursive serialization
               to<ZMEM, MemberType>::template op<Opts>(member, ctx, b, ix);
            }
         });

         // Phase 2: Write variable section and patch references
         // Compile-time unrolled - O(NumVariable) with direct dispatch, no O(N) search
         if constexpr (NumVariable > 0) {
            [&]<size_t... Idx>(std::index_sequence<Idx...>) {
               // Each Idx maps to VariableIndices[Idx] which is the actual member index
               // This is O(1) dispatch per variable member instead of O(N)
               ((write_variable_member_data<VariableIndices[Idx], Opts>(
                  value, ref_positions[Idx], inline_start, ctx, b, ix)), ...);
            }(std::make_index_sequence<NumVariable>{});
         }

         // Patch total size
         const uint64_t total_size = zmem::to_little_endian(static_cast<uint64_t>(ix - size_pos - 8));
         std::memcpy(b.data() + size_pos, &total_size, sizeof(uint64_t));
      }
   };

   // ============================================================================
   // Inline Section Size Calculator (for recursive nested complex types)
   // ============================================================================

   // Primary template handles complex aggregate types with reflection support
   // This is the only case where inline_section_size_calculator should be used
   // (nested complex types within a complex struct)
   template <class T>
   struct inline_section_size_calculator
   {
      static constexpr size_t value = []() consteval {
         if constexpr (!zmem::is_simple_type_v<T> && !zmem::is_std_array_v<T> &&
                       (glz::reflectable<T> || glz::glaze_object_t<T>)) {
            return to<ZMEM, T>::InlineSectionSize;
         }
         else {
            // Fallback for types that don't match (shouldn't normally be reached)
            return sizeof(T);
         }
      }();
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
         return {error_code::file_open_failure};
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
      return {ctx.error};
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

      return {ctx.error};
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
