// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "glaze/core/opts.hpp"
#include "glaze/core/context.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/inline.hpp"

namespace glz
{
   // ZMEM format constant
   // Using value 3 (next available after BEVE=1, CBOR=2)
   inline constexpr uint32_t ZMEM = 3;

   // ZMEM requires little-endian byte order for zero-copy memory access
   static_assert(std::endian::native == std::endian::little,
                 "ZMEM format requires a little-endian architecture");

   namespace zmem
   {
      // ============================================================================
      // Endianness Utilities
      // ============================================================================

      // ZMEM uses little-endian byte order for all multi-byte values
      template <class T>
      GLZ_ALWAYS_INLINE constexpr void byteswap_le(T& value) noexcept
      {
         if constexpr (sizeof(T) > 1) {
            if constexpr (std::endian::native == std::endian::big) {
               value = std::byteswap(value);
            }
         }
      }

      template <class T>
      GLZ_ALWAYS_INLINE constexpr T to_little_endian(T value) noexcept
      {
         byteswap_le(value);
         return value;
      }

      template <class T>
      GLZ_ALWAYS_INLINE constexpr T from_little_endian(T value) noexcept
      {
         byteswap_le(value);
         return value;
      }

      // ============================================================================
      // Wire Format Types
      // ============================================================================

      // Vector reference stored in inline section for variable structs
      struct alignas(8) vector_ref
      {
         uint64_t offset; // Byte offset to array data (relative to byte 8)
         uint64_t count;  // Number of elements
      };
      static_assert(sizeof(vector_ref) == 16);
      static_assert(alignof(vector_ref) == 8);

      // String reference for variable-length strings
      struct alignas(8) string_ref
      {
         uint64_t offset; // Byte offset to string data (relative to byte 8)
         uint64_t length; // Byte length (NOT null-terminated)
      };
      static_assert(sizeof(string_ref) == 16);
      static_assert(alignof(string_ref) == 8);

      // ============================================================================
      // Optional Type with Guaranteed Layout
      // ============================================================================

      // ZMEM optional with explicit layout for wire compatibility
      // Layout: [present:1][padding:alignment-1][value:sizeof(T)]
      // Use 8-byte alignment for 64-bit types to ensure consistent layout across 32/64-bit platforms
      template <class T>
      struct alignas(sizeof(T) >= 8 ? 8 : alignof(T)) optional
      {
         uint8_t present{0};
         // Padding bytes (implicit in struct layout due to alignment)
         T value{};

         constexpr optional() noexcept = default;
         constexpr optional(std::nullopt_t) noexcept : present(0), value{} {}
         constexpr optional(const T& v) noexcept : present(1), value(v) {}
         constexpr optional(T&& v) noexcept : present(1), value(std::move(v)) {}

         constexpr bool has_value() const noexcept { return present != 0; }
         constexpr explicit operator bool() const noexcept { return has_value(); }

         constexpr T& operator*() noexcept { return value; }
         constexpr const T& operator*() const noexcept { return value; }
         constexpr T* operator->() noexcept { return &value; }
         constexpr const T* operator->() const noexcept { return &value; }

         constexpr T value_or(const T& default_value) const noexcept
         {
            return has_value() ? value : default_value;
         }

         constexpr void reset() noexcept
         {
            present = 0;
            value = T{};
         }

         constexpr optional& operator=(std::nullopt_t) noexcept
         {
            reset();
            return *this;
         }

         constexpr optional& operator=(const T& v) noexcept
         {
            present = 1;
            value = v;
            return *this;
         }
      };

      // Verify optional layout matches ZMEM spec
      static_assert(sizeof(optional<uint8_t>) == 2);
      static_assert(sizeof(optional<uint16_t>) == 4);
      static_assert(sizeof(optional<uint32_t>) == 8);
      static_assert(sizeof(optional<uint64_t>) == 16);
      static_assert(alignof(optional<uint32_t>) == 4);
      static_assert(alignof(optional<uint64_t>) == 8);

      // ============================================================================
      // Type Traits for ZMEM Categories
      // ============================================================================

      // Check if a type is a ZMEM optional
      template <class T>
      struct is_zmem_optional : std::false_type {};

      template <class T>
      struct is_zmem_optional<optional<T>> : std::true_type {};

      template <class T>
      inline constexpr bool is_zmem_optional_v = is_zmem_optional<T>::value;

      // Check if a type is a std::vector (which makes struct variable)
      template <class T>
      struct is_std_vector : std::false_type {};

      template <class T, class Alloc>
      struct is_std_vector<std::vector<T, Alloc>> : std::true_type {};

      template <class T>
      inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

      // Check if a type is a std::string (variable-length, makes struct variable)
      template <class T>
      struct is_std_string : std::false_type {};

      template <class CharT, class Traits, class Alloc>
      struct is_std_string<std::basic_string<CharT, Traits, Alloc>> : std::true_type {};

      template <class T>
      inline constexpr bool is_std_string_v = is_std_string<T>::value;

      // Check if a type is a fixed-size array (char[N] for strings)
      template <class T>
      struct is_fixed_string : std::false_type {};

      template <std::size_t N>
      struct is_fixed_string<char[N]> : std::true_type {};

