// Distributed under the MIT license
// Developed by Anyar Inc.

#pragma once

#include <fstream>
#include <sstream>

#include "fmt/format.h"
#include "fmt/compile.h"

#include "glaze/record/recorder.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/core/common.hpp"
#include "glaze/core/format.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/write.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/error.hpp"
#include "glaze/core/write_chars.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct to_csv {};
      
      template <>
      struct write<csv>
      {
         template <auto Opts, class T,  is_context Ctx, class B>
         static void op(T&& value, Ctx&& ctx, B&& b) {
            to_csv<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b));
         }
         
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) {
            to_csv<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };
      
      template <class T>
      requires str_t<T> || char_t<T>
      struct to_csv<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&&, Args&&... args) noexcept
         {
            dump(value, std::forward<Args>(args)...);
         }
      };
      
      template <num_t T>
      struct to_csv<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b) noexcept
         {
            write_chars::op<Opts>(value, ctx, b);
         }
         
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            write_chars::op<Opts>(value, ctx, b, ix);
         }
      };
      
      template <boolean_like T>
      struct to_csv<T>
      {         
         template <auto Opts, class... Args>
         static void op(const bool value, is_context auto&&, Args&&... args) noexcept
         {
            if (value) {
               dump<"1">(std::forward<Args>(args)...);
            }
            else {
               dump<"0">(std::forward<Args>(args)...);
            }
         }
      };
      
      template <is_std_tuple T>
      struct to_csv<T>
      {
         template <auto Opts, class... Args>
         static void op(T&& tuple, is_context auto&& ctx, Args&&... args) noexcept
         {
            const auto tuples = tuple_split(std::forward<T>(tuple));
            static constexpr auto N = std::tuple_size_v<T> / 2;
            
            const auto& data = std::get<1>(tuples);
            auto n = std::get<0>(data).size();
            
            // output minimum dimension
            for_each<N>([&](auto I) {
               const auto m = std::get<I>(data).size();
               n = m < n ? m : n;
            });
            
            // write titles
            const auto& titles = std::get<0>(tuples);
            if constexpr (Opts.rowwise) {
               for_each<N>([&](auto I) {
                  write<csv>::op<Opts>(std::get<I>(titles), ctx, args...);
                  dump<",">(args...);
                  
                  const auto& x = std::get<I>(data);
                  for (size_t i = 0; i < n; ++i) {
                     write<csv>::op<Opts>(x[i], ctx, args...);
                     dump<",">(args...);
                  }
                  dump<"\n">(args...);
               });
            }
            else {
               for_each<N>([&](auto I) {
                  write<csv>::op<Opts>(std::get<I>(titles), ctx, args...);
                  if constexpr (I != N - 1) {
                     dump<",">(args...);
                  }
               });
               dump<"\n">(args...);
               
               // write out columns of data
               for (size_t i = 0; i < n; ++i) {
                  for_each<N>([&](auto I) {
                     write<csv>::op<Opts>(std::get<I>(data)[i], ctx, args...);
                     if constexpr (I != N - 1) {
                        dump<",">(args...);
                     }
                  });
                  dump<"\n">(args...);
               }
            }
         }
      };
      
      template <class Map>
      concept is_map = std::same_as<typename Map::value_type, std::pair<const typename Map::key_type, typename Map::mapped_type>>;
      
      template <class T>
      requires is_map<std::decay_t<T>>
      struct to_csv<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& map, is_context auto&& ctx, Args&&... args) noexcept
         {
            const auto N = map.size();

            // write out minimum dimensions
            size_t n = std::numeric_limits<size_t>::max();
            for (auto& [title, data] : map) {
               n = data.size() < n ? data.size() : n;
            }

            if constexpr (Opts.rowwise) {
               for (auto& [title, data] : map) {
                  write<csv>::op<Opts>(title, ctx, args...);
                  dump<",">(args...);

                  for (size_t i = 0; i < n; ++i) {
                     write<csv>::op<Opts>(data[i], ctx, args...);
                     dump<",">(args...);
                  }
                  dump<"\n">(args...);
               }
            }
            else {
               size_t I = 0;
               for (auto& [title, data] : map) {
                  write<csv>::op<Opts>(title, ctx, args...);
                  if (I != N - 1) {
                     dump<",">(args...);
                  }
                  ++I;
               }
               dump<"\n">(args...);

               // write out columns of data
               for (size_t i = 0; i < n; ++i) {
                  I = 0;
                  for (auto& [title, data] : map) {
                     write<csv>::op<Opts>(data[i], ctx, args...);
                     if (I != N - 1) {
                        dump<",">(args...);
                     }
                     ++I;
                  }
                  dump<"\n">(args...);
               }
            }
         }
      };
      
      template <class T>
      requires is_specialization_v<std::decay_t<T>, recorder>
      struct to_csv<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& rec, is_context auto&& ctx, Args&&... args) noexcept
         {
            auto& map = rec.data;

            const auto N = map.size();
            
            // write out minimum dimensions
            size_t n = std::numeric_limits<size_t>::max();
            for (auto& [title, data] : map) {
               const auto m = variant_container_size(data.first);
               n = m < n ? m : n;
            }

            if constexpr (Opts.rowwise) {
                for (auto& [title, data] : map) {
                   write<csv>::op<Opts>(title, ctx, args...);
                   dump<",">(args...);

                    std::visit([&](auto&& arg) {
                       for (size_t i = 0; i < n; ++i) {
                          write<csv>::op<Opts>(arg[i], ctx, args...);
                          dump<",">(args...);
                       }
                    }, data.first);
                   dump<"\n">(args...);
                }
            }
            else {
                size_t I = 0;
                for (auto& [title, data] : map) {
                   write<csv>::op<Opts>(title, ctx, args...);
                    if (I != N - 1) {
                       dump<",">(args...);
                    }
                    ++I;
                }
               dump<"\n">(args...);

                const auto n = map.size();
                for (size_t i = 0; i < n; ++i) {
                    I = 0;
                    for (auto& [title, data] : map) {
                       std::visit([&](auto&& arg) { write<csv>::op<Opts>(arg[i], ctx, args...); }, data.first);
                        if (I != N - 1) {
                           dump<",">(args...);
                        }
                        ++I;
                    }
                   dump<"\n">(args...);
                }
            }
         }
      };
   }
   
   template <bool RowWise = true, class Buffer, class... Args> requires (sizeof...(Args) > 1)
     inline void write_csv(Buffer& buffer, Args&&... args)
   {
      return write<opts{.format = csv, .rowwise = RowWise}>(std::make_tuple(std::forward<Args>(args)...), buffer);
   }
   
   template <bool RowWise = true, class Buffer, class T>
     inline void write_csv(Buffer& buffer, T&& value)
   {
      return write<opts{.format = csv, .rowwise = RowWise}>(std::forward<T>(value), buffer);
   }
   
   template <bool RowWise = true, class Buffer, class... Ts>
     inline void write_csv(Buffer& buffer, recorder<Ts...>& rec)
   {
      return write<opts{.format = csv, .rowwise = RowWise}>(rec, buffer);
   }
   
   template <bool RowWise = true, class... Args>
   inline void write_file_csv(const std::string_view file_name, Args&&... args)
   {
      std::string buffer;
      write_csv<RowWise>(buffer, std::forward<Args>(args)...);
      
      std::ios_base::sync_with_stdio(false);
      std::fstream file(std::string{file_name}, std::ios::out);
      
      if (!file) {
         throw std::runtime_error(fmt::format("csv | file '{}' could not be created", file_name));
      }
      
      file.write(buffer.data(), buffer.size());
   }
}


