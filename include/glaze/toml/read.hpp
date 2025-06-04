// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <cctype>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/glaze_fast_float.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   template <>
   struct parse<TOML>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end)
      {
         from<TOML, std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                        std::forward<It0>(it), std::forward<It1>(end));
      }
   };

   // Skip whitespace and comments
   template <class It, class End>
   GLZ_ALWAYS_INLINE void skip_ws_and_comments(It&& it, End&& end) noexcept
   {
      while (it != end) {
         if (*it == ' ' || *it == '\t') {
            ++it;
         }
         else if (*it == '#') {
            // Skip comment to end of line
            while (it != end && *it != '\n' && *it != '\r') {
               ++it;
            }
         }
         else {
            break;
         }
      }
   }

   // Skip to next line
   template <class Ctx, class It, class End>
   GLZ_ALWAYS_INLINE bool skip_to_next_line(Ctx&, It&& it, End&& end) noexcept
   {
      while (it != end && *it != '\n' && *it != '\r') {
         ++it;
      }
      
      if (it == end) {
         return false;
      }

      if (*it == '\r') {
         ++it;
         if (it != end && *it == '\n') {
            ++it;
         }
      }
      else if (*it == '\n') {
         ++it;
      }
      
      return true;
   }

   // Parse TOML key (bare key or quoted key)
   template <class Ctx, class It, class End>
   GLZ_ALWAYS_INLINE bool parse_toml_key(std::string& key, Ctx& ctx, It&& it, End&& end) noexcept
   {
      key.clear();
      skip_ws_and_comments(it, end);
      
      if (it == end) {
         ctx.error = error_code::unexpected_end;
         return false;
      }

      if (*it == '"') {
         // Quoted key
         ++it;
         while (it != end && *it != '"') {
            if (*it == '\\') {
               ++it;
               if (it == end) {
                  ctx.error = error_code::unexpected_end;
                  return false;
               }
               switch (*it) {
                  case '"': key.push_back('"'); break;
                  case '\\': key.push_back('\\'); break;
                  case 'n': key.push_back('\n'); break;
                  case 't': key.push_back('\t'); break;
                  case 'r': key.push_back('\r'); break;
                  default: 
                     key.push_back('\\');
                     key.push_back(*it);
                     break;
               }
            }
            else {
               key.push_back(*it);
            }
            ++it;
         }
         
         if (it == end || *it != '"') {
            ctx.error = error_code::syntax_error;
            return false;
         }
         ++it; // Skip closing quote
      }
      else {
         // Bare key
         while (it != end && (std::isalnum(*it) || *it == '_' || *it == '-')) {
            key.push_back(*it);
            ++it;
         }
         
         if (key.empty()) {
            ctx.error = error_code::syntax_error;
            return false;
         }
      }
      
      return true;
   }

   template <glaze_value_t T>
   struct from<TOML, T>
   {
      template <auto Opts, is_context Ctx, class It0, class It1>
      static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end)
      {
         using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
         from<TOML, V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx),
                                         std::forward<It0>(it), std::forward<It1>(end));
      }
   };

   template <num_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         skip_ws_and_comments(it, end);
         
         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         using V = decay_keep_volatile_t<decltype(value)>;
         if constexpr (int_t<V>) {
            if constexpr (std::is_unsigned_v<V>) {
               uint64_t i{};
               if (*it == '-') [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }

               if (not glz::atoi(i, it, end)) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }

               if (i > (std::numeric_limits<V>::max)()) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               value = static_cast<V>(i);
            }
            else {
               uint64_t i{};
               int sign = 1;
               if (*it == '-') {
                  sign = -1;
                  ++it;
                  if (it == end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
               }
               if (not glz::atoi(i, it, end)) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }

               if (i > (std::numeric_limits<V>::max)()) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               value = sign * static_cast<V>(i);
            }
         }
         else {
            auto [ptr, ec] = glz::from_chars<false>(it, end, value); // Always treat as non-null-terminated
            if (ec != std::errc()) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            it = ptr;
         }
      }
   };

   template <string_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         value.clear();
         skip_ws_and_comments(it, end);

         if (it == end) {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (*it == '"') {
            // Basic string
            ++it; // Skip opening quote

            while (it != end && *it != '"') {
               if (*it == '\\') {
                  ++it;
                  if (it == end) {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  switch (*it) {
                     case '"': value.push_back('"'); break;
                     case '\\': value.push_back('\\'); break;
                     case 'n': value.push_back('\n'); break;
                     case 't': value.push_back('\t'); break;
                     case 'r': value.push_back('\r'); break;
                     case 'b': value.push_back('\b'); break;
                     case 'f': value.push_back('\f'); break;
                     default:
                        ctx.error = error_code::syntax_error;
                        return;
                  }
               }
               else {
                  value.push_back(*it);
               }
               ++it;
            }

            if (it == end || *it != '"') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it; // Skip closing quote
         }
         else if (*it == '\'') {
            // Literal string
            ++it; // Skip opening quote

            while (it != end && *it != '\'') {
               value.push_back(*it);
               ++it;
            }

            if (it == end || *it != '\'') {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it; // Skip closing quote
         }
         else {
            // Bare string (not typical in TOML values, but handle it)
            while (it != end && *it != '\n' && *it != '\r' && *it != '#') {
               value.push_back(*it);
               ++it;
            }
            
            // Trim trailing whitespace
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
               value.pop_back();
            }
         }
      }
   };

   template <bool_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         skip_ws_and_comments(it, end);
         
         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (it + 4 <= end && std::string_view(it, 4) == "true") {
            value = true;
            it += 4;
         }
         else if (it + 5 <= end && std::string_view(it, 5) == "false") {
            value = false;
            it += 5;
         }
         else {
            ctx.error = error_code::expected_true_or_false;
            return;
         }
      }
   };

   template <readable_array_t T>
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         skip_ws_and_comments(it, end);
         
         if (it == end || *it != '[') {
            ctx.error = error_code::syntax_error;
            return;
         }
         
         ++it; // Skip '['
         skip_ws_and_comments(it, end);
         
         // Handle empty array
         if (it != end && *it == ']') {
            ++it;
            return;
         }
         
         size_t index = 0;
         while (it != end) {
            using value_type = typename std::decay_t<T>::value_type;
            
            if constexpr (emplace_backable<T>) {
               auto& element = value.emplace_back();
               from<TOML, value_type>::template op<Opts>(element, ctx, it, end);
            }
            else {
               // For fixed-size arrays like std::array
               if (index >= value.size()) {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }
               from<TOML, value_type>::template op<Opts>(value[index], ctx, it, end);
               ++index;
            }
            
            if (bool(ctx.error)) {
               return;
            }
            
            skip_ws_and_comments(it, end);
            
            if (it == end) {
               ctx.error = error_code::unexpected_end;
               return;
            }
            
            if (*it == ']') {
               ++it;
               break;
            }
            else if (*it == ',') {
               ++it;
               skip_ws_and_comments(it, end);
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
      }
   };

   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && not custom_read<T>)
   struct from<TOML, T>
   {
      template <auto Opts, class It>
      static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
      {
         static constexpr auto N = reflect<T>::size;
         static constexpr auto HashInfo = hash_info<T>;

         while (it != end) {
            skip_ws_and_comments(it, end);
            
            if (it == end) {
               break;
            }
            
            // Skip empty lines
            if (*it == '\n' || *it == '\r') {
               skip_to_next_line(ctx, it, end);
               continue;
            }
            
            // Handle section headers [section]
            if (*it == '[') {
               // For now, skip section headers - we'll implement nested object support later
               skip_to_next_line(ctx, it, end);
               continue;
            }
            
            // Parse key = value
            std::string key;
            if (!parse_toml_key(key, ctx, it, end)) {
               return;
            }
            
            skip_ws_and_comments(it, end);
            
            if (it == end || *it != '=') {
               ctx.error = error_code::syntax_error;
               return;
            }
            
            ++it; // Skip '='
            skip_ws_and_comments(it, end);
            
            // Find the member with this key
            const auto index = decode_hash_with_size<TOML, T, HashInfo, HashInfo.type>::op(
               key.data(), key.data() + key.size(), key.size());

            if (index < N) [[likely]] {
               visit<N>(
                  [&]<size_t I>() {
                     if (I == index) {
                        decltype(auto) member = [&]() -> decltype(auto) {
                           if constexpr (reflectable<T>) {
                              return get_member(value, get<I>(to_tie(value)));
                           }
                           else {
                              return get_member(value, get<I>(reflect<T>::values));
                           }
                        }();

                        using member_type = std::decay_t<decltype(member)>;
                        from<TOML, member_type>::template op<Opts>(member, ctx, it, end);
                     }
                  },
                  index);

               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
            }
            else {
               // Unknown key - skip to end of line
               skip_to_next_line(ctx, it, end);
            }
            
            // Skip to next line
            skip_ws_and_comments(it, end);
            if (it != end && (*it == '\n' || *it == '\r')) {
               skip_to_next_line(ctx, it, end);
            }
         }
      }
   };

   template <read_supported<TOML> T, class Buffer>
   [[nodiscard]] inline auto read_toml(T&& value, Buffer&& buffer)
   {
      return read<opts{.format = TOML}>(value, std::forward<Buffer>(buffer));
   }

   template <read_supported<TOML> T, class Buffer>
   [[nodiscard]] inline auto read_toml(Buffer&& buffer)
   {
      T value{};
      read<opts{.format = TOML}>(value, std::forward<Buffer>(buffer));
      return value;
   }

   template <read_supported<TOML> T, is_buffer Buffer>
   [[nodiscard]] inline error_ctx read_file_toml(T& value, const sv file_name, Buffer&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {ec};
      }

      return read<opts{.format = TOML}>(value, buffer, ctx);
   }
}