      template <std::size_t N>
      struct is_fixed_string<const char[N]> : std::true_type {};

      template <class T>
      inline constexpr bool is_fixed_string_v = is_fixed_string<T>::value;

      // Check if a type is a C-style array
      template <class T>
      struct is_c_array : std::false_type {};

      template <class T, std::size_t N>
      struct is_c_array<T[N]> : std::true_type {};

      template <class T>
      inline constexpr bool is_c_array_v = is_c_array<T>::value;

      // Check if a type is a std::array
      template <class T>
      struct is_std_array : std::false_type {};

      template <class T, std::size_t N>
      struct is_std_array<std::array<T, N>> : std::true_type {};

      template <class T>
      inline constexpr bool is_std_array_v = is_std_array<T>::value;

      // ============================================================================
      // Fixed vs Variable Type Detection
      // ============================================================================

      // A type is "fixed" in ZMEM if it's trivially copyable
      // (no vectors, no std::string, no variable nested types)

      // Forward declaration for recursive check
      template <class T>
      struct is_fixed_type;

      // Primitives are fixed
      template <class T>
      concept zmem_primitive = std::is_arithmetic_v<T> || std::is_enum_v<T>;

      // Check if type is fixed (can be memcpy'd directly)
      template <class T>
      struct is_fixed_type
      {
         static constexpr bool value =
            std::is_trivially_copyable_v<T> &&
            !is_std_vector_v<T> &&
            !is_std_string_v<T>;
      };

      template <class T>
      inline constexpr bool is_fixed_type_v = is_fixed_type<T>::value;

      // Variable types contain vectors or variable-length strings
      template <class T>
      inline constexpr bool is_variable_type_v = !is_fixed_type_v<T>;

      // ============================================================================
      // Stack Allocation Thresholds
      // ============================================================================

      // Maximum number of elements for stack-allocated offset tables
      // 64 elements * 8 bytes = 512 bytes on stack (reasonable limit)
      inline constexpr size_t offset_table_stack_threshold = 64;

      // ============================================================================
      // Alignment and Size Utilities
      // ============================================================================

      // Calculate padding needed to align to boundary
      GLZ_ALWAYS_INLINE constexpr size_t padding_for_alignment(size_t offset, size_t alignment) noexcept
      {
         const size_t remainder = offset % alignment;
         return remainder == 0 ? 0 : alignment - remainder;
      }

      // Align offset up to boundary
      GLZ_ALWAYS_INLINE constexpr size_t align_up(size_t offset, size_t alignment) noexcept
      {
         return offset + padding_for_alignment(offset, alignment);
      }

      // Round size up to multiple of 8 (for fixed struct wire sizes)
      GLZ_ALWAYS_INLINE constexpr size_t padded_size_8(size_t size) noexcept
      {
         return (size + 7) & ~size_t(7);
      }

      // ============================================================================
      // Buffer Operations
      // ============================================================================

      // Write a value to buffer at given index (with endian conversion)
      // Template parameter `Unchecked`: if true, skip resize checks (buffer pre-allocated)
      template <bool Unchecked = false, class T, class B>
      GLZ_ALWAYS_INLINE void write_value(T value, B& buffer, size_t& ix) noexcept
      {
         byteswap_le(value);

         if constexpr (!Unchecked && resizable<B>) {
            if (ix + sizeof(T) > buffer.size()) {
               buffer.resize((std::max)(buffer.size() * 2, ix + sizeof(T)));
            }
         }

         std::memcpy(buffer.data() + ix, &value, sizeof(T));
         ix += sizeof(T);
      }

      // Write raw bytes to buffer
      // Template parameter `Unchecked`: if true, skip resize checks (buffer pre-allocated)
      template <bool Unchecked = false, class B>
      GLZ_ALWAYS_INLINE void write_bytes(const void* data, size_t size, B& buffer, size_t& ix) noexcept
      {
         if constexpr (!Unchecked && resizable<B>) {
            if (ix + size > buffer.size()) {
               buffer.resize((std::max)(buffer.size() * 2, ix + size));
            }
         }

         std::memcpy(buffer.data() + ix, data, size);
         ix += size;
      }

      // Write padding zeros
      // Template parameter `Unchecked`: if true, skip resize checks (buffer pre-allocated)
      template <bool Unchecked = false, class B>
      GLZ_ALWAYS_INLINE void write_padding(size_t count, B& buffer, size_t& ix) noexcept
      {
         if (count == 0) return;

         if constexpr (!Unchecked && resizable<B>) {
            if (ix + count > buffer.size()) {
               buffer.resize((std::max)(buffer.size() * 2, ix + count));
            }
         }

         std::memset(buffer.data() + ix, 0, count);
         ix += count;
      }

      // Read a value from buffer (with endian conversion)
      template <class T>
      GLZ_ALWAYS_INLINE T read_value(const void* data) noexcept
      {
         T value;
         std::memcpy(&value, data, sizeof(T));
         byteswap_le(value);
         return value;
      }

      template <class T>
      GLZ_ALWAYS_INLINE T read_value(const void* data, size_t& ix) noexcept
      {
         T value = read_value<T>(static_cast<const uint8_t*>(data) + ix);
         ix += sizeof(T);
         return value;
      }

   } // namespace zmem
} // namespace glz
