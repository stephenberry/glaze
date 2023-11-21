// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>

#include "glaze/core/format.hpp"
#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/strod.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct from_csv
      {};

      template <>
      struct read<csv>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         static void op(T&& value, Ctx&& ctx, It0&& it, It1 end) noexcept
         {
            from_csv<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                         std::forward<It0>(it), std::forward<It1>(end));
         }
      };

      template <glaze_value_t T>
      struct from_csv<T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            from_csv<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx),
                                           std::forward<It0>(it), std::forward<It1>(end));
         }
      };

      template <num_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&&) noexcept
         {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            using V = std::decay_t<decltype(value)>;
            if constexpr (int_t<V>) {
               if constexpr (std::is_unsigned_v<V>) {
                  uint64_t i{};
                  if (*it == '-') [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
                  auto e = stoui64<V>(i, it);
                  if (!e) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }

                  if (i > std::numeric_limits<V>::max()) [[unlikely]] {
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
                  }
                  auto e = stoui64<V>(i, it);
                  if (!e) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }

                  if (i > std::numeric_limits<V>::max()) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
                  value = sign * static_cast<V>(i);
               }
            }
            else {
               auto s = parse_float<V, Opts.force_conformance>(value, it);
               if (!s) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
            }
         }
      };

      template <string_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
         {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            value.clear();
            auto start = it;
            while (it < end) {
               switch (*it) {
               case ',':
               case '\n': {
                  value.append(start, static_cast<size_t>(it - start));
                  return;
               }
               case '\\':
               case '\b':
               case '\f':
               case '\r':
               case '\t': {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               case '\0': {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               default:
                  ++it;
               }
            }

            value.append(start, static_cast<size_t>(it - start));
         }
      };

      template <bool_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&&) noexcept
         {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            uint64_t temp;
            auto s = stoui64(temp, it);
            if (!s) [[unlikely]] {
               ctx.error = error_code::expected_true_or_false;
               return;
            }
            value = static_cast<bool>(temp);
         }
      };

      template <readable_array_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
         {
            read<csv>::op<Opts>(value.emplace_back(), ctx, it, end);
         }
      };

      template <char delim>
      inline void goto_delim(auto&& it, auto&& end) noexcept
      {
         while (++it != end && *it != delim)
            ;
      }

      inline auto read_column_wise_keys(auto&& ctx, auto&& it, auto&& end)
      {
         std::vector<std::pair<sv, size_t>> keys;

         auto read_key = [&](auto&& start, auto&& it) {
            sv key{start, size_t(it - start)};

            size_t csv_index{};

            const auto brace_pos = key.find('[');
            if (brace_pos != sv::npos) {
               const auto close_brace = key.find(']');
               const auto index = key.substr(brace_pos + 1, close_brace - (brace_pos + 1));
               key = key.substr(0, brace_pos);
               const auto [ptr, ec] = std::from_chars(index.data(), index.data() + index.size(), csv_index);
               if (ec != std::errc()) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            keys.emplace_back(std::pair{key, csv_index});
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

         return keys;
      }

      template <readable_map_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
         {
            if constexpr (Opts.layout == rowwise) {
               while (it != end) {
                  auto start = it;
                  goto_delim<','>(it, end);
                  sv key{start, static_cast<size_t>(it - start)};

                  size_t csv_index;

                  const auto brace_pos = key.find('[');
                  if (brace_pos != sv::npos) {
                     const auto close_brace = key.find(']');
                     const auto index = key.substr(brace_pos + 1, close_brace - (brace_pos + 1));
                     key = key.substr(0, brace_pos);
                     const auto [ptr, ec] = std::from_chars(index.data(), index.data() + index.size(), csv_index);
                     if (ec != std::errc()) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }

                  match<','>(ctx, it, end);

                  using key_type = typename std::decay_t<decltype(value)>::key_type;
                  auto& member = value[key_type(key)];
                  using M = std::decay_t<decltype(member)>;
                  if constexpr (fixed_array_value_t<M> && emplace_backable<M>) {
                     size_t col = 0;
                     while (it != end) {
                        if (col < member.size()) [[likely]] {
                           read<csv>::op<Opts>(member[col][csv_index], ctx, it, end);
                        }
                        else [[unlikely]] {
                           read<csv>::op<Opts>(member.emplace_back()[csv_index], ctx, it, end);
                        }

                        if (*it == '\n') {
                           ++it;
                           break;
                        }

                        if (*it == ',') {
                           ++it;
                        }
                        else {
                           ctx.error = error_code::syntax_error;
                           return;
                        }

                        ++col;
                     }
                  }
                  else {
                     while (it != end) {
                        read<csv>::op<Opts>(member, ctx, it, end);

                        if (*it == '\n') {
                           ++it;
                           break;
                        }

                        if (*it == ',') {
                           ++it;
                        }
                        else {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                  }
               }
            }
            else // column wise
            {
               const auto keys = read_column_wise_keys(ctx, it, end);

               if (bool(ctx.error)) {
                  return;
               }

               if (*it == '\n') {
                  ++it; // skip new line
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               const auto n_keys = keys.size();

               size_t row = 0;

               while (it != end) {
                  for (size_t i = 0; i < n_keys; ++i) {
                     using key_type = typename std::decay_t<decltype(value)>::key_type;
                     auto& member = value[key_type(keys[i].first)];
                     using M = std::decay_t<decltype(member)>;
                     if constexpr (fixed_array_value_t<M> && emplace_backable<M>) {
                        const auto index = keys[i].second;
                        if (row < member.size()) [[likely]] {
                           read<csv>::op<Opts>(member[row][index], ctx, it, end);
                        }
                        else [[unlikely]] {
                           read<csv>::op<Opts>(member.emplace_back()[index], ctx, it, end);
                        }
                     }
                     else {
                        read<csv>::op<Opts>(member, ctx, it, end);
                     }

                     if (*it == ',') {
                        ++it;
                     }
                  }

                  if (*it == '\n') {
                     ++it; // skip new line
                     ++row;
                  }
               }
            }
         }
      };

      template <glaze_object_t T>
      struct from_csv<T>
      {
         template <auto Opts, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
         {
            static constexpr auto frozen_map = detail::make_map<T, Opts.use_hash_comparison>();
            // static constexpr auto N = std::tuple_size_v<meta_t<T>>;

            if constexpr (Opts.layout == rowwise) {
               while (it != end) {
                  auto start = it;
                  goto_delim<','>(it, end);
                  sv key{start, static_cast<size_t>(it - start)};

                  size_t csv_index;

                  const auto brace_pos = key.find('[');
                  if (brace_pos != sv::npos) {
                     const auto close_brace = key.find(']');
                     const auto index = key.substr(brace_pos + 1, close_brace - (brace_pos + 1));
                     key = key.substr(0, brace_pos);
                     const auto [ptr, ec] = std::from_chars(index.data(), index.data() + index.size(), csv_index);
                     if (ec != std::errc()) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }

                  match<','>(ctx, it, end);

                  const auto& member_it = frozen_map.find(key);

                  if (member_it != frozen_map.end()) [[likely]] {
                     std::visit(
                        [&](auto&& member_ptr) {
                           auto&& member = get_member(value, member_ptr);
                           using M = std::decay_t<decltype(member)>;
                           if constexpr (fixed_array_value_t<M> && emplace_backable<M>) {
                              size_t col = 0;
                              while (it != end) {
                                 if (col < member.size()) [[likely]] {
                                    read<csv>::op<Opts>(member[col][csv_index], ctx, it, end);
                                 }
                                 else [[unlikely]] {
                                    read<csv>::op<Opts>(member.emplace_back()[csv_index], ctx, it, end);
                                 }

                                 if (*it == '\n') {
                                    ++it;
                                    break;
                                 }

                                 if (*it == ',') {
                                    ++it;
                                 }
                                 else {
                                    ctx.error = error_code::syntax_error;
                                    return;
                                 }

                                 ++col;
                              }
                           }
                           else {
                              while (it != end) {
                                 read<csv>::op<Opts>(member, ctx, it, end);

                                 if (*it == '\n') {
                                    ++it;
                                    break;
                                 }

                                 if (*it == ',') {
                                    ++it;
                                 }
                                 else {
                                    ctx.error = error_code::syntax_error;
                                    return;
                                 }
                              }
                           }
                        },
                        member_it->second);
                  }
                  else [[unlikely]] {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
               }
            }
            else // column wise
            {
               const auto keys = read_column_wise_keys(ctx, it, end);

               if (bool(ctx.error)) {
                  return;
               }

               if (*it == '\n') {
                  ++it; // skip new line
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               const auto n_keys = keys.size();

               size_t row = 0;

               while (it != end) {
                  for (size_t i = 0; i < n_keys; ++i) {
                     const auto& member_it = frozen_map.find(keys[i].first);
                     if (member_it != frozen_map.end()) [[likely]] {
                        std::visit(
                           [&](auto&& member_ptr) {
                              auto&& member = get_member(value, member_ptr);
                              using M = std::decay_t<decltype(member)>;
                              if constexpr (fixed_array_value_t<M> && emplace_backable<M>) {
                                 const auto index = keys[i].second;
                                 if (row < member.size()) [[likely]] {
                                    read<csv>::op<Opts>(member[row][index], ctx, it, end);
                                 }
                                 else [[unlikely]] {
                                    read<csv>::op<Opts>(member.emplace_back()[index], ctx, it, end);
                                 }
                              }
                              else {
                                 read<csv>::op<Opts>(member, ctx, it, end);
                              }
                           },
                           member_it->second);
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::unknown_key;
                        return;
                     }

                     if (*it == ',') {
                        ++it;
                     }
                  }

                  if (*it == '\n') {
                     ++it; // skip new line
                     ++row;
                  }
               }
            }
         }
      };
   }

   template <uint32_t layout = rowwise, class T, class Buffer>
   inline auto read_csv(T&& value, Buffer&& buffer) noexcept
   {
      return read<opts{.format = csv, .layout = layout}>(value, std::forward<Buffer>(buffer));
   }

   template <uint32_t layout = rowwise, class T, class Buffer>
   inline auto read_csv(Buffer&& buffer) noexcept
   {
      T value{};
      read<opts{.format = csv, .layout = rowwise}>(value, std::forward<Buffer>(buffer));
      return value;
   }

   template <uint32_t layout = rowwise, class T>
   inline parse_error read_file_csv(T& value, const sv file_name)
   {
      context ctx{};
      ctx.current_file = file_name;

      std::string buffer;
      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {ec};
      }

      return read<opts{.format = csv, .layout = rowwise}>(value, buffer, ctx);
   }
}
