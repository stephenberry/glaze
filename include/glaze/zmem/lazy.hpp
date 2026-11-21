// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <span>
#include <string_view>

#include "glaze/zmem/header.hpp"
#include "glaze/zmem/layout.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/reflection/to_tuple.hpp"

namespace glz
{
   namespace zmem
   {
      template <class T>
      struct strided_span
      {
         const char* data{};
         size_t count{};
         size_t stride{};

         [[nodiscard]] size_t size() const noexcept { return count; }

         [[nodiscard]] const T& operator[](size_t index) const noexcept
         {
            return *reinterpret_cast<const T*>(data + index * stride);
         }
      };
   }

   // ============================================================================
   // lazy_zmem_view - Zero-copy view into ZMEM data
   // ============================================================================

   /**
    * @brief A lazy, zero-copy view into ZMEM serialized data.
    *
    * Unlike read_zmem which copies data into native C++ types (std::string, std::vector),
    * lazy_zmem_view provides direct access to the serialized buffer:
    * - Strings are accessed as std::string_view (no allocation)
    * - Fixed-type vectors are accessed as std::span<const T> (no allocation)
    * - Primitives are read directly from the buffer
    *
    * This enables zero-copy access patterns similar to FlatBuffers and Cap'n Proto.
    *
    * @tparam T The C++ type that describes the ZMEM structure
    *
    * Usage:
    * @code
    *   std::string buffer;
    *   glz::write_zmem(my_obj, buffer);
    *
    *   auto view = glz::lazy_zmem<MyType>(buffer);
    *   std::string_view name = view.get<0>(); // Zero-copy string access
    *   std::span<const int> ids = view.get<1>(); // Zero-copy vector access
    * @endcode
    */
   template <class T>
   struct lazy_zmem_view
   {
   private:
      const char* data_{};
      size_t size_{};

      // For variable structs, base points to the start of the inline section (after size header)
      // For fixed structs, base points to the struct data directly
      const char* base_{};

   public:
      lazy_zmem_view() = default;

      lazy_zmem_view(const char* data, size_t size) noexcept
         : data_(data), size_(size)
      {
         if constexpr (zmem::is_fixed_type_v<T>) {
            base_ = data_;
         }
         else {
            // Variable struct: skip size header and inline-base padding
            base_ = data_ + zmem::inline_layout<T>::InlineBaseOffset;
         }
      }

      template <class Buffer>
         requires requires(Buffer b) { b.data(); b.size(); }
      explicit lazy_zmem_view(const Buffer& buffer) noexcept
         : lazy_zmem_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())
      {}

      [[nodiscard]] bool valid() const noexcept { return data_ != nullptr && size_ > 0; }
      [[nodiscard]] const char* data() const noexcept { return data_; }
      [[nodiscard]] size_t size() const noexcept { return size_; }

      // ========================================================================
      // Fixed Struct Access
      // ========================================================================

      /**
       * @brief Access the underlying fixed struct directly (only for fixed types)
       * @return Const reference to the struct in the buffer
       */
      [[nodiscard]] const T& as_fixed() const noexcept
         requires zmem::is_fixed_type_v<T>
      {
         return *reinterpret_cast<const T*>(data_);
      }

      // ========================================================================
      // Field Access by Index
      // ========================================================================

      /**
       * @brief Get a field by compile-time index.
       *
       * For fixed types: returns the value directly
       * For strings: returns std::string_view (zero-copy)
       * For vectors of fixed types: returns std::span<const ElemType> (zero-copy)
       *
       * @tparam I Field index (0-based)
       * @return The field value or view
       */
      template <size_t I>
      [[nodiscard]] auto get() const noexcept
      {
         static_assert(I < reflect<T>::size, "Field index out of bounds");
         using MemberType = field_t<T, I>;

         if constexpr (zmem::is_fixed_type_v<T>) {
            // Fixed struct: direct access via reinterpret_cast
            const T* ptr = reinterpret_cast<const T*>(data_);
            if constexpr (reflectable<T>) {
               return get_member(*ptr, glz::get<I>(to_tie(*ptr)));
            }
            else {
               return get_member(*ptr, glz::get<I>(reflect<T>::values));
            }
         }
         else {
            // Variable struct: calculate offset at runtime and access
            const char* field_ptr = compute_field_ptr<I>();
            return read_field<MemberType>(field_ptr);
         }
      }

