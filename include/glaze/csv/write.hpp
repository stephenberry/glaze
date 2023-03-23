// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/dump.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct to_csv {};

      template <>
      struct write<csv>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            to_csv<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                       std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <glaze_value_t T>
      struct to_csv<T>
      {
         template <auto Opts, is_context Ctx, class B, class IX>
         static void op(auto&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            to_csv<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx),
                                         std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <num_t T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            write_chars::op<Opts>(value, ctx, b, ix);
         }
      };

      template <bool_t T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&&, B&& b, auto&& ix) noexcept
         {
            if (value) {
               dump<'1'>(b, ix);
            }
            else {
               dump<'0'>(b, ix);
            }

         }
      };

      template <array_t T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            if constexpr (resizeable<T>) {
               if constexpr (Opts.row_wise) {
                  const auto n = value.size();
                  for (size_t i = 0; i < n; ++i) {
                     write<csv>::op<Opts>(value[i], ctx, b, ix);

                     if (i != (n - 1)) {
                        dump<','>(b, ix);
                     }
                  }
               }
               else {
                  write<csv>::op<Opts>(value[ctx.csv_index], ctx, b, ix);
               }
            }
            else {
               const auto n = value.size();
               for (size_t i = 0; i < n; ++i) {
                  write<csv>::op<Opts>(value[i], ctx, b, ix);

                  if (i != (n - 1)) {
                     dump<','>(b, ix);
                  }
               }
            }
         }
      };

      template <class T> requires str_t<T> || char_t<T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&&, B&& b, auto&& ix) noexcept
         {
            dump(value, b, ix);
         }
      };
      
      template <glaze_object_t T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;

            if constexpr (Opts.row_wise) {
               for_each<N>([&](auto I) {
                  static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
                  static constexpr sv key = glz::tuplet::get<0>(item);

                  using item_type = std::decay_t<decltype(get_member(value, glz::tuplet::get<1>(item)))>;
                  using V_t = typename item_type::value_type;

                  if constexpr (array_t<V_t>) {

                     auto member = get_member(value, glz::tuplet::get<1>(item));
                     const auto count = member.size();
                     const auto size = member[0].size();
                     for (size_t i = 0; i < size; ++i) {
                        std::string idx_key(key);
                        idx_key += '[' + std::to_string(i) + ']';
                        write<csv>::op<Opts>(idx_key, ctx, b, ix);
                        dump<','>(b, ix);

                        for (size_t j = 0; j < count; ++j) {
                           write<csv>::op<Opts>(member[j][i], ctx, b, ix);
                           if (j != count - 1) {
                              dump<','>(b, ix);
                           }
                        }

                        if (i != size - 1)
                           dump<'\n'>(b, ix);
                     }
                  }
                  else {
                     write<csv>::op<Opts>(key, ctx, b, ix);
                     dump<','>(b, ix);
                     write<csv>::op<Opts>(get_member(value, glz::tuplet::get<1>(item)), ctx, b, ix);
                     dump<'\n'>(b, ix);
                  }
               });
            }
            else {
               int row_count = -1;
               bool first = true;

               for_each<N>([&](auto I) {
                  static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
                  static constexpr sv key = glz::tuplet::get<0>(item);

                  using item_type = std::decay_t<decltype(get_member(value, glz::tuplet::get<1>(item)))>;

                  if (first) {
                     first = false;
                     if constexpr (array_t<item_type>) {
                        row_count = int(get_member(value, glz::tuplet::get<1>(item)).size());
                     }
                  }
                  else {
                     dump<','>(b, ix);

                     if constexpr (array_t<item_type>) {
                        const auto size = get_member(value, glz::tuplet::get<1>(item)).size();
                        row_count = (int(size) < row_count) ? int(size) : row_count;
                     }
                  }

                  using V_t = typename item_type::value_type;

                  if constexpr (array_t<V_t> && !resizeable<V_t>) {
                     const auto size = get_member(value, glz::tuplet::get<1>(item))[0].size();
                     for (size_t i = 0; i < size; ++i) {
                        std::string idx_key(key);
                        idx_key += '[' + std::to_string(i) + ']';
                        write<csv>::op<Opts>(idx_key, ctx, b, ix);
                        if (i != size - 1)
                           dump<','>(b, ix);
                     }
                  }
                  else {
                     write<csv>::op<Opts>(key, ctx, b, ix);
                  }

                  
               });

               dump <'\n'>(b, ix);

               for (; int(ctx.csv_index) < row_count; ++ctx.csv_index) {
                  first = true;

                  for_each<N>([&](auto I) {
                     static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);

                     if (first) {
                        first = false;
                     }
                     else {
                        dump<','>(b, ix);
                     }

                     write<csv>::op<Opts>(get_member(value, glz::tuplet::get<1>(item)), ctx, b, ix);
                  });

                  dump<'\n'>(b, ix);
               }
            }
         }
      };
   }

   template <bool RowWise = true, class T>
   [[nodiscard]] inline write_error write_file_csv(T&& value, const std::string& file_name)
   {
      std::string buffer{};
      write<opts{.format = csv, .row_wise = RowWise}>(std::forward<T>(value), buffer);
      return {buffer_to_file(buffer, file_name)};
   }
}
