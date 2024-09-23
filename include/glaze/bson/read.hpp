// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/binary/skip.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/refl.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/dump.hpp"

namespace glz
{
   namespace detail
   {
      template <>
      struct read<BSON>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
               if constexpr (Opts.error_on_const_read) {
                  ctx.error = error_code::attempt_const_read;
               }
               else {
                  // do not read anything into the const value
                  skip_value_bson<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
               }
            }
            else {
               using V = std::remove_cvref_t<T>;
               from_bson<V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                 std::forward<It0>(it), std::forward<It1>(end));
            }
         }
      };

      template <class T>
         requires(glaze_value_t<T> && !custom_read<T>)
      struct from_bson<T>
      {
         template <auto Opts, class Value, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
            from_bson<V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                              std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };

      // Helper function to read a value from the buffer
      template <typename V>
      GLZ_ALWAYS_INLINE void bson_read_value(V& value, auto&& it, auto&& end, is_context auto&& ctx) noexcept
      {
         constexpr auto n = sizeof(V);
         if ((it + n) > end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         std::memcpy(&value, &(*it), n);
         it += n;
      }

      template <class T>
         requires num_t<T> || char_t<T>
      struct from_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();
            uint8_t type = *it++;
            if ((it == end) || (type != 0x01 && type != 0x10 && type != 0x12)) {
               ctx.error = error_code::syntax_error;
               return;
            }

            // Skip field name
            while (it != end && *it++ != '\0') {}

            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if constexpr (std::is_same_v<T, double>) {
               if (type != 0x01) {
                  ctx.error = error_code::type_mismatch;
                  return;
               }
               double temp;
               bson_read_value(temp, it, end, ctx);
               value = static_cast<T>(temp);
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
               if (type != 0x10) {
                  ctx.error = error_code::type_mismatch;
                  return;
               }
               int32_t temp;
               bson_read_value(temp, it, end, ctx);
               value = static_cast<T>(temp);
            }
            else {
               if (type != 0x12) {
                  ctx.error = error_code::type_mismatch;
                  return;
               }
               int64_t temp;
               bson_read_value(temp, it, end, ctx);
               value = static_cast<T>(temp);
            }
         }
      };
      
      template <is_variant T>
      struct from_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();
            uint8_t type = *it++;
            if (type != 0x03) {  // Expecting an Embedded Document
               ctx.error = error_code::syntax_error;
               return;
            }

            // Skip the field name (null-terminated string)
            while (it != end && *it != '\0') {
               ++it;
            }
            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }
            ++it;  // Skip null terminator

            // Read the document size
            int32_t doc_size;
            bson_read_value(doc_size, it, end, ctx);
            if (bool(ctx.error)) return;

            auto doc_end = it + doc_size - 5;  // Exclude size and null terminator

            int32_t type_index = -1;
            bool value_found = false;

            while (it < doc_end) {
               uint8_t elem_type = *it++;
               std::string key;
               while (it != end && *it != '\0') {
                  key += *it++;
               }
               if (it == end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               ++it;  // Skip null terminator

               if (key == "type_index") {
                  if (elem_type != 0x10) {  // Expecting Int32
                     ctx.error = error_code::type_mismatch;
                     return;
                  }
                  bson_read_value(type_index, it, end, ctx);
                  if (bool(ctx.error)) return;
               }
               else if (key == "value") {
                  if (type_index == -1) {
                     ctx.error = error_code::syntax_error;  // 'type_index' must be read before 'value'
                     return;
                  }

                  // Rewind iterator to include the element type for the 'value' field
                  it -= key.size() + 1;  // Back to field name
                  it -= 1;               // Back to element type

                  // Set the variant to the correct type based on 'type_index'
                  using Variant = T;
                  const auto& variant_map = runtime_variant_map<Variant>();
                  if (type_index < 0 || static_cast<size_t>(type_index) >= variant_map.size()) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  value = variant_map[type_index];

                  // Deserialize the value into the variant
                  std::visit([&](auto& v) {
                     from_bson<std::decay_t<decltype(v)>>::template op<Opts>(v, ctx, it, end);
                  }, value);
                  if (bool(ctx.error)) return;

                  value_found = true;
               }
               else {
                  // Skip unknown field
                  skip_bson_element(elem_type, it, end, ctx);
                  if (ctx.error) return;
               }
            }

            if (it != doc_end + 1 || *it != 0x00) {  // Check for document null terminator
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;  // Skip null terminator

            if (!value_found) {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

      private:
         static void skip_bson_element(uint8_t elem_type, auto& it, auto& end, is_context auto& ctx) noexcept
         {
            switch (elem_type) {
               case 0x01: it += 8; break;  // Double
               case 0x02: {
                  int32_t str_size;
                  bson_read_value(str_size, it, end, ctx);
                  if (ctx.error) return;
                  it += str_size;
                  break;
               }
               case 0x08: it += 1; break;  // Boolean
               case 0x10: it += 4; break;  // Int32
               case 0x12: it += 8; break;  // Int64
               case 0x03:  // Embedded document
               case 0x04: {  // Array
                  int32_t doc_size;
                  bson_read_value(doc_size, it, end, ctx);
                  if (ctx.error) return;
                  it += doc_size - 4;
                  break;
               }
               default:
                  ctx.error = error_code::syntax_error;
                  return;
            }
         }
      };


      template <str_t T>
      struct from_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();
            uint8_t type = *it++;
            if (type != 0x02) {
               ctx.error = error_code::syntax_error;
               return;
            }

            // Skip field name
            while (it != end && *it++ != '\0') {}

            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            int32_t str_size;
            bson_read_value(str_size, it, end, ctx);
            if (bool(ctx.error)) return;

            if (str_size <= 0 || (it + str_size) > end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            value.assign(it, it + str_size - 1);  // Exclude null terminator
            it += str_size;
         }
      };

      template <boolean_like T>
      struct from_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();
            uint8_t type = *it++;
            if (type != 0x08) {
               ctx.error = error_code::syntax_error;
               return;
            }

            // Skip field name
            while (it != end && *it++ != '\0') {}

            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            uint8_t bool_value;
            bson_read_value(bool_value, it, end, ctx);
            if (bool(ctx.error)) return;

            value = (bool_value != 0);
         }
      };

      template <nullable_t T>
      struct from_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();
            uint8_t type = *it++;

            // Skip field name
            while (it != end && *it++ != '\0') {}

            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if (type == 0x0A) {  // Null
               value.reset();
            }
            else {
               if (!value) {
                  if constexpr (is_specialization_v<T, std::optional>)
                     value = std::make_optional<typename T::value_type>();
                  else if constexpr (is_specialization_v<T, std::unique_ptr>)
                     value = std::make_unique<typename T::element_type>();
                  else if constexpr (is_specialization_v<T, std::shared_ptr>)
                     value = std::make_shared<typename T::element_type>();
                  else if constexpr (constructible<T>) {
                     value = meta_construct_v<T>();
                  }
                  else {
                     ctx.error = error_code::invalid_nullable_read;
                     return;
                     // Cannot read into unset nullable that is not std::optional, std::unique_ptr, or std::shared_ptr
                  }
               }
               
               read<BSON>::op<Opts>(*value, ctx, --it, end); // Rewind to include type
            }
         }
      };
      
      template <is_includer T>
      struct from_bson<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();

            // Read the BSON type indicator
            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }
            uint8_t type = *it++;
            if (type != 0x02) {  // Expecting a string type
               ctx.error = error_code::syntax_error;
               return;
            }

            // Skip the field name (null-terminated string)
            while (it != end && *it != '\0') {
               ++it;
            }
            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }
            ++it;  // Skip the null terminator

            // Read the string length (int32)
            int32_t str_size;
            bson_read_value(str_size, it, end, ctx);
            if (bool(ctx.error)) return;

            // Validate the string length (should be 1 for empty string including null terminator)
            if (str_size != 1) {
               ctx.error = error_code::syntax_error;
               return;
            }

            // Ensure there is enough data for the string (null terminator)
            if ((it + str_size) > end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            // Skip the string data (null terminator of empty string)
            it += str_size;
         }
      };


      template <readable_array_t T>
      struct from_bson<T>
      {
         template <auto Opts>
         static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();
            uint8_t type = *it++;
            if (type != 0x04) {  // Array
               ctx.error = error_code::syntax_error;
               return;
            }

            // Skip field name
            while (it != end && *it++ != '\0') {}

            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            int32_t size;
            bson_read_value(size, it, end, ctx);
            if (bool(ctx.error)) return;

            auto array_end = it + size - 5;  // Exclude size and null terminator

            using V = range_value_t<T>;
            size_t index{};
            while (it < array_end) {
               [[maybe_unused]] uint8_t elem_type = *it++;
               // Skip field name (array index)
               while (it != end && *it++ != '\0') {}

               if (it == end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               V elem;
               from_bson<V>::template op<Opts>(elem, ctx, --it, end);  // Rewind to include type
               if (bool(ctx.error)) return;

               if constexpr (resizable<T>) {
                  value.emplace_back(std::move(elem));
               }
               else {
                  if (index >= value.size()) {
                     ctx.error = error_code::exceeded_static_array_size;
                     return;
                  }
                  value[index] = std::move(elem);
                  ++index;
               }
            }

            if (it != array_end + 1 || *it != 0x00) {  // Check for document null terminator
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;  // Skip null terminator
         }
      };
      
      template <class T>
         requires glaze_array_t<T>
      struct from_bson<T> final
      {
         template <auto Opts>
         static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
             GLZ_END_CHECK();
             uint8_t type = *it++;
             if (type != 0x04) {  // Array type in BSON
                 ctx.error = error_code::syntax_error;
                 return;
             }

             // Skip field name (null-terminated string)
             while (it != end && *it != '\0') {
                 ++it;
             }
             if (it == end) {
                 ctx.error = error_code::unexpected_end;
                 return;
             }
             ++it; // Skip the null terminator

             // Read the document size (int32)
             int32_t doc_size;
             bson_read_value(doc_size, it, end, ctx);
             if (bool(ctx.error)) return;

             auto doc_end = it + doc_size - 5;  // Exclude size and null terminator

             constexpr auto N = refl<T>.N;
            
            for_each_short_circuit<N>([&](auto I) {
               if (it >= doc_end) {
                   ctx.error = error_code::unexpected_end;
                   return true;
               }

               // Read the element
               using V = decltype(get_member(value, get<I>(refl<T>.values)));
               auto& member = get_member(value, get<I>(refl<T>.values));

               // Read the BSON element (type, field name, and value)
               read_bson_element<V, Opts>(member, I, ctx, it, end);
               if (bool(ctx.error)) return true;
               return false;
            });

             if (it != doc_end + 1 || *it != 0x00) {  // Check for document null terminator
                 ctx.error = error_code::syntax_error;
                 return;
             }
             ++it;  // Skip null terminator
         }

      private:
         template <class V, auto Opts>
         static void read_bson_element(V& value, size_t expected_index, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
             if (it >= end) {
                 ctx.error = error_code::unexpected_end;
                 return;
             }

             [[maybe_unused]] uint8_t elem_type = *it++;

             // Read field name
             std::string key;
             while (it != end && *it != '\0') {
                 key += *it++;
             }
             if (it == end) {
                 ctx.error = error_code::unexpected_end;
                 return;
             }
             ++it; // Skip null terminator

             // Validate the field name
             if (key != std::to_string(expected_index)) {
                 ctx.error = error_code::syntax_error;
                 return;
             }

             // Rewind the iterator to include the type and field name
             auto elem_start = it - key.size() - 1 - 1;

             // Deserialize the value
             from_bson<V>::template op<Opts>(value, ctx, elem_start, end);
             if (bool(ctx.error)) return;

             // Update the iterator to where from_bson left it
             it = elem_start;
         }
      };


      template <glaze_object_t T>
      struct from_bson<T>
      {
         template <auto Opts>
         static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();
            uint8_t type = *it++;
            if (type != 0x03) {  // Embedded Document
               ctx.error = error_code::syntax_error;
               return;
            }

            // Skip field name
            while (it != end && *it++ != '\0') {}

            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            int32_t size;
            bson_read_value(size, it, end, ctx);
            if (bool(ctx.error)) return;

            auto doc_end = it + size - 5;  // Exclude size and null terminator

            static constexpr auto N = refl<T>.N;

            while (it < doc_end) {
               uint8_t elem_type = *it++;
               std::string key;
               while (it != end && *it != '\0') {
                  key += *it++;
               }

               if (it == end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               ++it;  // Skip null terminator

               bool found = false;
               invoke_table<N>([&]<size_t I>() {
                  constexpr auto& member_key = refl<T>.keys[I];
                  if (key == member_key) {
                     using member_type = std::decay_t<decltype(get_member(value, get<I>(refl<T>.values)))>;
                     from_bson<member_type>::template op<Opts>(
                         get_member(value, get<I>(refl<T>.values)), ctx, --it, end);  // Rewind to include type
                     if (bool(ctx.error)) return;
                     found = true;
                  }
               });

               if (!found) {
                  // Skip unknown field
                  switch (elem_type) {
                     case 0x01: it += 8; break;  // Double
                     case 0x02: {
                        int32_t str_size;
                        bson_read_value(str_size, it, end, ctx);
                        if (bool(ctx.error)) return;
                        it += str_size;
                        break;
                     }
                     case 0x08: it += 1; break;  // Boolean
                     case 0x10: it += 4; break;  // Int32
                     case 0x12: it += 8; break;  // Int64
                     case 0x03:  // Embedded document
                     case 0x04: {  // Array
                        int32_t doc_size;
                        bson_read_value(doc_size, it, end, ctx);
                        if (bool(ctx.error)) return;
                        it += doc_size - 4;
                        break;
                     }
                     default:
                        ctx.error = error_code::syntax_error;
                        return;
                  }
               }
            }

            if (it != doc_end + 1 || *it != 0x00) {  // Check for document null terminator
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;  // Skip null terminator
         }
      };
   }
   
   template <read_bson_supported T, class Buffer>
   [[nodiscard]] inline error_ctx read_bson(T&& value, Buffer&& buffer) noexcept
   {
      return read<opts{.format = BSON}>(value, std::forward<Buffer>(buffer));
   }

   template <read_bson_supported T, class Buffer>
   [[nodiscard]] inline expected<T, error_ctx> read_bson(Buffer&& buffer) noexcept
   {
      T value{};
      const auto pe = read<opts{.format = BSON}>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
      }
      return value;
   }
}
