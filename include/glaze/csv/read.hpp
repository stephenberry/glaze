// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
//#include "glaze/core/write.hpp"
#include "glaze/core/format.hpp"
#include "glaze/util/strod.hpp"
//#include "glaze/core/opts.hpp"
//#include "glaze/util/type_traits.hpp"
//#include "glaze/util/parse.hpp"
//#include "glaze/util/for_each.hpp"
#include "glaze/file/file_ops.hpp"

#include <charconv>

// Taken from https://stackoverflow.com/questions/48012539/idiomatically-split-a-string-view
inline std::vector<std::string_view> Split(const std::string_view str, const char delim = ',')
{
   std::vector<std::string_view> result;

   int indexCommaToLeftOfColumn = 0;
   int indexCommaToRightOfColumn = -1;

   for (int i = 0; i < static_cast<int>(str.size()); i++) {
      if (str[i] == delim) {
         indexCommaToLeftOfColumn = indexCommaToRightOfColumn;
         indexCommaToRightOfColumn = i;
         int index = indexCommaToLeftOfColumn + 1;
         int length = indexCommaToRightOfColumn - index;

         std::string_view column(str.data() + index, length);
         result.push_back(column);
      }
   }
   const std::string_view finalColumn(str.data() + indexCommaToRightOfColumn + 1,
                                      str.size() - indexCommaToRightOfColumn - 1);
   result.push_back(finalColumn);
   return result;
}

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
                  size_t size = value.size();
                  for (size_t i = 0; i < size; ++i) {
                     typename T::value_type element;
                     read<csv>::op<Opts>(element, ctx, it, end);
                     value[i] = element;
                     if (i != size - 1) {
                        ++it;
                     }
                  }
               }
               
            }
         }
      };

      inline void find_newline(auto&& it, auto&& end) noexcept
      {
         while (++it != end && *it != '\n')
            ;
      }

      inline void goto_delim(auto&& it, auto&& end, char delim) noexcept
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
            std::string_view file = sv{it, size_t(std::distance(it, end))};
            auto lines = Split(file, '\n');

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
                  goto_delim(it, end, ',');
                  std::string_view key = sv{start, static_cast<size_t>(it - start)};

                  while (*it != '\n' && it != end) {
                     ++it;

                     auto brace_pos = key.find('[');
                     if (brace_pos != std::string_view::npos) {
                        auto key_name = key.substr(0, brace_pos);

                        auto close_brace = key.find(']');
                        auto index = key.substr(brace_pos + 1, close_brace - (brace_pos + 1));
                        int i;
                        auto [ptr, ec] = std::from_chars(index.data(), index.data() + index.size(), i);
                        if (ec == std::errc()) {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                        ctx.csv_index = i;
                        const auto& member_it = frozen_map.find(key_name);
                        if (member_it != frozen_map.end()) [[likely]] {
                           std::visit(
                              [&](auto&& member_ptr) {
                                 read<csv>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
                              },
                              member_it->second);
                        }
                     }
                     else {
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
            }
            else {
               auto start = it;
               goto_delim(it, end, '\n');
               std::string_view key_line = sv{start, static_cast<size_t>(it - start)};
               //++it;  // pass by the new line

               auto keys = Split(key_line);

               while (it != end) {
                  for (size_t i = 0; i < keys.size(); ++i) {
                     ++it;

                     auto key = keys[i];

                     size_t brace_pos = key.find('[');
                     if (brace_pos != std::string::npos) {
                        key = key.substr(0, brace_pos);
                        for (size_t j = i + 1; j < keys.size(); ++j) {
                           if (keys[j].find(key) != std::string_view::npos) {
                              i++;
                           }
                        }
                     }

                     static constexpr auto frozen_map = detail::make_map<T, Opts.allow_hash_check>();
                     const auto& member_it = frozen_map.find(key);
                     if (member_it != frozen_map.end()) [[likely]] {
                        std::visit(
                           [&](auto&& member_ptr) { read<csv>::op<Opts>(get_member(value, member_ptr), ctx, it, end); },
                           member_it->second);
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