   private:
      static constexpr auto N = reflect<T>::size;

      // Compute field pointer at runtime, handling nested variable structs
      template <size_t TargetIndex>
      [[nodiscard]] const char* compute_field_ptr() const noexcept
      {
         return base_ + zmem::inline_layout<T>::Offsets[TargetIndex];
      }

      template <class MemberType>
      [[nodiscard]] auto read_field(const char* field_ptr) const noexcept
      {
         if constexpr (zmem::is_fixed_type_v<MemberType>) {
            // Fixed field: read value directly
            MemberType value;
            std::memcpy(&value, field_ptr, sizeof(MemberType));
            return value;
         }
         else if constexpr (zmem::is_std_string_v<MemberType>) {
            // String field: return string_view into buffer
            zmem::string_ref ref;
            std::memcpy(&ref, field_ptr, sizeof(zmem::string_ref));
            return std::string_view{base_ + ref.offset, static_cast<size_t>(ref.length)};
         }
         else if constexpr (zmem::is_std_vector_v<MemberType>) {
            using ElemType = typename MemberType::value_type;

            zmem::vector_ref ref;
            std::memcpy(&ref, field_ptr, sizeof(zmem::vector_ref));

            if constexpr (zmem::is_fixed_type_v<ElemType>) {
               constexpr size_t stride = zmem::vector_fixed_stride<ElemType>();
               if constexpr (stride == sizeof(ElemType)) {
                  const ElemType* elem_ptr = reinterpret_cast<const ElemType*>(base_ + ref.offset);
                  return std::span<const ElemType>{elem_ptr, static_cast<size_t>(ref.count)};
               }
               else {
                  return zmem::strided_span<ElemType>{base_ + ref.offset, static_cast<size_t>(ref.count), stride};
               }
            }
            else {
               // Variable element vector: return raw data pointer and count
               // A more complete implementation would return a lazy iterator
               return std::pair{base_ + ref.offset, ref.count};
            }
         }
         else if constexpr (zmem::is_std_map_like_v<MemberType>) {
            zmem::map_ref ref;
            std::memcpy(&ref, field_ptr, sizeof(zmem::map_ref));
            return std::pair{base_ + ref.offset, ref.count};
         }
         else {
            // Nested variable struct: return a lazy view into it
            uint64_t offset;
            std::memcpy(&offset, field_ptr, sizeof(uint64_t));
            const char* nested_ptr = base_ + offset;
            uint64_t nested_size;
            std::memcpy(&nested_size, nested_ptr, sizeof(uint64_t));
            return lazy_zmem_view<MemberType>{nested_ptr, static_cast<size_t>(sizeof(uint64_t) + nested_size)};
         }
      }
   };

   // ============================================================================
   // Public API Functions
   // ============================================================================

   /**
    * @brief Create a lazy zero-copy view into ZMEM data.
    *
    * @tparam T The C++ type describing the structure
    * @param buffer The serialized ZMEM data (must remain valid for view lifetime)
    * @return A lazy view that provides zero-copy field access
    *
    * @code
    *   auto view = glz::lazy_zmem<MyStruct>(buffer);
    *   auto name = view.get<0>(); // Returns std::string_view for string fields
    * @endcode
    */
   template <class T, class Buffer>
      requires requires(Buffer b) { b.data(); b.size(); }
   [[nodiscard]] GLZ_ALWAYS_INLINE lazy_zmem_view<T> lazy_zmem(const Buffer& buffer) noexcept
   {
      return lazy_zmem_view<T>{buffer};
   }

   /**
    * @brief Create a lazy view from raw pointer and size.
    */
   template <class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE lazy_zmem_view<T> lazy_zmem(const char* data, size_t size) noexcept
   {
      return lazy_zmem_view<T>{data, size};
   }

   template <class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE lazy_zmem_view<T> lazy_zmem(const void* data, size_t size) noexcept
   {
      return lazy_zmem_view<T>{static_cast<const char*>(data), size};
   }

} // namespace glz
