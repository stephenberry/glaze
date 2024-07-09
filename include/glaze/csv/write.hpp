// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"

namespace glz
{
   namespace detail
   {
      template <>
      struct write<csv>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            to_csv<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                       std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <glaze_value_t T>
      struct to_csv<T>
      {
         template <auto Opts, is_context Ctx, class B, class IX>
         static void op(auto&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
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

      template <writable_array_t T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            if constexpr (resizable<T>) {
               if constexpr (Opts.layout == rowwise) {
                  const auto n = value.size();
                  for (size_t i = 0; i < n; ++i) {
                     write<csv>::op<Opts>(value[i], ctx, b, ix);

                     if (i != (n - 1)) {
                        dump<','>(b, ix);
                     }
                  }
               }
               else {
                  static_assert(false_v<T>, "Dynamic arrays within dynamic arrays are unsupported");
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

      template <class T>
         requires str_t<T> || char_t<T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&&, B&& b, auto&& ix) noexcept
         {
            dump_maybe_empty(value, b, ix);
         }
      };

      template <writable_map_t T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            if constexpr (Opts.layout == rowwise) {
               for (auto& [name, data] : value) {
                  dump_maybe_empty(name, b, ix);
                  dump<','>(b, ix);
                  const auto n = data.size();
                  for (size_t i = 0; i < n; ++i) {
                     write<csv>::op<Opts>(data[i], ctx, b, ix);
                     if (i < n - 1) {
                        dump<','>(b, ix);
                     }
                  }
                  dump<'\n'>(b, ix);
               }
            }
            else {
               // dump titles
               const auto n = value.size();
               size_t i = 0;
               for (auto& [name, data] : value) {
                  dump_maybe_empty(name, b, ix);
                  ++i;
                  if (i < n) {
                     dump<','>(b, ix);
                  }
               }

               dump<'\n'>(b, ix);

               size_t row = 0;
               bool end = false;
               while (true) {
                  i = 0;
                  for (auto& [name, data] : value) {
                     if (row >= data.size()) {
                        end = true;
                        break;
                     }

                     write<csv>::op<Opts>(data[row], ctx, b, ix);
                     ++i;
                     if (i < n) {
                        dump<','>(b, ix);
                     }
                  }

                  if (end) {
                     break;
                  }

                  dump<'\n'>(b, ix);

                  ++row;
               }
            }
         }
      };

      template <class T>
         requires(glaze_object_t<T> || reflectable<T>)
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            static constexpr auto N = refl<T>.N;

            [[maybe_unused]] decltype(auto) t = [&] {
               if constexpr (reflectable<T>) {
                  return to_tuple(value);
               }
               else {
                  return nullptr;
               }
            }();

            if constexpr (Opts.layout == rowwise) {
               for_each<N>([&](auto I) {
                  using value_type = typename std::decay_t<refl_t<T, I>>::value_type;

                  static constexpr sv key = refl<T>.keys[I];

                  decltype(auto) mem = [&]() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return get<I>(t);
                     }
                     else {
                        return get<I>(refl<T>.values);
                     }
                  }();

                  if constexpr (writable_array_t<value_type>) {
                     decltype(auto) member = get_member(value, mem);
                     const auto count = member.size();
                     const auto size = member[0].size();
                     for (size_t i = 0; i < size; ++i) {
                        dump<key>(b, ix);
                        dump<'['>(b, ix);
                        write_chars::op<Opts>(i, ctx, b, ix);
                        dump<']'>(b, ix);
                        dump<','>(b, ix);

                        for (size_t j = 0; j < count; ++j) {
                           write<csv>::op<Opts>(member[j][i], ctx, b, ix);
                           if (j != count - 1) {
                              dump<','>(b, ix);
                           }
                        }

                        if (i != size - 1) {
                           dump<'\n'>(b, ix);
                        }
                     }
                  }
                  else {
                     dump<key>(b, ix);
                     dump<','>(b, ix);
                     write<csv>::op<Opts>(get_member(value, mem), ctx, b, ix);
                     dump<'\n'>(b, ix);
                  }
               });
            }
            else {
               // write titles
               for_each<N>([&](auto I) {
                  using X = refl_t<T, I>;

                  static constexpr sv key = refl<T>.keys[I];

                  decltype(auto) member = [&]() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return get<I>(t);
                     }
                     else {
                        return get<I>(refl<T>.values);
                     }
                  }();

                  if constexpr (fixed_array_value_t<X>) {
                     const auto size = get_member(value, member)[0].size();
                     for (size_t i = 0; i < size; ++i) {
                        dump<key>(b, ix);
                        dump<'['>(b, ix);
                        write_chars::op<Opts>(i, ctx, b, ix);
                        dump<']'>(b, ix);
                        if (i != size - 1) {
                           dump<','>(b, ix);
                        }
                     }
                  }
                  else {
                     write<csv>::op<Opts>(key, ctx, b, ix);
                  }

                  if (I != N - 1) {
                     dump<','>(b, ix);
                  }
               });

               dump<'\n'>(b, ix);

               size_t row = 0;
               bool end = false;

               while (true) {
                  for_each<N>([&](auto I) {
                     using X = std::decay_t<refl_t<T, I>>;

                     decltype(auto) mem = [&]() -> decltype(auto) {
                        if constexpr (reflectable<T>) {
                           return get<I>(t);
                        }
                        else {
                           return get<I>(refl<T>.values);
                        }
                     }();

                     if constexpr (fixed_array_value_t<X>) {
                        decltype(auto) member = get_member(value, mem);
                        if (row >= member.size()) {
                           end = true;
                           return;
                        }

                        const auto n = member[0].size();
                        for (size_t i = 0; i < n; ++i) {
                           write<csv>::op<Opts>(member[row][i], ctx, b, ix);
                           if (i != n - 1) {
                              dump<','>(b, ix);
                           }
                        }
                     }
                     else {
                        decltype(auto) member = get_member(value, mem);
                        if (row >= member.size()) {
                           end = true;
                           return;
                        }

                        write<csv>::op<Opts>(member[row], ctx, b, ix);

                        if (I != N - 1) {
                           dump<','>(b, ix);
                        }
                     }
                  });

                  if (end) {
                     break;
                  }

                  ++row;

                  dump<'\n'>(b, ix);
               }
            }
         }
      };
   }

   template <uint32_t layout = rowwise, write_csv_supported T, class Buffer>
   [[nodiscard]] auto write_csv(T&& value, Buffer&& buffer) noexcept
   {
      return write<opts{.format = csv, .layout = layout}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <uint32_t layout = rowwise, write_csv_supported T>
   [[nodiscard]] expected<std::string, error_ctx> write_csv(T&& value) noexcept
   {
      return write<opts{.format = csv, .layout = layout}>(std::forward<T>(value));
   }

   template <uint32_t layout = rowwise, write_csv_supported T>
   [[nodiscard]] error_ctx write_file_csv(T&& value, const std::string& file_name, auto&& buffer) noexcept
   {
      const auto ec = write<opts{.format = csv, .layout = layout}>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec;
      }
      return {buffer_to_file(buffer, file_name)};
   }
}
