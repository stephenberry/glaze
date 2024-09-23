// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

#include "glaze/binary/header.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/refl.hpp"
#include "glaze/core/seek.hpp"
#include "glaze/core/write.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   namespace detail
   {
      template <>
      struct write<BSON>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            to_bson<std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                               std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      // Helper function to write a value to the buffer
      GLZ_ALWAYS_INLINE void bson_dump_value(auto&& value, auto&& b, auto&& ix) noexcept
      {
         using V = std::decay_t<decltype(value)>;
         constexpr auto n = sizeof(V);
         if (ix + n > b.size()) [[unlikely]] {
            b.resize((std::max)(b.size() * 2, ix + n));
         }
         std::memcpy(&b[ix], &value, n);
         ix += n;
      }

      template <class T>
         requires num_t<T> || char_t<T>
      struct to_bson<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&&, Args&&... args) noexcept
         {
            constexpr uint8_t type = std::is_floating_point_v<T> ? 0x01  // Double
                                   : std::is_signed_v<T>        ? 0x12  // Int64
                                                                : 0x12; // Unsigned treated as Int64

            bson_dump_value(type, args...);
            // Field name (empty for array elements)
            bson_dump_value(uint8_t(0), args...);

            if constexpr (std::is_same_v<T, double>) {
               bson_dump_value(value, args...);
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
               int32_t val = static_cast<int32_t>(value);
               bson_dump_value(val, args...);
            }
            else {
               int64_t val = static_cast<int64_t>(value);
               bson_dump_value(val, args...);
            }
         }
      };

      template <str_t T>
      struct to_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            bson_dump_value(uint8_t(0x02), b, ix);  // Type: String
            bson_dump_value(uint8_t(0), b, ix);     // Field name (empty for array elements)

            const auto n = value.size();
            const auto str_size = n + 1;  // Include null terminator
            bson_dump_value(str_size, b, ix);

            if (ix + n + 1 > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n + 1));
            }
            std::memcpy(&b[ix], value.data(), n);
            ix += n;
            b[ix++] = '\0';  // Null terminator
         }
      };
      
      template<typename V>
      constexpr uint8_t get_bson_type_indicator() {
         if constexpr (std::is_same_v<V, double>) {
            return 0x01; // Double
         }
         else if constexpr (str_t<V>) {
            return 0x02; // String
         }
         else if constexpr (std::is_same_v<V, int32_t>) {
            return 0x10; // Int32
         }
         else if constexpr (std::is_same_v<V, int64_t> || std::is_integral_v<V>) {
            return 0x12; // Int64
         }
         else if constexpr (boolean_like<V>) {
            return 0x08; // Boolean
         }
         else if constexpr (glaze_object_t<V>) {
            return 0x03; // Embedded Document
         }
         else if constexpr (writable_array_t<V>) {
            return 0x04; // Array
         }
         else {
            static_assert(false_v<V>, "Unsupported type in get_bson_type_indicator");
         }
      }

      
      template <is_variant T>
      struct to_bson<T>
      {
         template <auto Opts>
         static void op(const T& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            // Write the BSON type indicator for an embedded document
            bson_dump_value(uint8_t(0x03), b, ix);  // Type: Embedded Document
            bson_dump_value(uint8_t(0), b, ix);     // Field name (empty for top-level document)
            
            // Placeholder for document size
            auto size_ix = ix;
            int32_t size_placeholder = 0;
            bson_dump_value(size_placeholder, b, ix);  // Placeholder for size
            
            const auto start_ix = ix;
            
            std::visit(
               [&](const auto& v) {
                  using V = std::decay_t<decltype(v)>;
                  using Variant = T;
                  constexpr uint64_t index = variant_index_v<V, Variant>;
                  
                  // Serialize "type_index" field
                  bson_dump_value(uint8_t(0x10), b, ix);  // Type: Int32 (0x10)
                  const char* type_index_field_name = "type_index";
                  size_t len = strlen(type_index_field_name);
                  if (ix + len + 1 > b.size()) {
                     b.resize((std::max)(b.size() * 2, ix + len + 1));
                  }
                  std::memcpy(&b[ix], type_index_field_name, len);
                  ix += len;
                  b[ix++] = '\0';
                  int32_t index32 = static_cast<int32_t>(index);
                  bson_dump_value(index32, b, ix);
                  
                  // Serialize "value" field
                  uint8_t value_type_indicator = get_bson_type_indicator<V>();
                  bson_dump_value(value_type_indicator, b, ix);

                  const char* value_field_name = "value";
                  len = strlen(value_field_name);
                  if (ix + len + 1 > b.size()) {
                     b.resize((std::max)(b.size() * 2, ix + len + 1));
                  }
                  std::memcpy(&b[ix], value_field_name, len);
                  ix += len;
                  b[ix++] = '\0';
                  
                  // Now write the value
                  to_bson<V>::template op<Opts>(v, ctx, b, ix);
               },
               value);
            
            bson_dump_value(uint8_t(0x00), b, ix);  // Null terminator for document
            
            // Update the document size
            int32_t size = int32_t(ix - start_ix);
            std::memcpy(&b[size_ix], &size, sizeof(int32_t));
         }
      };

      
      // write includers as empty strings
      template <is_includer T>
      struct to_bson<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&... args) noexcept
         {
            // Write the BSON type indicator for a string
            bson_dump_value(uint8_t(0x02), args...);  // Type: String
            
            // Write the field name (empty for array elements or handled externally)
            bson_dump_value(uint8_t(0), args...);     // Empty field name (null terminator)
            
            // Write the length of the string (int32). For an empty string, length is 1 (for null terminator)
            int32_t str_size = 1;  // Empty string length including null terminator
            bson_dump_value(str_size, args...);
            
            // Write the null terminator for the string value
            bson_dump_value(uint8_t(0), args...);  // Null terminator for the string value
         }
      };

      template <boolean_like T>
      struct to_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            bson_dump_value(uint8_t(0x08), b, ix);  // Type: Boolean
            bson_dump_value(uint8_t(0), b, ix);     // Field name (empty for array elements)
            bson_dump_value(uint8_t(value ? 0x01 : 0x00), b, ix);
         }
      };

      template <nullable_t T>
      struct to_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(const T& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            if (value) {
               write<BSON>::op<Opts>(*value, ctx, b, ix);
            }
            else {
               bson_dump_value(uint8_t(0x0A), b, ix);  // Type: Null
               bson_dump_value(uint8_t(0), b, ix);     // Field name (empty for array elements)
            }
         }
      };

      template <writable_array_t T>
      struct to_bson<T>
      {
         template <auto Opts>
         static void op(const T& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            bson_dump_value(uint8_t(0x04), b, ix);  // Type: Array
            bson_dump_value(uint8_t(0), b, ix);     // Field name (empty for array elements)

            auto size_ix = ix;
            int32_t size_placeholder = 0;
            bson_dump_value(size_placeholder, b, ix);  // Placeholder for size

            const auto start_ix = ix;

            int index = 0;
            for (const auto& elem : value) {
               // Field name is the array index as a string
               auto key = std::to_string(index++);
               bson_dump_value(uint8_t(0x00), b, ix);  // Placeholder for element type
               if (ix + key.size() + 1 > b.size()) {
                  b.resize(b.size() + key.size() + 1);
               }
               std::memcpy(&b[ix], key.data(), key.size());
               ix += key.size();
               b[ix++] = '\0';

               auto type_ix = ix - key.size() - 2;  // Type is before field name
               write<BSON>::op<Opts>(elem, ctx, b, ix);

               // Update the element type based on the written data
               b[type_ix] = b[type_ix + key.size() + 2];
            }

            bson_dump_value(uint8_t(0x00), b, ix);  // Null terminator for document

            int32_t size = int32_t(ix - start_ix);
            std::memcpy(&b[size_ix], &size, sizeof(int32_t));
         }
      };

      template <glaze_object_t T>
      struct to_bson<T>
      {
         template <auto Opts>
         static void op(const T& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            bson_dump_value(uint8_t(0x03), b, ix);  // Type: Embedded Document
            bson_dump_value(uint8_t(0), b, ix);     // Field name (empty for array elements)

            auto size_ix = ix;
            int32_t size_placeholder = 0;
            bson_dump_value(size_placeholder, b, ix);  // Placeholder for size

            const auto start_ix = ix;

            static constexpr auto N = refl<T>.N;

            invoke_table<N>([&]<size_t I>() {
               constexpr auto& key = refl<T>.keys[I];
               
               // Write element type and field name
               bson_dump_value(uint8_t(0x00), b, ix);  // Placeholder for element type
               if (ix + key.size() + 1 > b.size()) {
                  b.resize(b.size() + key.size() + 1);
               }
               std::memcpy(&b[ix], key.data(), key.size());
               ix += key.size();
               b[ix++] = '\0';

               auto type_ix = ix - key.size() - 2;  // Type is before field name
               write<BSON>::op<Opts>(get_member(value, get<I>(refl<T>.values)), ctx, b, ix);

               // Update the element type based on the written data
               b[type_ix] = b[type_ix + key.size() + 2];
            });

            bson_dump_value(uint8_t(0x00), b, ix);  // Null terminator for document

            int32_t size = int32_t(ix - start_ix);
            std::memcpy(&b[size_ix], &size, sizeof(int32_t));
         }
      };
   }
   
   template <write_bson_supported T, class Buffer>
   [[nodiscard]] error_ctx write_bson(T&& value, Buffer&& buffer) noexcept
   {
      return write<opts{.format = BSON}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <opts Opts = opts{}, write_bson_supported T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_bson(T&& value) noexcept
   {
      return write<set_bson<Opts>()>(std::forward<T>(value));
   }
}
