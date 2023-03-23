// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
#include "glaze/core/format.hpp"
#include "glaze/util/strod.hpp"
#include "glaze/file/file_ops.hpp"

#include <charconv>

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct from_csv {};

      template <>
      struct read<csv>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         static void op(T&& value, Ctx&& ctx, It0&& it, It1 end) noexcept
         {
            from_csv<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };

      template <glaze_value_t T>
      struct from_csv<T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            from_csv<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };

      template <num_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&&) noexcept
         {
            if (static_cast<bool>(ctx.error)) [[unlikely]] {
               return;
            }
            
            // TODO: fix this also, taken from json
            using X = std::conditional_t<std::is_const_v<std::remove_pointer_t<std::remove_reference_t<decltype(it)>>>,
                                         const uint8_t*, uint8_t*>;
            auto cur = reinterpret_cast<X>(it);
            auto s = parse_number(value, cur);
            if (!s) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            it = reinterpret_cast<std::remove_reference_t<decltype(it)>>(cur);
         }
      };

      template <bool_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&&) noexcept
         {
            if (static_cast<bool>(ctx.error)) [[unlikely]] {
               return;
            }
            
            // TODO: fix this also, taken from json
            using X = std::conditional_t<std::is_const_v<std::remove_pointer_t<std::remove_reference_t<decltype(it)>>>,
                                         const uint8_t*, uint8_t*>;
            auto cur = reinterpret_cast<X>(it);
            int temp;
            auto s = parse_number(temp, cur);
            if (!s) [[unlikely]] {
               ctx.error = error_code::expected_true_or_false;
               return;
            }
            value = temp;
            it = reinterpret_cast<std::remove_reference_t<decltype(it)>>(cur);
         }
      };

      template <array_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
         {
            if constexpr (resizeable<T>)
            {
               typename T::value_type element;
               read<csv>::op<Opts>(element, ctx, it, end);
               value.emplace_back(element);
            }
            else
            {
               if constexpr (Opts.row_wise) {
                  typename T::value_type element;
                  read<csv>::op<Opts>(element, ctx, it, end);
                  value[ctx.csv_index] = element;
               }
               else {
                  const auto size = value.size();
                  for (size_t i = 0; i < size; ++i) {
                     read<csv>::op<Opts>(value[i], ctx, it, end);
                     if (i != size - 1) {
                        ++it; // skip comment
                     }
                  }
               }
            }
         }
      };
      
      template <char delim>
      inline void goto_delim(auto&& it, auto&& end) noexcept
      {
         while (++it != end && *it != delim)
            ;
      }

      template <glaze_object_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
         {
            if constexpr (Opts.row_wise) {
               static constexpr auto frozen_map = detail::make_map<T, Opts.allow_hash_check>();

               bool first = true;

               while (it != end) {
                  if (first) {
                     first = false;
                  }
                  else {
                     ++it;
                  }

                  auto start = it;
                  goto_delim<','>(it, end);
                  sv key{start, static_cast<size_t>(it - start)};
                  
                  const auto brace_pos = key.find('[');
                  if (brace_pos != std::string_view::npos) {
                     const auto close_brace = key.find(']');
                     auto index = key.substr(brace_pos + 1, close_brace - (brace_pos + 1));
                     key = key.substr(0, brace_pos);
                     uint32_t i;
                     const auto [ptr, ec] = std::from_chars(index.data(), index.data() + index.size(), i);
                     if (ec != std::errc()) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     ctx.csv_index = i;
                  }

                  while (*it != '\n' && it != end) {
                     ++it;
                     
                     const auto& member_it = frozen_map.find(key);
                     if (member_it != frozen_map.end()) [[likely]] {
                        std::visit(
                           [&](auto&& member_ptr) {
                              read<csv>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
                           },
                           member_it->second);
                     }
                  }
               }
            }
            else // column wise
            {
               std::vector<sv> keys;
               
               // array like keys are only read once, because they will handle multiple values at once
               auto read_key = [&](auto&& start, auto&& it) {
                  sv key{ start, size_t(it - start) };
                  const auto brace_pos = key.find('[');
                  if (brace_pos != std::string_view::npos) {
                     key = key.substr(0, brace_pos);
                  }
                  if (keys.size() && (keys.back() != key)) {
                     keys.emplace_back(key);
                  }
                  else if (keys.empty()) {
                     keys.emplace_back(key);
                  }
               };
               
               auto start = it;
               while (it != end) {
                  if (*it == ',') {
                     read_key(start, it);
                     ++it;
                     start = it;
                  }
                  else if (*it == '\n') {
                     if (start == it) {
                        // trailing comma or empty
                     }
                     else {
                        read_key(start, it);
                     }
                     break;
                  }
                  else {
                     ++it;
                  }
               }
               
               const auto n_keys = keys.size();
               
               static constexpr auto frozen_map = detail::make_map<T, Opts.allow_hash_check>();

               while (it != end) {
                  ++it; // skip new line
                  for (size_t i = 0; i < n_keys; ++i) {
                     if (*it == ',') {
                        ++it;
                     }
                     const auto& member_it = frozen_map.find(keys[i]);
                     if (member_it != frozen_map.end()) [[likely]] {
                        std::visit(
                           [&](auto&& member_ptr) {
                              read<csv>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
                           }, member_it->second);
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                  }
               }
            }
         }
      };
   }

   template <bool RowWise = true, class T, class Buffer>
   inline auto read_csv(T&& value, Buffer&& buffer) noexcept
   {
      return read<opts{.format = csv, .row_wise = RowWise}>(value, std::forward<Buffer>(buffer));
   }

   template <bool RowWise = true, class T, class Buffer>
   inline auto read_csv(Buffer&& buffer) noexcept
   {
      T value{};
      read<opts{.format = csv, .row_wise = RowWise}>(value, std::forward<Buffer>(buffer));
      return value;
   }

   template <bool RowWise = true, class T>
   inline parse_error read_file_csv(T& value, const sv file_name)
   {
      context ctx{};
      ctx.current_file = file_name;

      std::string buffer;
      const auto ec = file_to_buffer(buffer, ctx.current_file);
      
      if (static_cast<bool>(ec)) {
         return {ec};
      }

      return read<opts{.format = csv, .row_wise = RowWise}>(value, buffer, ctx);
   }
}
